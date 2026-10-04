/*
 * test_sand_sample.c - self-consistency test for sand_sample.h
 *
 * Builds random linear YCbCr frames, tiles them into SAND128 (NV12 and
 * P030, interleaved and separate chroma) with an independent tiler, runs
 * the sampler and compares the per-cell accumulators with a reference
 * computed straight from the linear frame.
 *
 * NOTE: this validates indexing/cell/decimation arithmetic against the
 * layout *assumed* in sand_sample.h. It cannot prove that real Pi 5 buffers
 * use that layout - use sand_dump_ppm() on hardware for that.
 */
#include "grabber/drm/sand_sample.h"

#define FBW 416u
#define FBH 240u

struct lin { uint16_t *Y, *Cb, *Cr; };   /* Y: FBWxFBH, Cb/Cr: (FBW/2)x(FBH/2) */

static uint32_t rng = 0x12345678u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* --- independent tiler -------------------------------------------------- */
struct tile { sand_fmt fmt; unsigned col_height; int separate; size_t off_c; size_t cs_y, cs_c; };

static void put32(uint8_t *p, unsigned slot, uint32_t v)
{
	uint32_t w; memcpy(&w, p, 4);
	w &= ~(0x3ffu << (10 * slot)); w |= v << (10 * slot);
	memcpy(p, &w, 4);
}
static void put64(uint8_t *p, unsigned shift, uint64_t v)
{
	uint64_t w; memcpy(&w, p, 8);
	w &= ~(0x3ffull << shift); w |= v << shift;
	memcpy(p, &w, 8);
}

static void tile_frame(uint8_t *buf, const struct tile *t, const struct lin *l)
{
	const unsigned ppc = t->fmt == SAND_P030 ? 96 : 128;
	for (unsigned y = 0; y < FBH; y++)
		for (unsigned x = 0; x < FBW; x++) {
			size_t o = (size_t)(x / ppc) * t->cs_y + (size_t)y * 128;
			unsigned xin = x % ppc;
			uint32_t v = l->Y[y * FBW + x];
			if (t->fmt == SAND_NV12) buf[o + xin] = (uint8_t)(v >> 2);
			else put32(buf + o + (xin / 3) * 4, xin % 3, v);
		}
	for (unsigned cy = 0; cy < FBH / 2; cy++)
		for (unsigned cx = 0; cx < FBW / 2; cx++) {
			unsigned cppc = ppc / 2;
			size_t o = t->off_c + (size_t)(cx / cppc) * t->cs_c + (size_t)cy * 128;
			unsigned cin = cx % cppc;
			uint32_t cb = l->Cb[cy * (FBW / 2) + cx], cr = l->Cr[cy * (FBW / 2) + cx];
			if (t->fmt == SAND_NV12) { buf[o + 2 * cin] = cb >> 2; buf[o + 2 * cin + 1] = cr >> 2; }
			else {
				unsigned sh = 20 * (cin % 3);
				put64(buf + o + (cin / 3) * 8, sh, cb);
				put64(buf + o + (cin / 3) * 8, sh + 10, cr);
			}
		}
}

/* --- reference (same sample grid, reads the linear frame) --------------- */
static void ref_acc(const sand_geom *g, const sand_edge_cfg *c, const struct lin *l,
		    int side, unsigned n, sand_acc *acc)
{
	const unsigned B = c->band_px, xd = c->xdecim, yd = c->ydecim;
	const unsigned cdx = xd > 1 ? xd / 2 : 1, cdy = yd > 1 ? yd / 2 : 1;
	const int horiz = side <= SAND_BOTTOM;
	unsigned xa = g->x0, ya = g->y0, bw = g->w, bh = g->h;
	if (horiz) { bh = B; if (side == SAND_BOTTOM) ya = g->y0 + g->h - B; }
	else       { bw = B; if (side == SAND_RIGHT)  xa = g->x0 + g->w - B; }

	memset(acc, 0, n * sizeof(*acc));
	for (unsigned y = ya; y < ya + bh; y += yd)
		for (unsigned x = xa; x < xa + bw; x += xd) {
			unsigned cell = horiz ? sand_cell_of(x - g->x0, g->w, n)
					      : sand_cell_of(y - g->y0, g->h, n);
			acc[cell].sy += l->Y[y * FBW + x]; acc[cell].ny++;
		}
	for (unsigned cy = ya / 2; cy < (ya + bh) / 2; cy += cdy)
		for (unsigned cx = xa / 2; cx < (xa + bw) / 2; cx += cdx) {
			unsigned cell = horiz ? sand_cell_of(2 * cx - g->x0, g->w, n)
					      : sand_cell_of(2 * cy - g->y0, g->h, n);
			acc[cell].scb += l->Cb[cy * (FBW / 2) + cx];
			acc[cell].scr += l->Cr[cy * (FBW / 2) + cx];
			acc[cell].nc++;
		}
}

