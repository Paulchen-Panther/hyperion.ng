#include <grabber/drm/DRMFrameGrabber.h>

// Add missing AMD format modifier definitions for downward compatibility
#ifndef AMD_FMT_MOD_TILE_VER_GFX11
#define AMD_FMT_MOD_TILE_VER_GFX11 4
#endif
#ifndef AMD_FMT_MOD_TILE_VER_GFX12
#define AMD_FMT_MOD_TILE_VER_GFX12 5
#endif

static QString getDrmFormat(uint32_t format)
{
	return QString::fromLatin1(reinterpret_cast<const char*>(&format), sizeof format);
}

static QString getDrmModifierName(uint64_t modifier)
{
	uint64_t vendor = modifier >> 56;
	uint64_t mod = modifier & 0x00FFFFFFFFFFFFFF;
	QString name = QString("VENDOR: 0x%1, MOD: 0x%2").arg(vendor, 2, 16, QChar('0')).arg(mod, 14, 16, QChar('0'));

	switch (modifier)
	{
	case DRM_FORMAT_MOD_INVALID:
		return "DRM_FORMAT_MOD_INVALID";
	case DRM_FORMAT_MOD_LINEAR:
		return "DRM_FORMAT_MOD_LINEAR";
	default:
		break;
	}

	switch (vendor)
	{
	case DRM_FORMAT_MOD_VENDOR_INTEL:
		switch (mod)
		{
		case I915_FORMAT_MOD_X_TILED:
			return "I915_FORMAT_MOD_X_TILED";
		case I915_FORMAT_MOD_Y_TILED:
			return "I915_FORMAT_MOD_Y_TILED";
		case I915_FORMAT_MOD_Yf_TILED:
			return "I915_FORMAT_MOD_Yf_TILED";
		case I915_FORMAT_MOD_Y_TILED_CCS:
			return "I915_FORMAT_MOD_Y_TILED_CCS";
		case I915_FORMAT_MOD_Yf_TILED_CCS:
			return "I915_FORMAT_MOD_Yf_TILED_CCS";
		default:
			return QString("DRM_FORMAT_MOD_INTEL_UNKNOWN [0x%1]").arg(mod, 14, 16, QChar('0'));
		}
	case DRM_FORMAT_MOD_VENDOR_AMD:
		if (mod & AMD_FMT_MOD_TILE_VER_GFX9)
			return "AMD_FMT_MOD_TILE_VER_GFX9";
		if (mod & AMD_FMT_MOD_TILE_VER_GFX10)
			return "AMD_FMT_MOD_TILE_VER_GFX10";
		if (mod & AMD_FMT_MOD_TILE_VER_GFX11)
			return "AMD_FMT_MOD_TILE_VER_GFX11";
		if (mod & AMD_FMT_MOD_TILE_VER_GFX12)
			return "AMD_FMT_MOD_TILE_VER_GFX12";
		if (mod & AMD_FMT_MOD_DCC_BLOCK_128B)
			return "AMD_FMT_MOD_DCC_BLOCK_128B";
		if (mod & AMD_FMT_MOD_DCC_BLOCK_256B)
			return "AMD_FMT_MOD_DCC_BLOCK_256B";
		return QString("DRM_FORMAT_MOD_AMD_UNKNOWN [0x%1]").arg(mod, 14, 16, QChar('0'));
	case DRM_FORMAT_MOD_VENDOR_NVIDIA:
		if (mod & 0x10)
			return "DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D";
		return QString("DRM_FORMAT_MOD_NVIDIA_UNKNOWN [0x%1]").arg(mod , 14, 16, QChar('0'));
	case DRM_FORMAT_MOD_VENDOR_BROADCOM:
		switch (fourcc_mod_broadcom_mod(modifier))
		{
		case DRM_FORMAT_MOD_BROADCOM_SAND32:
			return "DRM_FORMAT_MOD_BROADCOM_SAND32";
		case DRM_FORMAT_MOD_BROADCOM_SAND64:
			return "DRM_FORMAT_MOD_BROADCOM_SAND64";
		case DRM_FORMAT_MOD_BROADCOM_SAND128:
			return "DRM_FORMAT_MOD_BROADCOM_SAND128";
		case DRM_FORMAT_MOD_BROADCOM_SAND256:
			return "DRM_FORMAT_MOD_BROADCOM_SAND256";
		case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED:
			return "DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED";
		default:
			return QString("DRM_FORMAT_MOD_BROADCOM_UNKNOWN [0x%1]").arg(mod, 14, 16, QChar('0'));
		}
	case DRM_FORMAT_MOD_VENDOR_ARM:
		if ((modifier & DRM_FORMAT_MOD_ARM_AFBC(0)) == DRM_FORMAT_MOD_ARM_AFBC(0))
			return "DRM_FORMAT_MOD_ARM_AFBC";
		return QString("DRM_FORMAT_MOD_ARM_UNKNOWN [0x%1]").arg(mod, 14, 16, QChar('0'));

	default:
		break;
	}

	return name;
}

