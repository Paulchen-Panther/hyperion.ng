#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include <hyperion/Hyperion.h>
#include <hyperion/HyperionIManager.h>

#include <QDirIterator>
#include <QFileInfo>

#include "grabber/V4L2Grabber.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_CAP_META_CAPTURE
	#define V4L2_CAP_META_CAPTURE 0x00800000 // Specified in kernel header v4.16. Required for backward compatibility.
#endif

// Constants
namespace { const bool verbose = false; }

static PixelFormat GetPixelFormat(const unsigned int format)
{
	if (format == V4L2_PIX_FMT_RGB32) return PixelFormat::RGB32;
	if (format == V4L2_PIX_FMT_RGB24) return PixelFormat::BGR24;
	if (format == V4L2_PIX_FMT_YUYV) return PixelFormat::YUYV;
	if (format == V4L2_PIX_FMT_UYVY) return PixelFormat::UYVY;
	if (format == V4L2_PIX_FMT_NV12) return  PixelFormat::NV12;
	if (format == V4L2_PIX_FMT_YUV420) return  PixelFormat::I420;
#ifdef HAVE_JPEG_DECODER
	if (format == V4L2_PIX_FMT_MJPEG) return  PixelFormat::MJPEG;
#endif
	return PixelFormat::NO_CHANGE;
};

V4L2Grabber::V4L2Grabber()
	: Grabber("V4L2")
	, _currentDeviceName("none")
	, _newDeviceName("none")
	, _ioMethod(IO_METHOD_MMAP)
	, _fileDescriptor(-1)
	, _pixelFormat(PixelFormat::NO_CHANGE)
	, _pixelFormatConfig(PixelFormat::NO_CHANGE)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _noSignalCounterThreshold(40)
	, _noSignalThresholdColor(ColorRgb{0,0,0})
	, _cecDetectionEnabled(true)
	, _cecStandbyActivated(false)
	, _signalDetectionEnabled(true)
	, _noSignalDetected(false)
	, _noSignalCounter(0)
	, _x_frac_min(0.25)
	, _y_frac_min(0.25)
	, _x_frac_max(0.75)
	, _y_frac_max(0.75)
	, _streamNotifier(nullptr)
	, _initialized(false)
	, _reload(false)
{
}

V4L2Grabber::~V4L2Grabber()
{
	uninit();
}

void V4L2Grabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log,"Uninit grabber: %s", QSTRING_CSTR(_newDeviceName));
		stop();
	}
}

bool V4L2Grabber::init()
{
	if (!_initialized)
	{
		bool noDeviceName = _currentDeviceName.compare("none", Qt::CaseInsensitive) == 0 || _currentDeviceName.compare("auto", Qt::CaseInsensitive) == 0;

		// enumerate the video capture devices on the user's system
		enumVideoCaptureDevices();

		if(noDeviceName)
			return false;

		if(!_deviceProperties.contains(_currentDeviceName))
		{
			Debug(_log, "Configured device at '%s' is not available.", QSTRING_CSTR(_currentDeviceName));
			_currentDeviceName = "none";
			return false;
		}

		bool valid = false;
		for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
			if (i.key() == _currentDeviceName && valid == false)
				for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
					if (y.key() == _input && valid == false)
						for (auto enc = y.value().encodingFormats.begin(); enc != y.value().encodingFormats.end(); enc++)
								if(enc.key() == _pixelFormat && enc.value().width == _width && enc.value().height == _height && valid == false)
									for (auto fps = enc.value().framerates.begin(); fps != enc.value().framerates.end(); fps++)
										if(*fps == _fps && valid == false)
											valid = true;

		if (!valid)
		{
			Debug(_log, "Configured device at '%s' is not available.", QSTRING_CSTR(_currentDeviceName));
			_currentDeviceName = "none";
			return false;
		}

		bool opened = false;
		try
		{
			if (open_device())
			{
				opened = true;
				init_device(_videoStandard);
				_initialized = true;
			}
		}
		catch(std::exception& e)
		{
			if (opened)
			{
				uninit_device();
				close_device();
			}

			Error(_log, "V4l2 init failed (%s)", e.what());
		}
	}

	return _initialized;
}

