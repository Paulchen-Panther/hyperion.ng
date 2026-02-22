#include <grabber/audio/ProjectMWrapper.h>

#ifdef ENABLE_PROJECTM

#include <utils/Logger.h>

// Qt
#include <QDir>

// EGL headers
#include <EGL/egl.h>

// OpenGL ES headers
#include <GLES3/gl3.h>

// projectM header (umbrella - includes core.h, parameters.h, render_opengl.h, etc.)
#include <projectM-4/projectM.h>

struct ProjectMWrapperPrivate
{
	EGLDisplay display  { EGL_NO_DISPLAY };
	EGLContext context  { EGL_NO_CONTEXT };
	EGLSurface surface  { EGL_NO_SURFACE };
	projectm_handle pm  { nullptr };
};

ProjectMWrapper::ProjectMWrapper()
	: _p(new ProjectMWrapperPrivate())
	, _initialised(false)
	, _width(0)
	, _height(0)
	, _log(Logger::getInstance("ProjectMWrapper"))
{
}

ProjectMWrapper::~ProjectMWrapper()
{
	cleanup();
	delete _p;
}

bool ProjectMWrapper::init(int width, int height, const QString& presetPath)
{
	if (_initialised)
		cleanup();

	_width  = width;
	_height = height;

	// ---- EGL setup --------------------------------------------------------

	_p->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (_p->display == EGL_NO_DISPLAY)
	{
		Error(_log, "ProjectM: eglGetDisplay failed");
		return false;
	}

	EGLint major = 0, minor = 0;
	if (!eglInitialize(_p->display, &major, &minor))
	{
		Error(_log, "ProjectM: eglInitialize failed (error 0x%x)", eglGetError());
		return false;
	}

	Debug(_log, "ProjectM: EGL version %d.%d", major, minor);

	// Request an OpenGL ES 3 config with RGB + depth
	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_DEPTH_SIZE,      16,
		EGL_NONE
	};

	EGLConfig eglConfig = nullptr;
	EGLint    numConfigs = 0;
	if (!eglChooseConfig(_p->display, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs < 1)
	{
		Error(_log, "ProjectM: eglChooseConfig failed (error 0x%x)", eglGetError());
		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
		return false;
	}

	// Create a pbuffer surface
	const EGLint pbufferAttribs[] = {
		EGL_WIDTH,  _width,
		EGL_HEIGHT, _height,
		EGL_NONE
	};

	_p->surface = eglCreatePbufferSurface(_p->display, eglConfig, pbufferAttribs);
	if (_p->surface == EGL_NO_SURFACE)
	{
		Error(_log, "ProjectM: eglCreatePbufferSurface failed (error 0x%x)", eglGetError());
		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
		return false;
	}

	// Bind OpenGL ES API before creating the context
	eglBindAPI(EGL_OPENGL_ES_API);

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};

	_p->context = eglCreateContext(_p->display, eglConfig, EGL_NO_CONTEXT, contextAttribs);
	if (_p->context == EGL_NO_CONTEXT)
	{
		Error(_log, "ProjectM: eglCreateContext failed (error 0x%x)", eglGetError());
		eglDestroySurface(_p->display, _p->surface);
		_p->surface = EGL_NO_SURFACE;
		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
		return false;
	}

	if (!eglMakeCurrent(_p->display, _p->surface, _p->surface, _p->context))
	{
		Error(_log, "ProjectM: eglMakeCurrent failed (error 0x%x)", eglGetError());
		eglDestroyContext(_p->display, _p->context);
		_p->context = EGL_NO_CONTEXT;
		eglDestroySurface(_p->display, _p->surface);
		_p->surface = EGL_NO_SURFACE;
		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
		return false;
	}

	// ---- projectM setup ---------------------------------------------------

	_p->pm = projectm_create();
	if (_p->pm == nullptr)
	{
		Error(_log, "ProjectM: projectm_create failed");
		eglMakeCurrent(_p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(_p->display, _p->context);
		_p->context = EGL_NO_CONTEXT;
		eglDestroySurface(_p->display, _p->surface);
		_p->surface = EGL_NO_SURFACE;
		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
		return false;
	}

	projectm_set_window_size(_p->pm, static_cast<size_t>(_width), static_cast<size_t>(_height));
	projectm_set_fps(_p->pm, 30);

	if (!presetPath.isEmpty())
	{
		// projectM 4 core API has no "set preset directory" function.
		// Scan the directory for the first .milk preset and load it.
		const QDir dir(presetPath);
		const QStringList presets = dir.entryList(QStringList() << "*.milk", QDir::Files);
		if (!presets.isEmpty())
		{
			const QString firstPreset = dir.absoluteFilePath(presets.first());
			projectm_load_preset_file(_p->pm, firstPreset.toUtf8().constData(), false /* no smooth transition */);
			Debug(_log, "ProjectM: loaded preset: %s", QSTRING_CSTR(firstPreset));
		}
		else
		{
			Warning(_log, "ProjectM: no .milk presets found in: %s", QSTRING_CSTR(presetPath));
		}
	}

	_initialised = true;
	Info(_log, "ProjectM: initialised (%dx%d), preset path: %s", _width, _height, QSTRING_CSTR(presetPath));
	return true;
}

void ProjectMWrapper::cleanup()
{
	if (!_initialised)
		return;

	_initialised = false;

	if (_p->pm != nullptr)
	{
		projectm_destroy(_p->pm);
		_p->pm = nullptr;
	}

	if (_p->display != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(_p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		if (_p->context != EGL_NO_CONTEXT)
		{
			eglDestroyContext(_p->display, _p->context);
			_p->context = EGL_NO_CONTEXT;
		}

		if (_p->surface != EGL_NO_SURFACE)
		{
			eglDestroySurface(_p->display, _p->surface);
			_p->surface = EGL_NO_SURFACE;
		}

		eglTerminate(_p->display);
		_p->display = EGL_NO_DISPLAY;
	}
}

void ProjectMWrapper::addPCMData(const int16_t* buffer, int length)
{
	if (!_initialised || _p->pm == nullptr || buffer == nullptr || length <= 0)
		return;

	projectm_pcm_add_int16(_p->pm, buffer, static_cast<uint32_t>(length), PROJECTM_MONO);
}

bool ProjectMWrapper::renderFrame(Image<ColorRgb>& image)
{
	if (!_initialised || _p->pm == nullptr)
		return false;

	// Make our EGL context current in case it was released
	if (!eglMakeCurrent(_p->display, _p->surface, _p->surface, _p->context))
	{
		Error(_log, "ProjectM: eglMakeCurrent failed before render (error 0x%x)", eglGetError());
		return false;
	}

	// Render one frame
	projectm_opengl_render_frame(_p->pm);

	// Read back pixels (bottom-up; OpenGL convention is bottom-left origin)
	const int bufferSize = _width * _height * 3;
	std::vector<uint8_t> pixels(static_cast<size_t>(bufferSize));
	glReadPixels(0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// Fill image, flipping vertically to convert from OpenGL (bottom-up) to top-down
	image = Image<ColorRgb>(_width, _height);
	for (int y = 0; y < _height; ++y)
	{
		const int srcRow = (_height - 1 - y);
		memcpy(
			reinterpret_cast<uint8_t*>(image.memptr()) + y * _width * 3,
			pixels.data() + srcRow * _width * 3,
			static_cast<size_t>(_width * 3)
		);
	}

	return true;
}

#endif // ENABLE_PROJECTM