static int run_case(sand_fmt fmt, int separate, sand_chroma_mode mode, unsigned dec,
		    int mapped_chroma_separately)
{
	struct lin l;
	l.Y = malloc(FBW * FBH * 2);
	l.Cb = malloc(FBW * FBH / 2); l.Cr = malloc(FBW * FBH / 2);
	for (unsigned i = 0; i < FBW * FBH; i++)
		l.Y[i] = fmt == SAND_NV12 ? (rnd() & 0xff) << 2 : rnd() & 0x3ff;
	for (unsigned i = 0; i < FBW * FBH / 4; i++) {
		l.Cb[i] = fmt == SAND_NV12 ? (rnd() & 0xff) << 2 : rnd() & 0x3ff;
		l.Cr[i] = fmt == SAND_NV12 ? (rnd() & 0xff) << 2 : rnd() & 0x3ff;
	}

	const unsigned ppc = fmt == SAND_P030 ? 96 : 128;
	const unsigned ncols = (FBW + ppc - 1) / ppc;
	const unsigned YL = 256, UVL = 128;
	struct tile t = { .fmt = fmt, .separate = separate };
	size_t total;
	sand_geom g = { .fmt = fmt, .x0 = 16, .y0 = 8, .w = 384, .h = 224, .chroma_mode = mode };
	if (!separate) {
		t.col_height = YL + UVL; t.cs_y = t.cs_c = (size_t)t.col_height * 128;
		t.off_c = (size_t)YL * 128; total = ncols * t.cs_y;
	} else {
		t.col_height = YL; t.cs_y = (size_t)YL * 128; t.cs_c = (size_t)(YL / 2) * 128;
		t.off_c = ncols * t.cs_y; total = t.off_c + ncols * t.cs_c;
	}
	g.col_height = t.col_height; g.off_y = 0; g.off_c = t.off_c;

	uint8_t *buf = calloc(1, total);
	tile_frame(buf, &t, &l);

	sand_maps m = { .y = buf, .y_size = total };
	if (mapped_chroma_separately) {         /* chroma image in its own buffer */
		size_t csz = total - t.off_c;
		m.c = buf + t.off_c; m.c_size = csz; g.off_c = 0; m.y_size = t.off_c;
	}

	sand_edge_cfg cfg = { .top = 13, .bottom = 13, .left = 7, .right = 7, .band_px = 16,
			      .xdecim = dec, .ydecim = dec, .matrix = SAND_BT709 };
	sand_sampler s;
	if (sand_sampler_init(&s, &g, &cfg)) { puts("init failed"); return 1; }

	sand_rgb ot[13], ob[13], ol[7], or_[7];
	sand_edge_out out = { ot, ob, ol, or_ };
	int rc = sand_sampler_run(&s, &m, &out);
	if (rc) { printf("run rc=%d\n", rc); return 1; }

	int bad = 0;
	for (int side = 0; side < 4; side++) {
		sand_acc ref[13];
		ref_acc(&s.g, &s.cfg, &l, side, s.n[side], ref);
		for (unsigned i = 0; i < s.n[side]; i++)
			if (memcmp(&ref[i], &s.acc[side][i], sizeof(sand_acc))) {
				printf("  MISMATCH side=%d cell=%u\n", side, i);
				bad = 1;
			}
	}

	/* bounds check: a truncated mapping must fail cleanly, not crash */
	if (!mapped_chroma_separately) {
		sand_maps small = m; small.y_size = total / 2;
		if (sand_sampler_run(&s, &small, &out) != -ERANGE) {
			puts("  bounds check did not trigger"); bad = 1;
		}
	}

	sand_sampler_free(&s);
	free(buf); free(l.Y); free(l.Cb); free(l.Cr);
	return bad;
}

int main(void)
{
	int fails = 0;
	for (int f = 0; f < 2; f++)
		for (int sep = 0; sep < 2; sep++)
			for (unsigned dec = 1; dec <= 4; dec *= 2) {
				int r1 = run_case(f ? SAND_P030 : SAND_NV12, sep, SAND_CHROMA_AUTO, dec, 0);
				int r2 = run_case(f ? SAND_P030 : SAND_NV12, sep,
						  sep ? SAND_CHROMA_SEPARATE : SAND_CHROMA_INTERLEAVED, dec, 0);
				printf("%s %-11s dec=%u  auto:%s forced:%s\n",
				       f ? "P030" : "NV12", sep ? "separate" : "interleaved", dec,
				       r1 ? "FAIL" : "ok", r2 ? "FAIL" : "ok");
				fails += r1 + r2;
			}
	/* chroma in a separate dma-buf (offset 0 inside its own mapping) */
	for (int f = 0; f < 2; f++) {
		int r = run_case(f ? SAND_P030 : SAND_NV12, 1, SAND_CHROMA_SEPARATE, 2, 1);
		printf("%s separate-dmabuf       ok?:%s\n", f ? "P030" : "NV12", r ? "FAIL" : "ok");
		fails += r;
	}
	puts(fails ? "FAILED" : "ALL PASSED");
	return fails != 0;
}