void V4L2Grabber::enumVideoCaptureDevices()
{
	QDirIterator it("/sys/class/video4linux/", QDirIterator::NoIteratorFlags);
	_deviceProperties.clear();
	while(it.hasNext())
	{
		//_v4lDevices
		QString dev = it.next();
		if (it.fileName().startsWith("video"))
		{
			QString devName = "/dev/" + it.fileName();
			int fd = open(QSTRING_CSTR(devName), O_RDWR | O_NONBLOCK, 0);

			if (fd < 0)
			{
				throw_errno_exception("Cannot open '" + devName + "'");
				continue;
			}

			struct v4l2_capability cap;
			CLEAR(cap);

			if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
			{
				throw_errno_exception("'" + devName + "' is no V4L2 device");
				close(fd);
				continue;
			}

			if (cap.device_caps & V4L2_CAP_META_CAPTURE) // this device has bit 23 set (and bit 1 reset), so it doesn't have capture.
			{
				close(fd);
				continue;
			}

			// get the current settings
			struct v4l2_format fmt;
			CLEAR(fmt);

			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
			{
				close(fd);
				continue;
			}

			V4L2Grabber::DeviceProperties properties;

			// collect available device inputs (index & name)
			struct v4l2_input input;
			CLEAR(input);

			input.index = 0;
			while (xioctl(fd, VIDIOC_ENUMINPUT, &input) >= 0)
			{
				V4L2Grabber::DeviceProperties::InputProperties inputProperties;
				inputProperties.inputName = QString((char*)input.name);

				// Enumerate video standards
				struct v4l2_standard standard;
				CLEAR(standard);

				standard.index = 0;
				while (xioctl(fd, VIDIOC_ENUMSTD, &standard) >= 0)
				{
					if (standard.id & input.std)
					{
						if (standard.id == V4L2_STD_PAL)
							inputProperties.standards.append(VideoStandard::PAL);
						else if (standard.id == V4L2_STD_NTSC)
							inputProperties.standards.append(VideoStandard::NTSC);
						else if (standard.id == V4L2_STD_SECAM)
							inputProperties.standards.append(VideoStandard::SECAM);
					}

					standard.index++;
				}

				// Enumerate pixel formats
				struct v4l2_fmtdesc desc;
				CLEAR(desc);

				desc.index = 0;
				desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				while (xioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0)
				{
					PixelFormat encodingFormat = GetPixelFormat(desc.pixelformat);
					if (encodingFormat != PixelFormat::NO_CHANGE)
					{
						V4L2Grabber::DeviceProperties::InputProperties::EncodingProperties encodingProperties;

						// Enumerate frame sizes and frame rates
						struct v4l2_frmsizeenum frmsizeenum;
						CLEAR(frmsizeenum);

						frmsizeenum.index = 0;
						frmsizeenum.pixel_format = desc.pixelformat;
						while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) >= 0)
						{
							switch (frmsizeenum.type)
							{
								case V4L2_FRMSIZE_TYPE_DISCRETE:
								{
									encodingProperties.width	= frmsizeenum.discrete.width;
									encodingProperties.height	= frmsizeenum.discrete.height;
									enumFrameIntervals(encodingProperties.framerates, fd, desc.pixelformat, frmsizeenum.discrete.width, frmsizeenum.discrete.height);
								}
								break;
								case V4L2_FRMSIZE_TYPE_CONTINUOUS:
								case V4L2_FRMSIZE_TYPE_STEPWISE: // We do not take care of V4L2_FRMSIZE_TYPE_CONTINUOUS or V4L2_FRMSIZE_TYPE_STEPWISE
									break;
							}

							inputProperties.encodingFormats.insert(encodingFormat, encodingProperties);
							frmsizeenum.index++;
						}

						// Failsafe: In case VIDIOC_ENUM_FRAMESIZES fails, insert current heigth, width and fps.
						if (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1)
						{
							encodingProperties.width	= fmt.fmt.pix.width;
							encodingProperties.height	= fmt.fmt.pix.height;
							enumFrameIntervals(encodingProperties.framerates, fd, desc.pixelformat, encodingProperties.width, encodingProperties.height);
							inputProperties.encodingFormats.insert(encodingFormat, encodingProperties);
						}
					}

					desc.index++;
				}

				properties.inputs.insert(input.index, inputProperties);
				input.index++;
			}

			if (close(fd) < 0) continue;

			QFile devNameFile(dev+"/name");
			if (devNameFile.exists())
			{
				devNameFile.open(QFile::ReadOnly);
				devName = devNameFile.readLine();
				devName = devName.trimmed();
				properties.name = devName;
				devNameFile.close();
			}

			_deviceProperties.insert("/dev/"+it.fileName(), properties);
		}
    }
}

