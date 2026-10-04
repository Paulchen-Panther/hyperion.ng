/*
 * sand_sample.h - Edge-band sampler for Broadcom SAND128 frames (NV12 / P030)
 *
 * Reads only the border bands of a column-tiled (SAND128) video frame, using
 * a few large contiguous copies into a cacheable scratch buffer, and reduces
 * them to per-LED-cell mean colours. No DRM dependency: works on any mapped
 * buffer, which makes it unit-testable on a PC (see test_sand_sample.c).
 *
 * Layout (see drm_fourcc.h, DRM_FORMAT_MOD_BROADCOM_SAND128, and igt's
 * vc4_sand_tiled_offset()):
 *   - The frame is split into columns that are 128 bytes wide.
 *   - Inside a column, lines are consecutive: line y is at y*128.
 *   - Column c starts at c * col_height * 128 (col_height = modifier param).
 *   NV12: 128 px per column (1 byte/px luma, Cb/Cr byte pairs for chroma).
 *   P030: 96 px per column, 3 x 10 bit packed in a 32 bit word (luma) /
 *         3 Cb/Cr pairs in a 64 bit word (chroma), Cb in the low bits.
 *
 * Chroma placement is the one thing that differs between producers:
 *   INTERLEAVED: UV lines follow the Y lines inside every column; chroma
 *                column stride == luma column stride (drm_fourcc.h note).
 *   SEPARATE:    UV is its own tiled image with half the column height
 *                (reported for rpi-hevc-dec output).
 * AUTO picks SEPARATE if the chroma offset lies beyond the first luma
 * column, else INTERLEAVED. Verify with sand_dump_ppm() on real hardware.
 *
 * All values are normalised to the 10 bit domain (NV12 << 2).
 */
#ifndef SAND_SAMPLE_H
#define SAND_SAMPLE_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAND_COL_BYTES 128u

typedef enum { SAND_NV12 = 0, SAND_P030 = 1 } sand_fmt;
typedef enum {
	SAND_CHROMA_AUTO = 0,
	SAND_CHROMA_INTERLEAVED = 1,
	SAND_CHROMA_SEPARATE = 2
} sand_chroma_mode;
typedef enum { SAND_BT709 = 0, SAND_BT2020 = 1 } sand_matrix;

typedef struct {
	sand_fmt fmt;
	unsigned x0, y0, w, h;   /* sampled rectangle, luma px, all even        */
	unsigned col_height;     /* lines per column (modifier parameter)       */
	size_t off_y, off_c;     /* plane offsets in bytes (fb2->offsets[0/1])  */
	sand_chroma_mode chroma_mode;
} sand_geom;

typedef struct {
	const uint8_t *y;  size_t y_size;
	const uint8_t *c;  size_t c_size;   /* c == NULL: chroma lives in y map */
} sand_maps;

typedef struct {
	unsigned top, bottom, left, right;  /* number of cells per side        */
	unsigned band_px;                   /* band thickness (luma px, even)  */
	unsigned xdecim, ydecim;            /* sample decimation inside bands  */
	sand_matrix matrix;
	int full_range;
} sand_edge_cfg;

typedef struct { uint8_t r, g, b; } sand_rgb;
typedef struct { sand_rgb *top, *bottom, *left, *right; } sand_edge_out;

typedef struct { uint64_t sy, scb, scr; uint32_t ny, nc; } sand_acc;

typedef struct {
	uint8_t *buf;
	size_t cap;
	unsigned col_first, ncols;
	unsigned line0, nlines, step;
} sand_band;

enum { SAND_TOP = 0, SAND_BOTTOM = 1, SAND_LEFT = 2, SAND_RIGHT = 3 };

typedef struct {
	sand_geom g;
	sand_edge_cfg cfg;
	sand_band by[4], bc[4];
	sand_acc *acc[4];
	unsigned n[4];
} sand_sampler;

/* ------------------------------------------------------------------ */
/* geometry helpers                                                    */

static inline unsigned sand_px_per_col(sand_fmt f)
{
	return f == SAND_P030 ? 96u : 128u;
}