static PixelFormat GetPixelFormatForDrmFormat(uint32_t format)
{
	switch (format)
	{
#ifdef DRM_FORMAT_RGB565
		case DRM_FORMAT_RGB565:  return PixelFormat::BGR16;
#endif
		case DRM_FORMAT_XRGB8888: return PixelFormat::BGR32;
		case DRM_FORMAT_ARGB8888: return PixelFormat::BGR32;
		case DRM_FORMAT_XBGR8888: return PixelFormat::RGB32;
		case DRM_FORMAT_ABGR8888: return PixelFormat::RGB32;
		case DRM_FORMAT_NV12:     return PixelFormat::NV12;
#ifdef DRM_FORMAT_NV21
		case DRM_FORMAT_NV21:     return PixelFormat::NV21;
#endif
#ifdef DRM_FORMAT_P010
		case DRM_FORMAT_P010:     return PixelFormat::P010;
#endif
#ifdef DRM_FORMAT_P030
		case DRM_FORMAT_P030:     return PixelFormat::P030;
#endif
		case DRM_FORMAT_YUV420:   return PixelFormat::I420;
		default:                  return PixelFormat::NO_CHANGE;
	}
}

// Forward declarations for QDebug operators
QDebug operator<<(QDebug dbg, const drmModeFB2* fb);
QDebug operator<<(QDebug dbg, const drmModePlane* plane);

DRMFrameGrabber::DRMFrameGrabber(int deviceIdx, int cropLeft, int cropRight, int cropTop, int cropBottom)
	: Grabber("GRABBER-DRM", cropLeft, cropRight, cropTop, cropBottom), _deviceFd(-1), _crtc(nullptr)
{
	_input = deviceIdx;
	_useImageResampler = true;
}

DRMFrameGrabber::~DRMFrameGrabber()
{
	freeResources();
	closeDevice();
}

void DRMFrameGrabber::freeResources()
{
	_connectors.clear();
	_encoders.clear();

	if (_crtc != nullptr)
	{
		drmModeFreeCrtc(_crtc);
		_crtc = nullptr;
	}

	for (auto const &[id, plane] : _planes)
	{
		if (plane != nullptr)
		{
			drmModeFreePlane(plane);
		}
	}
	_planes.clear();

	for (auto const &[id, framebuffer] : _framebuffers)
	{
		drmModeFreeFB2(framebuffer);
	}
	_framebuffers.clear();
}

bool DRMFrameGrabber::setupScreen()
{
	freeResources();
	closeDevice();

	bool success = openDevice() && getScreenInfo();
	setEnabled(success);

	if (!success)
	{
		freeResources();
		closeDevice();
	}

	return success;
}

bool DRMFrameGrabber::setWidthHeight(int width, int height)
{
	if (Grabber::setWidthHeight(width, height))
	{
		return setupScreen();
	}

	return false;
}