bool V4L2Grabber::start()
{
	try
	{
		if (init() && _streamNotifier != nullptr && !_streamNotifier->isEnabled())
		{
			_streamNotifier->setEnabled(true);
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch(std::exception& e)
	{
		Error(_log, "start failed (%s)", e.what());
	}

	return false;
}

void V4L2Grabber::stop()
{
	if (_streamNotifier != nullptr && _streamNotifier->isEnabled())
	{
		stop_capturing();
		_streamNotifier->setEnabled(false);
		uninit_device();
		close_device();
		_initialized = false;
		_deviceProperties.clear();
		Info(_log, "Stopped");
	}
}

bool V4L2Grabber::open_device()
{
	struct stat st;

	if (-1 == stat(QSTRING_CSTR(_currentDeviceName), &st))
	{
		throw_errno_exception("Cannot identify '" + _currentDeviceName + "'");
		return false;
	}

	if (!S_ISCHR(st.st_mode))
	{
		throw_exception("'" + _currentDeviceName + "' is no device");
		return false;
	}

	_fileDescriptor = open(QSTRING_CSTR(_currentDeviceName), O_RDWR | O_NONBLOCK, 0);

	if (-1 == _fileDescriptor)
	{
		throw_errno_exception("Cannot open '" + _currentDeviceName + "'");
		return false;
	}

	// create the notifier for when a new frame is available
	_streamNotifier = new QSocketNotifier(_fileDescriptor, QSocketNotifier::Read);
	_streamNotifier->setEnabled(false);
	connect(_streamNotifier, &QSocketNotifier::activated, this, &V4L2Grabber::read_frame);
	return true;
}

void V4L2Grabber::close_device()
{
	if (-1 == close(_fileDescriptor))
	{
		throw_errno_exception("close");
		return;
	}

	_fileDescriptor = -1;

	delete _streamNotifier;
	_streamNotifier = nullptr;
}

void V4L2Grabber::init_read(unsigned int buffer_size)
{
	_buffers.resize(1);

	_buffers[0].length = buffer_size;
	_buffers[0].start = malloc(buffer_size);

	if (!_buffers[0].start)
	{
		throw_exception("Out of memory");
		return;
	}
}

void V4L2Grabber::init_mmap()
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _currentDeviceName + "' does not support memory mapping");
			return;
		}
		else
		{
			throw_errno_exception("VIDIOC_REQBUFS");
			return;
		}
	}

	if (req.count < 2)
	{
		throw_exception("Insufficient buffer memory on " + _currentDeviceName);
		return;
	}

	_buffers.resize(req.count);

	for (size_t n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;

		if (-1 == xioctl(VIDIOC_QUERYBUF, &buf))
		{
			throw_errno_exception("VIDIOC_QUERYBUF");
			return;
		}

		_buffers[n_buffers].length = buf.length;
		_buffers[n_buffers].start = mmap(NULL /* start anywhere */,
						buf.length,
						PROT_READ | PROT_WRITE /* required */,
						MAP_SHARED /* recommended */,
						_fileDescriptor, buf.m.offset
					);

		if (MAP_FAILED == _buffers[n_buffers].start)
		{
			throw_errno_exception("mmap");
			return;
		}
	}
}

void V4L2Grabber::init_userp(unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count  = 4;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _currentDeviceName + "' does not support user pointer");
			return;
		}
		else
		{
			throw_errno_exception("VIDIOC_REQBUFS");
			return;
		}
	}

	_buffers.resize(4);

	for (size_t n_buffers = 0; n_buffers < 4; ++n_buffers)
	{
		_buffers[n_buffers].length = buffer_size;
		_buffers[n_buffers].start = malloc(buffer_size);

		if (!_buffers[n_buffers].start)
		{
			throw_exception("Out of memory");
			return;
		}
	}
}