static inline sand_chroma_mode sand_resolve_chroma(const sand_geom *g)
{
	if (g->chroma_mode != SAND_CHROMA_AUTO)
		return g->chroma_mode;
	return g->off_c >= (size_t)g->col_height * SAND_COL_BYTES
		       ? SAND_CHROMA_SEPARATE : SAND_CHROMA_INTERLEAVED;
}

static inline size_t sand_col_stride_y(const sand_geom *g)
{
	return (size_t)g->col_height * SAND_COL_BYTES;
}

static inline size_t sand_col_stride_c(const sand_geom *g)
{
	unsigned lines = sand_resolve_chroma(g) == SAND_CHROMA_SEPARATE
				 ? g->col_height / 2 : g->col_height;
	return (size_t)lines * SAND_COL_BYTES;
}

/* ------------------------------------------------------------------ */
/* sample decoding (row = pointer to one 128 byte line of one column)  */

static inline uint32_t sand_y_px(sand_fmt f, const uint8_t *row, unsigned xin)
{
	if (f == SAND_NV12)
		return (uint32_t)row[xin] << 2;
	uint32_t w;
	memcpy(&w, row + (xin / 3) * 4, 4);
	return (w >> (10 * (xin % 3))) & 0x3ff;
}

static inline void sand_c_px(sand_fmt f, const uint8_t *row, unsigned cin,
			     uint32_t *cb, uint32_t *cr)
{
	if (f == SAND_NV12) {
		*cb = (uint32_t)row[2 * cin] << 2;
		*cr = (uint32_t)row[2 * cin + 1] << 2;
		return;
	}
	uint64_t w;
	memcpy(&w, row + (cin / 3) * 8, 8);
	unsigned sh = 20 * (cin % 3);
	*cb = (w >> sh) & 0x3ff;
	*cr = (w >> (sh + 10)) & 0x3ff;
}

static inline unsigned sand_cell_of(unsigned rel, unsigned len, unsigned n)
{
	return (unsigned)(((uint64_t)rel * n) / len);
}

/* ------------------------------------------------------------------ */
/* band reading: few large sequential copies into cacheable scratch    */

static inline const uint8_t *sand_band_row(const sand_band *b, unsigned col,
					   unsigned line)
{
	return b->buf + (((size_t)(col - b->col_first) * b->nlines) +
			 (line - b->line0) / b->step) * SAND_COL_BYTES;
}

/*
 * Copy lines [line0, line_end) (every `step`-th) of columns
 * [col_first, col_last] into the band buffer, laid out [col][line][128].
 * step == 1 -> one memcpy per column (fully contiguous in the source).
 * Returns 0 or -ERANGE if the layout would read outside the mapping.
 */