int DRMFrameGrabber::grabFrame(Image<ColorRgb>& image, bool /*forceUpdate*/)
{
	if (!_isEnabled || _isDeviceInError)
	{
		return -1;
	}

	if (_framebuffers.empty())
	{
		Error(_log, "No framebuffers found. Was setupScreen() successful?");
		return -1;
	}

	const auto& [id, framebuffer] = *_framebuffers.begin();

	_pixelFormat = GetPixelFormatForDrmFormat(framebuffer->pixel_format);

	const int w = static_cast<int>(framebuffer->width);
	const int h = static_cast<int>(framebuffer->height);
	const uint64_t modifier = framebuffer->modifier;

	qCDebug(grabber_screen_capture) << QString("Framebuffer ID: %1 - Width: %2 - Height: %3  - DRM Format: %4 - PixelFormat: %5, Modifier: %6")
			.arg(id)
			.arg(w)
			.arg(h)
			.arg(getDrmFormat(framebuffer->pixel_format))
			.arg(pixelFormatToString(_pixelFormat))
			.arg(getDrmModifierName(modifier));

	Grabber::setWidthHeight(w, h);

	if (!_wbConverter.isReady())
	{
		if (!DRMWritebackConverter::isSupported(_deviceFd))
		{
			setInError(QString("Hardware does not support DRM Writeback Connector").arg(modifier, 16, 16, QChar('0')));
			return -1;
		}

		const int crtcIndex = _crtc ? findCrtcIndex() : -1;
		if (!_wbConverter.init(_deviceFd, _crtc ? _crtc->crtc_id : 0, crtcIndex, static_cast<uint32_t>(w), static_cast<uint32_t>(h), _log.data()))
		{
			setInError("DRMWritebackConverter init failed");
			return -1;
		}
	}

	if (wbConverter && wbConverter->isReady())
	{
		uint8_t* result = wbConverter->capture(_log.data());
		if (!result)
		{
			setInError("DrmWritebackConverter::capture failed");
			return -1;
		}

		// Writeback Converter always outputs DRM_FORMAT_ARGB8888 linear
		_imageResampler.processImage(result, w, h, static_cast<int>(wbConverter->stride()), PixelFormat::BGR32, image);
	}

	return 0;
}

bool DRMFrameGrabber::openDevice()
{
	if (_deviceFd >= 0)
	{
		return true;
	}

	if (!_isAvailable)
	{
		return false;
	}

	// Try read-only first to minimize required privileges. Some drivers require O_RDWR; fallback in that case.
	_deviceFd = ::open(QSTRING_CSTR(getDeviceName()), O_RDONLY | O_CLOEXEC);
	if (_deviceFd < 0)
	{
		// fallback to read-write if required by driver
		_deviceFd = ::open(QSTRING_CSTR(getDeviceName()), O_RDWR | O_CLOEXEC);
	}
	if (_deviceFd < 0)
	{
		QString errorReason = QString("Error opening %1, [%2] %3").arg(getDeviceName()).arg(errno).arg(std::strerror(errno));
		this->setInError(errorReason);
		return false;
	}

	return true;
}

bool DRMFrameGrabber::closeDevice()
{
	if (_deviceFd < 0)
	{
		return true;
	}

	bool success = (::close(_deviceFd) == 0);
	_deviceFd = -1;

	return success;
}

QSize DRMFrameGrabber::getScreenSize() const
{
	return getScreenSize(getDeviceName());
}

QSize DRMFrameGrabber::getScreenSize(const QString &device) const
{
	DrmResources drmResources;
	if (!discoverDrmResources(device, drmResources))
	{
		return {};
	}

	// 3. Iterate through connectors to find a connected one
	for (const auto& connector : drmResources.connectors)
	{
		if (connector->connection != DRM_MODE_CONNECTED || connector->count_modes <= 0)
		{
			continue;
		}

		for (const auto& crtc : drmResources.crtcs)
		{
			if (crtc->mode_valid)
			{
				return QSize(crtc->width, crtc->height);
			}
		}
	}

	return {};
}