void V4L2Grabber::init_device(VideoStandard videoStandard)
{
	struct v4l2_capability cap;
	CLEAR(cap);

	if (-1 == xioctl(VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _currentDeviceName + "' is no V4L2 device");
			return;
		}
		else
		{
			throw_errno_exception("VIDIOC_QUERYCAP");
			return;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		throw_exception("'" + _currentDeviceName + "' is no video capture device");
		return;
	}

	switch (_ioMethod)
	{
		case IO_METHOD_READ:
		{
			if (!(cap.capabilities & V4L2_CAP_READWRITE))
			{
				throw_exception("'" + _currentDeviceName + "' does not support read i/o");
				return;
			}
		}
		break;

		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
		{
			if (!(cap.capabilities & V4L2_CAP_STREAMING))
			{
				throw_exception("'" + _currentDeviceName + "' does not support streaming i/o");
				return;
			}
		}
		break;
	}


	/* Select video input, video standard and tune here. */

	struct v4l2_cropcap cropcap;
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(VIDIOC_CROPCAP, &cropcap))
	{
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
				case EINVAL: /* Cropping not supported. */
				default: /* Errors ignored. */
					break;
			}
		}
	}
	else
	{
		/* Errors ignored. */
	}

	// set input if needed and supported
	struct v4l2_input v4l2Input;
	CLEAR(v4l2Input);
	v4l2Input.index = _input;

	if (_input >= 0 && 0 == xioctl(VIDIOC_ENUMINPUT, &v4l2Input))
	{
		(-1 == xioctl(VIDIOC_S_INPUT, &_input))
		?	Debug(_log, "Input settings not supported.")
		:	Debug(_log, "Set device input to: %s", v4l2Input.name);
	}

	// set the video standard if needed and supported
	struct v4l2_standard standard;
	CLEAR(standard);

	if (-1 != xioctl(VIDIOC_ENUMSTD, &standard))
	{
		switch (videoStandard)
		{
			case VideoStandard::PAL:
			{
				standard.id = V4L2_STD_PAL;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=PAL");
			}
			break;

			case VideoStandard::NTSC:
			{
				standard.id = V4L2_STD_NTSC;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=NTSC");
			}
			break;

			case VideoStandard::SECAM:
			{
				standard.id = V4L2_STD_SECAM;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=SECAM");
			}
			break;

			case VideoStandard::NO_CHANGE:
			default:
				// No change to device settings
				break;
		}
	}

	// get the current settings
	struct v4l2_format fmt;
	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(VIDIOC_G_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_G_FMT");
		return;
	}

	// set the requested pixel format
	switch (_pixelFormat)
	{
		case PixelFormat::RGB32:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
		break;

		case PixelFormat::BGR24:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
		break;

		case PixelFormat::YUYV:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		break;

		case PixelFormat::UYVY:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		break;

		case PixelFormat::NV12:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
		break;

		case PixelFormat::I420:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		break;

#ifdef HAVE_JPEG_DECODER
		case PixelFormat::MJPEG:
		{
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
			fmt.fmt.pix.field       = V4L2_FIELD_ANY;
		}
		break;
#endif

		case PixelFormat::NO_CHANGE:
		default:
			// No change to device settings
			break;
	}

	// set custom resolution for width and height if they are not zero
	if(_width != 0 && _height != 0)
	{
		fmt.fmt.pix.width = _width;
		fmt.fmt.pix.height = _height;
	}

	// set the settings
	if (-1 == xioctl(VIDIOC_S_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_S_FMT");
		return;
	}

	// initialize current width and height
	_width = fmt.fmt.pix.width;
	_height = fmt.fmt.pix.height;

	// display the used width and height
	Debug(_log, "Set resolution to width=%d height=%d", _width, _height );

	// Trying to set frame rate
	struct v4l2_streamparm streamparms;
	CLEAR(streamparms);

	streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// Check that the driver knows about framerate get/set
	if (xioctl(VIDIOC_G_PARM, &streamparms) >= 0)
	{
		// Check if the device is able to accept a capture framerate set.
		if (streamparms.parm.capture.capability == V4L2_CAP_TIMEPERFRAME)
		{
			streamparms.parm.capture.timeperframe.numerator = 1;
			streamparms.parm.capture.timeperframe.denominator = _fps;
			(-1 == xioctl(VIDIOC_S_PARM, &streamparms))
			?	Debug(_log, "Frame rate settings not supported.")
			:	Debug(_log, "Set framerate to %d fps", streamparms.parm.capture.timeperframe.denominator);
		}
	}

	// set the line length
	_lineLength = fmt.fmt.pix.bytesperline;

	// check pixel format and frame size
	switch (fmt.fmt.pix.pixelformat)
	{
		case V4L2_PIX_FMT_RGB32:
		{
			_pixelFormat = PixelFormat::RGB32;
			_frameByteSize = _width * _height * 4;
			Debug(_log, "Pixel format=RGB32");
		}
		break;

		case V4L2_PIX_FMT_RGB24:
		{
			_pixelFormat = PixelFormat::BGR24;
			_frameByteSize = _width * _height * 3;
			Debug(_log, "Pixel format=BGR24");
		}
		break;


		case V4L2_PIX_FMT_YUYV:
		{
			_pixelFormat = PixelFormat::YUYV;
			_frameByteSize = _width * _height * 2;
			Debug(_log, "Pixel format=YUYV");
		}
		break;

		case V4L2_PIX_FMT_UYVY:
		{
			_pixelFormat = PixelFormat::UYVY;
			_frameByteSize = _width * _height * 2;
			Debug(_log, "Pixel format=UYVY");
		}
		break;

		case V4L2_PIX_FMT_NV12:
		{
			_pixelFormat = PixelFormat::NV12;
			_frameByteSize = (_width * _height * 6) / 4;
			Debug(_log, "Pixel format=NV12");
		}
		break;

		case V4L2_PIX_FMT_YUV420:
		{
			_pixelFormat = PixelFormat::I420;
			_frameByteSize = (_width * _height * 6) / 4;
			Debug(_log, "Pixel format=I420");
		}
		break;

#ifdef HAVE_JPEG_DECODER
		case V4L2_PIX_FMT_MJPEG:
		{
			_pixelFormat = PixelFormat::MJPEG;
			Debug(_log, "Pixel format=MJPEG");
		}
		break;
#endif

		default:
#ifdef HAVE_JPEG_DECODER
			throw_exception("Only pixel formats RGB32, BGR24, YUYV, UYVY, NV12, I420 and MJPEG are supported");
#else
			throw_exception("Only pixel formats RGB32, BGR24, YUYV, UYVY, NV12 and I420 are supported");
#endif
		return;
	}

	switch (_ioMethod)
	{
		case IO_METHOD_READ:
			init_read(fmt.fmt.pix.sizeimage);
		break;

		case IO_METHOD_MMAP:
			init_mmap();
		break;

		case IO_METHOD_USERPTR:
			init_userp(fmt.fmt.pix.sizeimage);
		break;
	}
}