static inline int sand_band_fill(sand_band *b, const uint8_t *map,
				 size_t map_size, size_t plane_off,
				 size_t col_stride, unsigned col_first,
				 unsigned col_last, unsigned line0,
				 unsigned line_end, unsigned step)
{
	if (!map || line_end <= line0 || col_last < col_first || !step)
		return -EINVAL;

	const unsigned ncols = col_last - col_first + 1;
	const unsigned nlines = (line_end - line0 + step - 1) / step;
	const size_t need = (size_t)ncols * nlines * SAND_COL_BYTES;

	if (need > b->cap) {
		uint8_t *nb = (uint8_t *)realloc(b->buf, need);
		if (!nb)
			return -ENOMEM;
		b->buf = nb;
		b->cap = need;
	}
	b->col_first = col_first; b->ncols = ncols;
	b->line0 = line0; b->nlines = nlines; b->step = step;

	for (unsigned c = 0; c < ncols; c++) {
		const size_t col_base = plane_off + (size_t)(col_first + c) * col_stride;
		const size_t last_end = col_base +
			((size_t)line0 + (size_t)(nlines - 1) * step + 1) * SAND_COL_BYTES;
		if (last_end > map_size)
			return -ERANGE;

		uint8_t *dst = b->buf + (size_t)c * nlines * SAND_COL_BYTES;
		if (step == 1) {
			memcpy(dst, map + col_base + (size_t)line0 * SAND_COL_BYTES,
			       (size_t)nlines * SAND_COL_BYTES);
		} else {
			for (unsigned r = 0; r < nlines; r++)
				memcpy(dst + (size_t)r * SAND_COL_BYTES,
				       map + col_base +
				       ((size_t)line0 + (size_t)r * step) * SAND_COL_BYTES,
				       SAND_COL_BYTES);
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* accumulation                                                        */

static inline void sand_acc_y(sand_acc *a, uint32_t y) { a->sy += y; a->ny++; }
static inline void sand_acc_c(sand_acc *a, uint32_t cb, uint32_t cr)
{
	a->scb += cb; a->scr += cr; a->nc++;
}

/* top (ya = y0) or bottom (ya = y0 + h - B) band; lines stored with step 1 */
static inline void sand_acc_hband(const sand_geom *g, const sand_band *by,
				  const sand_band *bc, unsigned ya, unsigned B,
				  unsigned n, unsigned xd, unsigned yd,
				  sand_acc *acc)
{
	const unsigned ppc = sand_px_per_col(g->fmt), cppc = ppc / 2;
	const unsigned cdx = xd > 1 ? xd / 2 : 1, cdy = yd > 1 ? yd / 2 : 1;

	for (unsigned y = ya; y < ya + B; y += yd)
		for (unsigned x = g->x0; x < g->x0 + g->w; x += xd) {
			const uint8_t *row = sand_band_row(by, x / ppc, y);
			sand_acc_y(&acc[sand_cell_of(x - g->x0, g->w, n)],
				   sand_y_px(g->fmt, row, x % ppc));
		}

	for (unsigned cy = ya / 2; cy < (ya + B) / 2; cy += cdy)
		for (unsigned cx = g->x0 / 2; cx < (g->x0 + g->w) / 2; cx += cdx) {
			const uint8_t *row = sand_band_row(bc, cx / cppc, cy);
			uint32_t cb, cr;
			sand_c_px(g->fmt, row, cx % cppc, &cb, &cr);
			sand_acc_c(&acc[sand_cell_of(2 * cx - g->x0, g->w, n)], cb, cr);
		}
}

/* left (xa = x0) or right (xa = x0 + w - B) band; lines stored with step yd */
static inline void sand_acc_vband(const sand_geom *g, const sand_band *by,
				  const sand_band *bc, unsigned xa, unsigned B,
				  unsigned n, unsigned xd, unsigned yd,
				  sand_acc *acc)
{
	const unsigned ppc = sand_px_per_col(g->fmt), cppc = ppc / 2;
	const unsigned cdx = xd > 1 ? xd / 2 : 1, cdy = yd > 1 ? yd / 2 : 1;

	for (unsigned y = g->y0; y < g->y0 + g->h; y += yd) {
		sand_acc *a = &acc[sand_cell_of(y - g->y0, g->h, n)];
		for (unsigned x = xa; x < xa + B; x += xd) {
			const uint8_t *row = sand_band_row(by, x / ppc, y);
			sand_acc_y(a, sand_y_px(g->fmt, row, x % ppc));
		}
	}
	for (unsigned cy = g->y0 / 2; cy < (g->y0 + g->h) / 2; cy += cdy) {
		sand_acc *a = &acc[sand_cell_of(2 * cy - g->y0, g->h, n)];
		for (unsigned cx = xa / 2; cx < (xa + B) / 2; cx += cdx) {
			const uint8_t *row = sand_band_row(bc, cx / cppc, cy);
			uint32_t cb, cr;
			sand_c_px(g->fmt, row, cx % cppc, &cb, &cr);
			sand_acc_c(a, cb, cr);
		}
	}
}

/* ------------------------------------------------------------------ */
/* colour conversion (10 bit YCbCr -> 8 bit RGB, same transfer function) */

static inline uint8_t sand_q8(double v)
{
	if (v < 0.0) v = 0.0;
	if (v > 1.0) v = 1.0;
	return (uint8_t)(v * 255.0 + 0.5);
}

static inline sand_rgb sand_ycc_to_rgb(double y, double cb, double cr,
				       sand_matrix mx, int full)
{
	double Y, Cb, Cr;
	if (full) {
		Y = y / 1023.0; Cb = (cb - 512.0) / 1023.0; Cr = (cr - 512.0) / 1023.0;
	} else {
		Y = (y - 64.0) / 876.0; Cb = (cb - 512.0) / 896.0; Cr = (cr - 512.0) / 896.0;
	}
	double kr, kb;
	if (mx == SAND_BT2020) { kr = 0.2627; kb = 0.0593; }
	else                   { kr = 0.2126; kb = 0.0722; }
	const double kg = 1.0 - kr - kb;
	const double R = Y + 2.0 * (1.0 - kr) * Cr;
	const double B = Y + 2.0 * (1.0 - kb) * Cb;
	const double G = (Y - kr * R - kb * B) / kg;
	sand_rgb o = { sand_q8(R), sand_q8(G), sand_q8(B) };
	return o;
}

static inline sand_rgb sand_acc_to_rgb(const sand_acc *a, sand_matrix mx, int full)
{
	sand_rgb black = { 0, 0, 0 };
	if (!a->ny)
		return black;
	const double y  = (double)a->sy / a->ny;
	const double cb = a->nc ? (double)a->scb / a->nc : 512.0;
	const double cr = a->nc ? (double)a->scr / a->nc : 512.0;
	return sand_ycc_to_rgb(y, cb, cr, mx, full);
}

/* ------------------------------------------------------------------ */
/* sampler                                                             */

static inline void sand_sampler_free(sand_sampler *s)
{
	for (int i = 0; i < 4; i++) {
		free(s->by[i].buf); free(s->bc[i].buf); free(s->acc[i]);
	}
	memset(s, 0, sizeof(*s));
}

static inline int sand_sampler_init(sand_sampler *s, const sand_geom *g,
				    const sand_edge_cfg *cfg)
{
	memset(s, 0, sizeof(*s));
	s->g = *g;
	s->cfg = *cfg;

	/* chroma pairs need even rectangle and even band */
	s->g.x0 &= ~1u; s->g.y0 &= ~1u; s->g.w &= ~1u; s->g.h &= ~1u;
	if (!s->cfg.xdecim) s->cfg.xdecim = 1;
	if (!s->cfg.ydecim) s->cfg.ydecim = 1;
	s->cfg.band_px = (s->cfg.band_px + 1) & ~1u;

	if (!s->g.w || !s->g.h || !s->g.col_height || s->cfg.band_px < 2 ||
	    s->cfg.band_px > s->g.w / 2 || s->cfg.band_px > s->g.h / 2)
		return -EINVAL;

	s->n[SAND_TOP] = cfg->top;       s->n[SAND_BOTTOM] = cfg->bottom;
	s->n[SAND_LEFT] = cfg->left;     s->n[SAND_RIGHT] = cfg->right;
	if (s->n[SAND_TOP] > s->g.w || s->n[SAND_BOTTOM] > s->g.w ||
	    s->n[SAND_LEFT] > s->g.h || s->n[SAND_RIGHT] > s->g.h)
		return -EINVAL;

	for (int i = 0; i < 4; i++) {
		if (!s->n[i])
			continue;
		s->acc[i] = (sand_acc *)calloc(s->n[i], sizeof(sand_acc));
		if (!s->acc[i]) {
			sand_sampler_free(s);
			return -ENOMEM;
		}
	}
	return 0;
}

static inline int sand_sampler_run(sand_sampler *s, const sand_maps *m,
				   sand_edge_out *o)
{
	const sand_geom *g = &s->g;
	const sand_edge_cfg *c = &s->cfg;
	const unsigned ppc = sand_px_per_col(g->fmt);
	const size_t csy = sand_col_stride_y(g), csc = sand_col_stride_c(g);
	const uint8_t *mc = m->c ? m->c : m->y;
	const size_t mcs = m->c ? m->c_size : m->y_size;
	const unsigned B = c->band_px, xd = c->xdecim, yd = c->ydecim;
	const unsigned cdy = yd > 1 ? yd / 2 : 1;
	int rc;

	for (int side = SAND_TOP; side <= SAND_BOTTOM; side++) {
		const unsigned n = s->n[side];
		if (!n)
			continue;
		const unsigned ya = side == SAND_TOP ? g->y0 : g->y0 + g->h - B;
		const unsigned cf = g->x0 / ppc, cl = (g->x0 + g->w - 1) / ppc;

		rc = sand_band_fill(&s->by[side], m->y, m->y_size, g->off_y, csy,
				    cf, cl, ya, ya + B, 1);
		if (rc) return rc;
		rc = sand_band_fill(&s->bc[side], mc, mcs, g->off_c, csc,
				    cf, cl, ya / 2, (ya + B) / 2, 1);
		if (rc) return rc;

		memset(s->acc[side], 0, n * sizeof(sand_acc));
		sand_acc_hband(g, &s->by[side], &s->bc[side], ya, B, n, xd, yd,
			       s->acc[side]);
	}

	for (int side = SAND_LEFT; side <= SAND_RIGHT; side++) {
		const unsigned n = s->n[side];
		if (!n)
			continue;
		const unsigned xa = side == SAND_LEFT ? g->x0 : g->x0 + g->w - B;
		const unsigned cf = xa / ppc, cl = (xa + B - 1) / ppc;

		rc = sand_band_fill(&s->by[side], m->y, m->y_size, g->off_y, csy,
				    cf, cl, g->y0, g->y0 + g->h, yd);
		if (rc) return rc;
		rc = sand_band_fill(&s->bc[side], mc, mcs, g->off_c, csc,
				    cf, cl, g->y0 / 2, (g->y0 + g->h) / 2, cdy);
		if (rc) return rc;

		memset(s->acc[side], 0, n * sizeof(sand_acc));
		sand_acc_vband(g, &s->by[side], &s->bc[side], xa, B, n, xd, yd,
			       s->acc[side]);
	}

	sand_rgb *dst[4] = { o->top, o->bottom, o->left, o->right };
	for (int side = 0; side < 4; side++)
		for (unsigned i = 0; i < s->n[side]; i++)
			dst[side][i] = sand_acc_to_rgb(&s->acc[side][i], c->matrix,
						       c->full_range);
	return 0;
}

/* ------------------------------------------------------------------ */
/* debug: slow per-pixel accessor + decimated PPM preview               */
/* Run this once on real hardware to confirm the chroma layout.         */

static inline int sand_px_ycc(const sand_geom *g, const sand_maps *m,
			      unsigned x, unsigned y,
			      uint32_t *Y, uint32_t *Cb, uint32_t *Cr)
{
	const unsigned ppc = sand_px_per_col(g->fmt), cppc = ppc / 2;
	const uint8_t *mc = m->c ? m->c : m->y;
	const size_t mcs = m->c ? m->c_size : m->y_size;

	size_t oy = g->off_y + (size_t)(x / ppc) * sand_col_stride_y(g) +
		    (size_t)y * SAND_COL_BYTES;
	size_t oc = g->off_c + (size_t)((x / 2) / cppc) * sand_col_stride_c(g) +
		    (size_t)(y / 2) * SAND_COL_BYTES;
	if (oy + SAND_COL_BYTES > m->y_size || oc + SAND_COL_BYTES > mcs)
		return -ERANGE;
	*Y = sand_y_px(g->fmt, m->y + oy, x % ppc);
	sand_c_px(g->fmt, mc + oc, (x / 2) % cppc, Cb, Cr);
	return 0;
}

static inline int sand_dump_ppm(const sand_geom *g0, const sand_maps *m,
				const char *path, unsigned step,
				sand_matrix mx, int full)
{
	sand_geom g = *g0;
	if (!step) step = 1;
	FILE *f = fopen(path, "wb");
	if (!f)
		return -errno;
	const unsigned ow = g.w / step, oh = g.h / step;
	fprintf(f, "P6\n%u %u\n255\n", ow, oh);
	for (unsigned j = 0; j < oh; j++)
		for (unsigned i = 0; i < ow; i++) {
			uint32_t Y, Cb, Cr;
			int rc = sand_px_ycc(&g, m, g.x0 + i * step, g.y0 + j * step,
					     &Y, &Cb, &Cr);
			if (rc) { fclose(f); return rc; }
			sand_rgb p = sand_ycc_to_rgb(Y, Cb, Cr, mx, full);
			fwrite(&p, 1, 3, f);
		}
	fclose(f);
	return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SAND_SAMPLE_H */
