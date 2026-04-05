#pragma once

// C
#include <cerrno>
#include <cstring>

// Unix
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

// DRM
#include <drm_fourcc.h>
#include <xf86drmMode.h>
#include <xf86drm.h>

// Utils
#include <utils/ColorRgb.h>
#include <utils/Logger.h>

// ─────────────────────────────────────────────────────────────────────────────

class DRMWritebackConverter
{
public:
	DRMWritebackConverter() = default;
	~DRMWritebackConverter() { destroy(); }

	DRMWritebackConverter(const DRMWritebackConverter&) = delete;
	DRMWritebackConverter& operator=(const DRMWritebackConverter&) = delete;

	static bool isSupported(int deviceFd)
	{
		// We need ATOMIC + WRITEBACK caps to even enumerate writeback connectors
		if (drmSetClientCap(deviceFd, DRM_CLIENT_CAP_ATOMIC, 1) != 0)
			return false;
		if (drmSetClientCap(deviceFd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1) != 0)
			return false;

		drmModeResPtr res = drmModeGetResources(deviceFd);
		if (!res)
			return false;

		bool found = false;
		for (int i = 0; i < res->count_connectors && !found; ++i)
		{
			drmModeConnectorPtr c = drmModeGetConnector(deviceFd, res->connectors[i]);
			if (c)
			{
				found = (c->connector_type == DRM_MODE_CONNECTOR_WRITEBACK);
				drmModeFreeConnector(c);
			}
		}

		drmModeFreeResources(res);
		return found;
	}

	bool init(int deviceFd, uint32_t crtcId, int crtcIndex, uint32_t w, uint32_t h, Logger* log)
	{
		_deviceFd  = deviceFd;
		_crtcId    = crtcId;
		_crtcIndex = crtcIndex;
		_width     = w;
		_height    = h;

		if (!findWritebackConnector(log))
			return false;
		if (!resolvePropertyIds(log))
			return false;
		if (!createDstFramebuffer(log))
			return false;
		if (!mmapDstBuffer(log))
			return false;

		_ready = true;
		Info(log, "DRMWritebackConverter ready (%ux%u, connector %u)", w, h, _connectorId);
		return true;
	}

	uint8_t* capture(Logger* log)
	{
		int outFenceFd = -1;

		drmModeAtomicReqPtr req = drmModeAtomicAlloc();
		if (!req)
		{
			Error(log, "DRMWritebackConverter: drmModeAtomicAlloc failed");
			return nullptr;
		}

		drmModeAtomicAddProperty(req, _connectorId, _propWritebackFbId, _dstFbId);
		drmModeAtomicAddProperty(req, _connectorId, _propWritebackOutFencePtr, reinterpret_cast<uint64_t>(&outFenceFd));

		const int flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
		if (drmModeAtomicCommit(_deviceFd, req, flags, nullptr) != 0)
		{
			Error(log, "DRMWritebackConverter: atomic commit failed: %s", strerror(errno));
			drmModeAtomicFree(req);
			return nullptr;
		}
		drmModeAtomicFree(req);

		if (!waitPageFlip(log))
			return nullptr;

		if (outFenceFd >= 0)
		{
			if (!waitFence(outFenceFd, log))
			{
				close(outFenceFd);
				return nullptr;
			}
			close(outFenceFd);
		}

		return _mmapPtr;
	}

	uint32_t stride() const { return _dstPitch; }
	uint32_t width()  const { return _width;  }
	uint32_t height() const { return _height; }

	bool isReady() const { return _ready; }