void V4L2Grabber::uninit_device()
{
	switch (_ioMethod)
	{
		case IO_METHOD_READ:
			free(_buffers[0].start);
		break;

		case IO_METHOD_MMAP:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
				if (-1 == munmap(_buffers[i].start, _buffers[i].length))
				{
					throw_errno_exception("munmap");
					return;
				}
		}
		break;

		case IO_METHOD_USERPTR:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
				free(_buffers[i].start);
		}
		break;
	}

	_buffers.resize(0);
}

void V4L2Grabber::start_capturing()
{
	switch (_ioMethod)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;

		case IO_METHOD_MMAP:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
			{
				struct v4l2_buffer buf;

				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return;
				}
			}
			v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(VIDIOC_STREAMON, &type))
			{
				throw_errno_exception("VIDIOC_STREAMON");
				return;
			}
			break;
		}
		case IO_METHOD_USERPTR:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
			{
				struct v4l2_buffer buf;

				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_USERPTR;
				buf.index = i;
				buf.m.userptr = (unsigned long)_buffers[i].start;
				buf.length = _buffers[i].length;

				if (-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return;
				}
			}
			v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(VIDIOC_STREAMON, &type))
			{
				throw_errno_exception("VIDIOC_STREAMON");
				return;
			}
			break;
		}
	}
}

void V4L2Grabber::stop_capturing()
{
	enum v4l2_buf_type type;

	switch (_ioMethod)
	{
		case IO_METHOD_READ:
			break; /* Nothing to do. */

		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
		{
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ErrorIf((xioctl(VIDIOC_STREAMOFF, &type) == -1), _log, "VIDIOC_STREAMOFF  error code  %d, %s", errno, strerror(errno));
		}
		break;
	}
}

int V4L2Grabber::read_frame()
{
	bool rc = false;

	try
	{
		struct v4l2_buffer buf;

		switch (_ioMethod)
		{
			case IO_METHOD_READ:
			{
				int size;
				if ((size = read(_fileDescriptor, _buffers[0].start, _buffers[0].length)) == -1)
				{
					switch (errno)
					{
						case EAGAIN:
							return 0;

						case EIO: /* Could ignore EIO, see spec. */
						default:
							throw_errno_exception("read");
						return 0;
					}
				}

				rc = process_image(_buffers[0].start, size);
			}
			break;

			case IO_METHOD_MMAP:
			{
				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;

				if (-1 == xioctl(VIDIOC_DQBUF, &buf))
				{
					switch (errno)
					{
						case EAGAIN:
							return 0;

						case EIO: /* Could ignore EIO, see spec. */
						default:
						{
							throw_errno_exception("VIDIOC_DQBUF");
							stop();
							enumVideoCaptureDevices();
						}
						return 0;
					}
				}

				assert(buf.index < _buffers.size());

				rc = process_image(_buffers[buf.index].start, buf.bytesused);

				if (-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return 0;
				}
			}
			break;

			case IO_METHOD_USERPTR:
			{
				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_USERPTR;

				if (-1 == xioctl(VIDIOC_DQBUF, &buf))
				{
					switch (errno)
					{
						case EAGAIN:
							return 0;

						case EIO: /* Could ignore EIO, see spec. */
						default:
						{
							throw_errno_exception("VIDIOC_DQBUF");
							stop();
							enumVideoCaptureDevices();
						}
						return 0;
					}
				}

				for (size_t i = 0; i < _buffers.size(); ++i)
				{
					if (buf.m.userptr == (unsigned long)_buffers[i].start && buf.length == _buffers[i].length)
					{
						break;
					}
				}

				rc = process_image((void *)buf.m.userptr, buf.bytesused);

				if (-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return 0;
				}
			}
			break;
		}
	}
	catch (std::exception& e)
	{
		emit readError(e.what());
		rc = false;
	}

	return rc ? 1 : 0;
}

bool V4L2Grabber::process_image(const void *p, int size)
{
	// We do want a new frame...
#ifdef HAVE_JPEG_DECODER
	if (size < _frameByteSize && _pixelFormat != PixelFormat::MJPEG)
#else
	if (size < _frameByteSize)
#endif
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		process_image(reinterpret_cast<const uint8_t *>(p), size);
		return true;
	}

	return false;
}

