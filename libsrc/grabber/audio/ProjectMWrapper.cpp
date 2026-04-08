#include <grabber/audio/ProjectMWrapper.h>

#ifdef ENABLE_PROJECTM

#include <utils/Logger.h>

// Qt
#include <QDir>

// Platform-specific OpenGL/context headers
#ifdef __linux__
#  include <EGL/egl.h>
#  include <GLES3/gl3.h>
#elif defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/OpenGL.h>   // CGL API
#  include <OpenGL/gl3.h>      // OpenGL 3.2 Core functions
#endif

// projectM header (umbrella - includes core.h, parameters.h, render_opengl.h, etc.)
#include <projectM-4/projectM.h>

struct ProjectMWrapperPrivate
{
#ifdef __linux__
	EGLDisplay    display { EGL_NO_DISPLAY };
	EGLContext    context { EGL_NO_CONTEXT };
	EGLSurface    surface { EGL_NO_SURFACE };
#elif defined(__APPLE__)
	CGLContextObj context  { nullptr };
	GLuint        fbo      { 0 };
	GLuint        rboColor { 0 };
	GLuint        rboDepth { 0 };
#endif
	projectm_handle pm { nullptr };
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

#ifdef __linux__
	// ---- EGL headless context setup (Linux) --------------------------------

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

#elif defined(__APPLE__)
	// ---- CGL headless context setup (macOS) --------------------------------

	CGLPixelFormatAttribute attribs[] = {
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
		kCGLPFAColorSize,     (CGLPixelFormatAttribute)24,
		kCGLPFADepthSize,     (CGLPixelFormatAttribute)16,
		(CGLPixelFormatAttribute)0
	};

	CGLPixelFormatObj pix = nullptr;
	GLint npix = 0;
	CGLError cglErr = CGLChoosePixelFormat(attribs, &pix, &npix);
	if (cglErr != kCGLNoError || pix == nullptr)
	{
		Error(_log, "ProjectM: CGLChoosePixelFormat failed: %s", CGLErrorString(cglErr));
		return false;
	}

	cglErr = CGLCreateContext(pix, nullptr, &_p->context);
	CGLDestroyPixelFormat(pix);
	if (cglErr != kCGLNoError)
	{
		Error(_log, "ProjectM: CGLCreateContext failed: %s", CGLErrorString(cglErr));
		return false;
	}

	cglErr = CGLSetCurrentContext(_p->context);
	if (cglErr != kCGLNoError)
	{
		Error(_log, "ProjectM: CGLSetCurrentContext failed: %s", CGLErrorString(cglErr));
		CGLDestroyContext(_p->context);
		_p->context = nullptr;
		return false;
	}

	// Create an FBO so glReadPixels can read back rendered pixels without a drawable
	glGenFramebuffers(1, &_p->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, _p->fbo);

	glGenRenderbuffers(1, &_p->rboColor);
	glBindRenderbuffer(GL_RENDERBUFFER, _p->rboColor);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, _width, _height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _p->rboColor);

	glGenRenderbuffers(1, &_p->rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, _p->rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _p->rboDepth);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		Error(_log, "ProjectM: FBO is not complete");
		CGLSetCurrentContext(nullptr);
		CGLDestroyContext(_p->context);
		_p->context = nullptr;
		return false;
	}
#endif

	// ---- projectM setup ---------------------------------------------------

	_p->pm = projectm_create();
	if (_p->pm == nullptr)
	{
		Error(_log, "ProjectM: projectm_create failed");
		cleanup();
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

#ifdef __linux__
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
#elif defined(__APPLE__)
	if (_p->context != nullptr)
	{
		// Make context current before deleting GL objects; log but continue on failure
		if (CGLSetCurrentContext(_p->context) != kCGLNoError)
			Warning(_log, "ProjectM: CGLSetCurrentContext failed during cleanup; GL objects may leak");

		if (_p->fbo != 0)
		{
			glDeleteFramebuffers(1, &_p->fbo);
			_p->fbo = 0;
		}
		if (_p->rboColor != 0)
		{
			glDeleteRenderbuffers(1, &_p->rboColor);
			_p->rboColor = 0;
		}
		if (_p->rboDepth != 0)
		{
			glDeleteRenderbuffers(1, &_p->rboDepth);
			_p->rboDepth = 0;
		}

		CGLSetCurrentContext(nullptr);
		CGLDestroyContext(_p->context);
		_p->context = nullptr;
	}
#endif
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

	// Make the platform context current before rendering
#ifdef __linux__
	if (!eglMakeCurrent(_p->display, _p->surface, _p->surface, _p->context))
	{
		Error(_log, "ProjectM: eglMakeCurrent failed before render (error 0x%x)", eglGetError());
		return false;
	}
#elif defined(__APPLE__)
	CGLError cglErr = CGLSetCurrentContext(_p->context);
	if (cglErr != kCGLNoError)
	{
		Error(_log, "ProjectM: CGLSetCurrentContext failed before render: %s", CGLErrorString(cglErr));
		return false;
	}
	// Bind our FBO so projectM renders into it and glReadPixels can read it back
	glBindFramebuffer(GL_FRAMEBUFFER, _p->fbo);
#endif

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