	void destroy()
	{
		if (_mmapPtr)
		{
			munmap(_mmapPtr, _dstBoSize);
			_mmapPtr = nullptr;
		}

		if (_dstFbId)
		{
			drmModeRmFB(_deviceFd, _dstFbId);
			_dstFbId = 0;
		}

		if (_dstBoHandle)
		{
			drm_mode_destroy_dumb arg{};
			arg.handle = _dstBoHandle;
			drmIoctl(_deviceFd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
			_dstBoHandle = 0;
		}

		_connectorId  = 0;
		_propWritebackFbId = 0;
		_propWritebackOutFencePtr = 0;
		_crtcId = 0;
		_deviceFd = -1;
		_ready = false;
	}

private:

	int      _deviceFd  { -1 };
	uint32_t _crtcId    { 0 };
	int      _crtcIndex { -1 };
	uint32_t _width     { 0 };
	uint32_t _height    { 0 };
	bool     _ready     { false };

	uint32_t _connectorId              { 0 };
	uint32_t _propWritebackFbId        { 0 };
	uint32_t _propWritebackOutFencePtr { 0 };

	uint32_t _dstFbId    { 0 };
	uint32_t _dstBoHandle{ 0 };
	uint32_t _dstPitch   { 0 };
	uint64_t _dstBoSize  { 0 };
	uint8_t* _mmapPtr    { nullptr };


	bool findWritebackConnector(Logger* log)
	{
		drmModeResPtr res = drmModeGetResources(_deviceFd);
		if (!res)
		{
			Error(log, "DRMWritebackConverter: drmModeGetResources failed");
			return false;
		}

		for (int i = 0; i < res->count_connectors; ++i)
		{
			drmModeConnectorPtr c = drmModeGetConnector(_deviceFd, res->connectors[i]);
			if (!c) continue;

			if (c->connector_type == DRM_MODE_CONNECTOR_WRITEBACK)
			{
				_connectorId = c->connector_id;
				drmModeFreeConnector(c);
				break;
			}
			drmModeFreeConnector(c);
		}

		drmModeFreeResources(res);

		if (_connectorId == 0)
		{
			Error(log, "DRMWritebackConverter: no writeback connector found");
			return false;
		}

		return true;
	}

	bool resolvePropertyIds(Logger* log)
	{
		_propWritebackFbId = getPropId(DRM_MODE_OBJECT_CONNECTOR, _connectorId, "WRITEBACK_FB_ID");
		_propWritebackOutFencePtr = getPropId(DRM_MODE_OBJECT_CONNECTOR, _connectorId, "WRITEBACK_OUT_FENCE_PTR");

		if (_propWritebackFbId == 0 || _propWritebackOutFencePtr == 0)
		{
			Error(log, "DRMWritebackConverter: required writeback properties not found  (WRITEBACK_FB_ID=%u, WRITEBACK_OUT_FENCE_PTR=%u)", _propWritebackFbId, _propWritebackOutFencePtr);
			return false;
		}

		return true;
	}

	bool createDstFramebuffer(Logger* log)
	{
		drm_mode_create_dumb arg{};
		arg.width  = _width;
		arg.height = _height;
		arg.bpp    = 32;

		if (drmIoctl(_deviceFd, DRM_IOCTL_MODE_CREATE_DUMB, &arg) != 0)
		{
			Error(log, "DRMWritebackConverter: DRM_IOCTL_MODE_CREATE_DUMB failed: %s", strerror(errno));
			return false;
		}

		_dstBoHandle = arg.handle;
		_dstPitch    = arg.pitch;
		_dstBoSize   = arg.size;

		uint32_t handles[4]   = { arg.handle, 0, 0, 0 };
		uint32_t pitches[4]   = { arg.pitch,  0, 0, 0 };
		uint32_t offsets[4]   = { 0, 0, 0, 0 };
		uint64_t modifiers[4] = { DRM_FORMAT_MOD_LINEAR, 0, 0, 0 };

		if (drmModeAddFB2WithModifiers(_deviceFd, _width, _height, DRM_FORMAT_ARGB8888, handles, pitches, offsets, modifiers, &_dstFbId, DRM_MODE_FB_MODIFIERS) != 0)
		{
			Error(log, "DRMWritebackConverter: drmModeAddFB2WithModifiers failed: %s", strerror(errno));
			return false;
		}

		return true;
	}

	bool mmapDstBuffer(Logger* log)
	{
		drm_mode_map_dumb mapArg{};
		mapArg.handle = _dstBoHandle;
		if (drmIoctl(_deviceFd, DRM_IOCTL_MODE_MAP_DUMB, &mapArg) != 0)
		{
			Error(log, "DRMWritebackConverter: DRM_IOCTL_MODE_MAP_DUMB failed: %s", strerror(errno));
			return false;
		}

		_mmapPtr = reinterpret_cast<uint8_t*>(mmap(nullptr, _dstBoSize, PROT_READ, MAP_SHARED, _deviceFd, static_cast<off_t>(mapArg.offset)));

		if (_mmapPtr == MAP_FAILED)
		{
			_mmapPtr = nullptr;
			Error(log, "DRMWritebackConverter: mmap failed: %s", strerror(errno));
			return false;
		}
		return true;
	}

	bool waitPageFlip(Logger* log) const
	{
		drmEventContext evctx{};
		evctx.version = DRM_EVENT_CONTEXT_VERSION;
		evctx.page_flip_handler = pageFlipHandler;

		struct pollfd pfd{ _deviceFd, POLLIN, 0 };

		if (poll(&pfd, 1, 100) <= 0)
		{
			Error(log, "DRMWritebackConverter: poll timeout waiting for page-flip");
			return false;
		}

		int ret = 0;
		do
		{
			ret = drmHandleEvent(_deviceFd, &evctx);
		} while (ret == 0);
		return true;
	}

	static bool waitFence(int fd, Logger* log)
	{
		struct pollfd pfd{ fd, POLLIN, 0 };
		if (poll(&pfd, 1, 100) <= 0)
		{
			Error(log, "DRMWritebackConverter: poll timeout on out-fence");
			return false;
		}
		return true;
	}

	static void pageFlipHandler(int /*fd*/, unsigned int /*frame*/, unsigned int /*sec*/, unsigned int /*usec*/, void* /*data*/) {}

	uint32_t getPropId(uint32_t objType, uint32_t objId, const char* name) const
	{
		uint32_t result = 0;
		drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_deviceFd, objId, objType);
		if (!props)
			return 0;

		for (unsigned int i = 0; i < props->count_props; ++i)
		{
			drmModePropertyPtr prop = drmModeGetProperty(_deviceFd, props->props[i]);
			if (prop)
			{
				if (strcmp(prop->name, name) == 0)
				{
					result = props->props[i];
				}

				drmModeFreeProperty(prop);

				if (result)
					break;
			}
		}

		drmModeFreeObjectProperties(props);
		return result;
	}
};