void V4L2Grabber::process_image(const uint8_t * data, int size)
{
	if (_cecDetectionEnabled && _cecStandbyActivated)
		return;

	Image<ColorRgb> image(_width, _height);

/* ----------------------------------------------------------
 * ----------- BEGIN of JPEG decoder related code -----------
 * --------------------------------------------------------*/

#ifdef HAVE_JPEG_DECODER
	if (_pixelFormat == PixelFormat::MJPEG)
	{
#endif
#ifdef HAVE_JPEG
		_decompress = new jpeg_decompress_struct;
		_error = new errorManager;

		_decompress->err = jpeg_std_error(&_error->pub);
		_error->pub.error_exit = &errorHandler;
		_error->pub.output_message = &outputHandler;

		jpeg_create_decompress(_decompress);

		if (setjmp(_error->setjmp_buffer))
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			return;
		}

		jpeg_mem_src(_decompress, const_cast<uint8_t*>(data), size);

		if (jpeg_read_header(_decompress, (bool) TRUE) != JPEG_HEADER_OK)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			return;
		}

		_decompress->scale_num = 1;
		_decompress->scale_denom = 1;
		_decompress->out_color_space = JCS_RGB;
		_decompress->dct_method = JDCT_IFAST;

		if (!jpeg_start_decompress(_decompress))
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			return;
		}

		if (_decompress->out_color_components != 3)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			return;
		}

		QImage imageFrame = QImage(_decompress->output_width, _decompress->output_height, QImage::Format_RGB888);

		int y = 0;
		while (_decompress->output_scanline < _decompress->output_height)
		{
			uchar *row = imageFrame.scanLine(_decompress->output_scanline);
			jpeg_read_scanlines(_decompress, &row, 1);
			y++;
		}

		jpeg_finish_decompress(_decompress);
		jpeg_destroy_decompress(_decompress);
		delete _decompress;
		delete _error;

		if (imageFrame.isNull() || _error->pub.num_warnings > 0)
			return;
#endif
#ifdef HAVE_TURBO_JPEG
		_decompress = tjInitDecompress();
		if (_decompress == nullptr)
			return;

		if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(data), size, &_width, &_height, &_subsamp) != 0)
		{
			tjDestroy(_decompress);
			return;
		}

		QImage imageFrame = QImage(_width, _height, QImage::Format_RGB888);
		if (tjDecompress2(_decompress, const_cast<uint8_t*>(data), size, imageFrame.bits(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
		{
			tjDestroy(_decompress);
			return;
		}

		tjDestroy(_decompress);

		if (imageFrame.isNull())
			return;
#endif
#ifdef HAVE_JPEG_DECODER
		QRect rect(_cropLeft, _cropTop, imageFrame.width() - _cropLeft - _cropRight, imageFrame.height() - _cropTop - _cropBottom);
		imageFrame = imageFrame.copy(rect);
		imageFrame = imageFrame.scaled(imageFrame.width() / _pixelDecimation, imageFrame.height() / _pixelDecimation,Qt::KeepAspectRatio);

		if ((image.width() != unsigned(imageFrame.width())) || (image.height() != unsigned(imageFrame.height())))
			image.resize(imageFrame.width(), imageFrame.height());

		for (int y=0; y<imageFrame.height(); ++y)
			for (int x=0; x<imageFrame.width(); ++x)
			{
				QColor inPixel(imageFrame.pixel(x,y));
				ColorRgb & outPixel = image(x,y);
				outPixel.red   = inPixel.red();
				outPixel.green = inPixel.green();
				outPixel.blue  = inPixel.blue();
			}
	}
	else
#endif

/* ----------------------------------------------------------
 * ------------ END of JPEG decoder related code ------------
 * --------------------------------------------------------*/

	_imageResampler.processImage(data, _width, _height, _lineLength, _pixelFormat, image);

	if (_signalDetectionEnabled)
	{
		// check signal (only in center of the resulting image, because some grabbers have noise values along the borders)
		bool noSignal = true;

		// top left
		unsigned xOffset  = image.width()  * _x_frac_min;
		unsigned yOffset  = image.height() * _y_frac_min;

		// bottom right
		unsigned xMax     = image.width()  * _x_frac_max;
		unsigned yMax     = image.height() * _y_frac_max;


		for (unsigned x = xOffset; noSignal && x < xMax; ++x)
		{
			for (unsigned y = yOffset; noSignal && y < yMax; ++y)
			{
				noSignal &= (ColorRgb&)image(x, y) <= _noSignalThresholdColor;
			}
		}

		if (noSignal)
		{
			++_noSignalCounter;
		}
		else
		{
			if (_noSignalCounter >= _noSignalCounterThreshold)
			{
				_noSignalDetected = true;
				Info(_log, "Signal detected");
			}

			_noSignalCounter = 0;
		}

		if ( _noSignalCounter < _noSignalCounterThreshold)
		{
			emit newFrame(image);
		}
		else if (_noSignalCounter == _noSignalCounterThreshold)
		{
			_noSignalDetected = false;
			Info(_log, "Signal lost");
		}
	}
	else
	{
		emit newFrame(image);
	}
}

int V4L2Grabber::xioctl(int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(_fileDescriptor, request, arg);
	}
	while (-1 == r && EINTR == errno);

	return r;
}