bool DRMFrameGrabber::discoverDrmResources(const QString& device, DrmResources& resources) const
{
	// 1. Open the DRM device
	int drmfd = ::open(QSTRING_CSTR(device), O_RDONLY | O_CLOEXEC);
	if (drmfd < 0)
	{
		drmfd = ::open(QSTRING_CSTR(device), O_RDWR | O_CLOEXEC);
		if (drmfd < 0)
		{
			return false;
		}
	}

	// 2. Get device resources
	drmModeResPtr drmModeResources = drmModeGetResources(drmfd);
	if (!drmModeResources)
	{
		::close(drmfd);
		return false;
	}

	for (int i = 0; i < drmModeResources->count_connectors; ++i)
	{
		drmModeConnectorPtr connector = drmModeGetConnector(drmfd, drmModeResources->connectors[i]);
		if (connector)
		{
			resources.connectors.emplace_back(connector, drmModeFreeConnector);
		}
	}

	for (int i = 0; i < drmModeResources->count_crtcs; ++i)
	{
		drmModeCrtcPtr crtc = drmModeGetCrtc(drmfd, drmModeResources->crtcs[i]);
		if (crtc)
		{
			resources.crtcs.emplace_back(crtc, drmModeFreeCrtc);
		}
	}

	drmModeFreeResources(drmModeResources);
	::close(drmfd);

	return true;
}