int V4L2Grabber::xioctl(int fileDescriptor, int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(fileDescriptor, request, arg);
	}
	while (r < 0 && errno == EINTR );

	return r;
}

void V4L2Grabber::setDevice(const QString& device)
{
	if (_currentDeviceName != device)
	{
		(_initialized)
		? _newDeviceName = device
		: _currentDeviceName = _newDeviceName = device;

		_reload = true;
	}
}

bool V4L2Grabber::setInput(int input)
{
	if(Grabber::setInput(input))
	{
		_reload = true;
		return true;
	}

	 return false;
}

bool V4L2Grabber::setWidthHeight(int width, int height)
{
	if(Grabber::setWidthHeight(width, height))
	{
		_reload = true;
		return true;
	}

	 return false;
}

void V4L2Grabber::setEncoding(QString enc)
{
	if(_pixelFormatConfig != parsePixelFormat(enc))
	{
		_pixelFormatConfig = parsePixelFormat(enc);
		if(_initialized)
		{
			Debug(_log,"Set hardware encoding to: %s", QSTRING_CSTR(enc.toUpper()));
			_reload = true;
		}
		else
			_pixelFormat = _pixelFormatConfig;
	}
}

void V4L2Grabber::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	if(_initialized)
		DebugIf(verbose, _log,"TODO: Set brightness to %i, contrast to %i, saturation to %i, hue to %i", brightness, contrast, saturation, hue);
}

void V4L2Grabber::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_noSignalThresholdColor.red   = uint8_t(255*redSignalThreshold);
	_noSignalThresholdColor.green = uint8_t(255*greenSignalThreshold);
	_noSignalThresholdColor.blue  = uint8_t(255*blueSignalThreshold);
	_noSignalCounterThreshold     = qMax(1, noSignalCounterThreshold);

	if(_signalDetectionEnabled)
		Info(_log, "Signal threshold set to: {%d, %d, %d}", _noSignalThresholdColor.red, _noSignalThresholdColor.green, _noSignalThresholdColor.blue );
}

void V4L2Grabber::setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax)
{
	// rainbow 16 stripes 0.47 0.2 0.49 0.8
	// unicolor: 0.25 0.25 0.75 0.75

	_x_frac_min = horizontalMin;
	_y_frac_min = verticalMin;
	_x_frac_max = horizontalMax;
	_y_frac_max = verticalMax;

	if(_signalDetectionEnabled)
		Info(_log, "Signal detection area set to: %f,%f x %f,%f", _x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max );
}