void DRMFrameGrabber::setDrmClientCaps()
{
	if (drmSetClientCap(_deviceFd, DRM_CLIENT_CAP_ATOMIC, 1) < 0)
	{
		Debug(_log, "drmSetClientCap(DRM_CLIENT_CAP_ATOMIC) failed: %s", strerror(errno));
	}
	if (drmSetClientCap(_deviceFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0)
	{
		Debug(_log, "drmSetClientCap(DRM_CLIENT_CAP_UNIVERSAL_PLANES) failed: %s", strerror(errno));
	}
}

void DRMFrameGrabber::enumerateConnectorsAndEncoders(const drmModeRes* resources)
{
	for (int i = 0; i < resources->count_connectors; i++)
	{
		auto connector = std::make_unique<Connector>();
		connector->ptr = drmModeGetConnector(_deviceFd, resources->connectors[i]);
		_connectors.insert_or_assign(resources->connectors[i], std::move(connector));
	}

	for (int i = 0; i < resources->count_encoders; i++)
	{
		auto encoder = std::make_unique<Encoder>();
		encoder->ptr = drmModeGetEncoder(_deviceFd, resources->encoders[i]);
		_encoders.insert_or_assign(resources->encoders[i], std::move(encoder));
	}
}

void DRMFrameGrabber::findActiveCrtc(const drmModeRes* resources)
{
	for (int i = 0; i < resources->count_crtcs; i++)
	{
		_crtc = drmModeGetCrtc(_deviceFd, resources->crtcs[i]);
		if (_crtc && _crtc->mode_valid)
		{
			return; // Found active CRTC, so we can exit
		}
		drmModeFreeCrtc(_crtc);
		_crtc = nullptr;
	}
}

bool DRMFrameGrabber::isPrimaryPlaneForCrtc(uint32_t planeId, const drmModeObjectProperties* properties)
{
	for (unsigned int j = 0; j < properties->count_props; ++j)
	{
		auto prop = drmModeGetProperty(_deviceFd, properties->props[j]);
		if (!prop) continue;

		bool isPrimary = (strcmp(prop->name, "type") == 0 && properties->prop_values[j] == DRM_PLANE_TYPE_PRIMARY);
		drmModeFreeProperty(prop);

		if (isPrimary)
		{
			auto plane = drmModeGetPlane(_deviceFd, planeId);
			if (plane && _crtc && plane->crtc_id == _crtc->crtc_id)
			{
				qCDebug(grabber_screen_flow) << plane;
				_planes.insert({planeId, plane});
				return true;
			}
			if (plane)
			{
				drmModeFreePlane(plane);
			}
		}
	}
	return false;
}

void DRMFrameGrabber::findPrimaryPlane(const drmModePlaneRes* planeResources)
{
	for (unsigned int i = 0; i < planeResources->count_planes; ++i)
	{
		uint32_t planeId = planeResources->planes[i];
		auto properties = drmModeObjectGetProperties(_deviceFd, planeId, DRM_MODE_OBJECT_PLANE);
		if (!properties) continue;

		bool found = isPrimaryPlaneForCrtc(planeId, properties);
		drmModeFreeObjectProperties(properties);

		if (found)
		{
			break;
		}
	}
}

void DRMFrameGrabber::getDrmObjectProperties()
{
    for (auto const &[id, connector] : _connectors)
    {
        auto properties = drmModeObjectGetProperties(_deviceFd, id, DRM_MODE_OBJECT_CONNECTOR);
        if (!properties) continue;

        for (unsigned int i = 0; i < properties->count_props; i++)
        {
            auto prop = drmModeGetProperty(_deviceFd, properties->props[i]);
            if (!prop) continue;
            connector->props.insert({std::string(prop->name), {.spec = prop, .value = properties->prop_values[i]}});
        }
        drmModeFreeObjectProperties(properties);
    }

    for (auto const &[id, encoder] : _encoders)
    {
        auto properties = drmModeObjectGetProperties(_deviceFd, id, DRM_MODE_OBJECT_ENCODER);
        if (!properties) continue;

        for (unsigned int i = 0; i < properties->count_props; i++)
        {
            auto prop = drmModeGetProperty(_deviceFd, properties->props[i]);
            if (!prop) continue;
            encoder->props.insert({std::string(prop->name), {.spec = prop, .value = properties->prop_values[i]}});
        }
        drmModeFreeObjectProperties(properties);
    }
}

bool DRMFrameGrabber::getFramebuffers()
{
	for (auto const &[id, plane] : _planes)
	{
		drmModeFB2Ptr fb = drmModeGetFB2(_deviceFd, plane->fb_id);
		qCDebug(grabber_screen_flow) << fb;
		if (fb == nullptr) continue;

		if (fb->handles[0] == 0)
		{
			setInError("Not able to acquire framebuffer handles. Screen capture not possible. Check permissions.");
			drmModeFreeFB2(fb);
			return false;
		}
		_framebuffers.insert({plane->fb_id, fb});
	}
	return true;
}

bool DRMFrameGrabber::getScreenInfo()
{
	if (_deviceFd < 0)
	{
		this->setInError("DRM device not open");
		return false;
	}

	setDrmClientCaps();

	drmModeResPtr resources = drmModeGetResources(_deviceFd);
	if (!resources)
	{
		this->setInError(QString("Unable to get DRM resources on %1").arg(getDeviceName()));
		return false;
	}

	enumerateConnectorsAndEncoders(resources);
	findActiveCrtc(resources);

	drmModePlaneResPtr planeResources = drmModeGetPlaneResources(_deviceFd);
	if (planeResources)
	{
		findPrimaryPlane(planeResources);
		drmModeFreePlaneResources(planeResources);
	}
	else
	{
		Debug(_log, "drmModeGetPlaneResources returned NULL or failed: %s", strerror(errno));
	}

	getDrmObjectProperties();
	if (!getFramebuffers())
	{
		return false;
	}

	drmModeFreeResources(resources);

	qCDebug(grabber_screen_flow) << "Framebuffer count:" << _framebuffers.size();

	return !_framebuffers.empty();
}

QJsonArray DRMFrameGrabber::getInputDeviceDetails() const
{
	// Find framebuffer devices 0-9
	QDir deviceDirectory(DRM_DIR_NAME);
	QStringList deviceFilter(QString("%1%2").arg(DRM_PRIMARY_MINOR_NAME).arg('?'));
	deviceDirectory.setNameFilters(deviceFilter);
	deviceDirectory.setSorting(QDir::Name);
	QFileInfoList deviceFiles = deviceDirectory.entryInfoList(QDir::System);

	QJsonArray video_inputs;
	for (const auto &deviceFile : deviceFiles)
	{
		QString const fileName = deviceFile.fileName();
		int deviceIdx = fileName.right(1).toInt();
		QString device = deviceFile.absoluteFilePath();
		qCDebug(grabber_screen_properties) << "DRM device [" << device << "] found";

		QSize screenSize = getScreenSize(device);
		//Only add devices with a valid screen size, i.e. where a monitor is connected
		if ( !screenSize.isEmpty() )
		{
			QJsonObject input;

			QString displayName;
			displayName = QString("Output %1").arg(deviceIdx);

			input["name"] = displayName;
			input["inputIdx"] = deviceIdx;

			QJsonArray formats;
			QJsonObject format;

			QJsonArray resolutionArray;

			QJsonObject resolution;

			resolution["width"] = screenSize.width();
			resolution["height"] = screenSize.height();
			resolution["fps"] = getFpsSupported();

			resolutionArray.append(resolution);

			format["resolutions"] = resolutionArray;
			formats.append(format);

			input["formats"] = formats;
			video_inputs.append(input);
		}
	}

	return video_inputs;
}

QJsonObject DRMFrameGrabber::discover(const QJsonObject & /*params*/)
{
	if (!isAvailable(false))
	{
		return {};
	}

	QJsonObject inputsDiscovered;

	QJsonArray const video_inputs = getInputDeviceDetails();
	if (video_inputs.isEmpty())
	{
		qCDebug(grabber_screen_properties) << "No displays found to capture from!";
		return {};
	}

	inputsDiscovered["device"] = "drm";
	inputsDiscovered["device_name"] = "DRM";
	inputsDiscovered["type"] = "screen";
	inputsDiscovered["video_inputs"] = video_inputs;

	QJsonObject defaults;
	QJsonObject video_inputs_default;
	QJsonObject resolution_default;

	resolution_default["fps"] = _fps;
	video_inputs_default["resolution"] = resolution_default;
	video_inputs_default["inputIdx"] = 0;
	defaults["video_input"] = video_inputs_default;
	inputsDiscovered["default"] = defaults;

	return inputsDiscovered;
}

QDebug operator<<(QDebug dbg, const drmModeFB2* fb)
{
	QDebugStateSaver saver(dbg);
	dbg.nospace();

	if (!fb)
	{
		dbg << "drmModeFB2Ptr(null)";
		return dbg;
	}

	dbg << "drmModeFB2("
		<< "id: " << fb->fb_id
		<< ", size: " << fb->width << "x" << fb->height
		<< ", DRM format: " << getDrmFormat(fb->pixel_format)
		<< ", DRM modifier: " << getDrmModifierName(fb->modifier)
		<< ", flags: " << Qt::hex << fb->flags << Qt::dec
		<< ", handles: " << fb->handles[0] << fb->handles[1] << fb->handles[2] << fb->handles[3]
		<< ", pitches: " << fb->pitches[0] << fb->pitches[1] << fb->pitches[2] << fb->pitches[3]
		<< ", offsets: " << fb->offsets[0] << fb->offsets[1] << fb->offsets[2] << fb->offsets[3]
		<< ")";

	return dbg;
}

QDebug operator<<(QDebug dbg, const drmModePlane* plane)
{
	QDebugStateSaver saver(dbg);
	dbg.nospace();

	if (!plane)
	{
		dbg << "drmModePlanePtr(null)";
		return dbg;
	}

	dbg << "drmModePlane("
		<< "id: " << plane->plane_id
		<< ", CRTC id: " << plane->crtc_id
		<< ", FB id: " << plane->fb_id
		<< ", pos: " << plane->x << "," << plane->y
		<< ", CRTC pos: " << plane->crtc_x << "," << plane->crtc_y
		<< ", possible CRTCs: " << Qt::hex << plane->possible_crtcs << Qt::dec
		<< ", gamma size: " << plane->gamma_size
		<< ", format count: " << plane->count_formats
		<< ")";

	return dbg;
}