void V4L2Grabber::setSignalDetectionEnable(bool enable)
{
	if (_signalDetectionEnabled != enable)
	{
		_signalDetectionEnabled = enable;
		if(_initialized)
			Info(_log, "Signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void V4L2Grabber::setCecDetectionEnable(bool enable)
{
	if (_cecDetectionEnabled != enable)
	{
		_cecDetectionEnabled = enable;
		if(_initialized)
			Info(_log, QString("CEC detection is now %1").arg(enable ? "enabled" : "disabled").toLocal8Bit());
	}
}

bool V4L2Grabber::reload(bool force)
{
	if (_streamNotifier != nullptr && _streamNotifier->isEnabled() && (_reload || force))
	{
		Info(_log,"Reloading V4L2 Grabber");
		uninit();
		_pixelFormat = _pixelFormatConfig;
		_newDeviceName = _currentDeviceName;
		_reload = false;
		return start();
	}

	return false;
}

#if defined(ENABLE_CEC)

void V4L2Grabber::handleCecEvent(CECEvent event)
{
	switch (event)
	{
		case CECEvent::On  :
			Debug(_log,"CEC on event received");
			_cecStandbyActivated = false;
			return;
		case CECEvent::Off :
			Debug(_log,"CEC off event received");
			_cecStandbyActivated = true;
			return;
		default: break;
	}
}

#endif

QJsonArray V4L2Grabber::discover(const QJsonObject& params)
{
	DebugIf(verbose, _log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	enumVideoCaptureDevices();

	QJsonArray inputsDiscovered;
	for(auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
	{
		QJsonObject device, in;
		QJsonArray video_inputs, formats;

		device["device"] = it.key();
		device["device_name"] = _deviceProperties.value(it.key()).name;
		device["type"] = "v4l2";

		QMultiMap<QString, int> inputs = QMultiMap<QString, int>();
		for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
			if (i.key() == it.key())
				for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
					if (!inputs.contains(y.value().inputName, y.key()))
						inputs.insert(y.value().inputName, y.key());

		for (auto input = inputs.begin(); input != inputs.end(); input++)
		{
			in["name"] = input.key();
			in["inputIdx"] = input.value();

			QJsonArray standards;
			QList<VideoStandard> videoStandards = QList<VideoStandard>();
			for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
				if (i.key() == it.key())
					for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
						if (y.key() == input.value())
							for (auto std = y.value().standards.begin(); std != y.value().standards.end(); std++)
								if(!videoStandards.contains(*std))
									videoStandards << *std;

			for (auto standard : videoStandards)
				standards.append(VideoStandard2String(standard));

			if (!standards.isEmpty())
				in["standards"] = standards;

			QList<PixelFormat> encodingFormats = QList<PixelFormat>();
			for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
				if (i.key() == it.key())
					for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
						if (y.key() == input.value())
							for (auto enc = y.value().encodingFormats.begin(); enc != y.value().encodingFormats.end(); enc++)
								if (!encodingFormats.contains(enc.key()))
									encodingFormats << enc.key();

			for (auto encodingFormat : encodingFormats)
			{
				QJsonObject format;
				QJsonArray resolutionArray;

				format["format"] = pixelFormatToString(encodingFormat);

				QMultiMap<int, int> deviceResolutions = QMultiMap<int, int>();
				for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
					if (i.key() == it.key())
						for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
							if (y.key() == input.value())
								for (auto enc = y.value().encodingFormats.begin(); enc != y.value().encodingFormats.end(); enc++)
									if (enc.key() == encodingFormat && !deviceResolutions.contains(enc.value().width, enc.value().height))
										deviceResolutions.insert(enc.value().width, enc.value().height);

				for (auto width_height = deviceResolutions.begin(); width_height != deviceResolutions.end(); width_height++)
				{
					QJsonObject resolution;
					QJsonArray fps;

					resolution["width"] = int(width_height.key());
					resolution["height"] = int(width_height.value());

					QIntList framerates = QIntList();
					for(auto i = _deviceProperties.begin(); i != _deviceProperties.end(); ++i)
						if (i.key() == it.key())
							for (auto y = i.value().inputs.begin(); y != i.value().inputs.end(); y++)
								if (y.key() == input.value())
									for (auto enc = y.value().encodingFormats.begin(); enc != y.value().encodingFormats.end(); enc++)
											if(enc.key() == encodingFormat && enc.value().width == width_height.key() && enc.value().height == width_height.value())
												for (auto fps = enc.value().framerates.begin(); fps != enc.value().framerates.end(); fps++)
													if(!framerates.contains(*fps))
														framerates << *fps;

					for (auto framerate : framerates)
						fps.append(framerate);

					resolution["fps"] = fps;
					resolutionArray.append(resolution);
				}

				format["resolutions"] = resolutionArray;
				formats.append(format);
			}
			in["formats"] = formats;
			video_inputs.append(in);

		}

		device["video_inputs"] = video_inputs;
		inputsDiscovered.append(device);
	}

	_deviceProperties.clear();
	DebugIf(verbose, _log, "device: [%s]", QString(QJsonDocument(inputsDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return inputsDiscovered;
}

void V4L2Grabber::enumFrameIntervals(QList<int> &framerates, int fileDescriptor, int pixelformat, int width, int height)
{
	// collect available frame rates
	struct v4l2_frmivalenum frmivalenum;
	CLEAR(frmivalenum);

	frmivalenum.index = 0;
	frmivalenum.pixel_format = pixelformat;
	frmivalenum.width = width;
	frmivalenum.height = height;

	while (xioctl(fileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum) >= 0)
	{
		int rate;
		switch (frmivalenum.type)
		{
			case V4L2_FRMSIZE_TYPE_DISCRETE:
			{
				if (frmivalenum.discrete.numerator != 0)
				{
					rate = frmivalenum.discrete.denominator / frmivalenum.discrete.numerator;
					if (!framerates.contains(rate))
						framerates.append(rate);
				}
			}
			break;
			case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			case V4L2_FRMSIZE_TYPE_STEPWISE:
			{
				if (frmivalenum.stepwise.min.denominator != 0)
				{
					rate = frmivalenum.stepwise.min.denominator / frmivalenum.stepwise.min.numerator;
					if (!framerates.contains(rate))
						framerates.append(rate);
				}
			}
		}
		frmivalenum.index++;
	}

	// If VIDIOC_ENUM_FRAMEINTERVALS fails, try to read the current fps via VIDIOC_G_PARM if possible and insert it into 'framerates'.
	if (xioctl(fileDescriptor, VIDIOC_ENUM_FRAMESIZES, &frmivalenum) == -1)
	{
		struct v4l2_streamparm streamparms;
		CLEAR(streamparms);
		streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (xioctl(fileDescriptor, VIDIOC_G_PARM, &streamparms) >= 0)
			framerates.append(streamparms.parm.capture.timeperframe.denominator / streamparms.parm.capture.timeperframe.numerator);
	}
}