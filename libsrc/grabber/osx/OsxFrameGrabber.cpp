// STL includes
#include <cassert>
#include <iostream>

// Local includes
#include <grabber/OsxFrameGrabber.h>

//Qt
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

// Constants
namespace {
const bool verbose = false;
} //End of constants

OsxFrameGrabber::OsxFrameGrabber(int display)
	: Grabber("OSXGRABBER")
	, _screenIndex(display)
{
	_isEnabled = false;
	_useImageResampler = false;
}

OsxFrameGrabber::~OsxFrameGrabber()
{
}

bool OsxFrameGrabber::setupDisplay()
{
	bool rc (false);

	rc = setDisplayIndex(_screenIndex);

	return rc;
}

int OsxFrameGrabber::grabFrame(Image<ColorRgb> & image)
{
	int rc = 0;
	if (_isEnabled && !_isDeviceInError)
	{

		CGImageRef dispImage;

		if (!(_cropLeft > 0 || _cropTop > 0 || _cropRight > 0 || _cropBottom > 0))
		{
			dispImage = CGDisplayCreateImage(_display);
		}
		else
		{
			CGRect region = CGRectMake(
				_cropLeft,
				_cropTop,
				_displayWidth - _cropLeft - _cropRight,
				_displayHeight - _cropTop - _cropBottom
			);

			dispImage = CGDisplayCreateImageForRect(_display, region);
		}

		// display lost, use main
		if (dispImage == nullptr && _display != 0)
		{
			dispImage = CGDisplayCreateImage(kCGDirectMainDisplay);
			// no displays connected, return
			if (dispImage == nullptr)
			{
				Error(_log, "No display connected...");
				return -1;
			}
		}

		unsigned dspWidth  = CGImageGetWidth(dispImage);
		unsigned dspHeight = CGImageGetHeight(dispImage);

		if (_pixelDecimation > 1)
		{
			dspWidth = dspWidth / _pixelDecimation;
			dspHeight = dspHeight /_pixelDecimation;

			CGContextRef context = CGBitmapContextCreate(
				nullptr,
				dspWidth,
				dspHeight,
				CGImageGetBitsPerComponent(dispImage),
				CGImageGetBytesPerRow(dispImage) / CGImageGetWidth(dispImage) * dspWidth,
				CGImageGetColorSpace(dispImage),
				kCGImageAlphaPremultipliedLast
			);

			CGContextSetInterpolationQuality(context, kCGInterpolationDefault);
			CGContextDrawImage(context, CGContextGetClipBoundingBox(context), dispImage);
			CGImageRelease(dispImage);
			dispImage = CGBitmapContextCreateImage(context);
			CGContextRelease(context);
		}

		CFDataRef imgData   = CGDataProviderCopyData(CGImageGetDataProvider(dispImage));
		unsigned char *imgDataPtr  = (unsigned char*) CFDataGetBytePtr(imgData);

		image.resize(dspWidth, dspHeight);
		for (int source=0, destination=0; source < dspWidth * dspHeight * static_cast<int>(sizeof(ColorRgb)); source+=sizeof(ColorRgb), destination+=sizeof(ColorRgba))
		{
			memmove((uint8_t*)image.memptr() + source, imgDataPtr + destination, sizeof(ColorRgb));
		}

		CFRelease(imgData);
		CGImageRelease(dispImage);

	}
	return rc;
}

bool OsxFrameGrabber::setDisplayIndex(int index)
{
	bool rc (true);
	if(_screenIndex != index || !_isEnabled)
	{
		_screenIndex = index;

		// get list of displays
		CGDisplayCount dspyCnt = 0 ;
		CGDisplayErr err;
		err = CGGetActiveDisplayList(0, nullptr, &dspyCnt);
		if (err == kCGErrorSuccess && dspyCnt > 0)
		{
			CGDirectDisplayID *activeDspys = new CGDirectDisplayID[dspyCnt];
			err = CGGetActiveDisplayList(dspyCnt, activeDspys, &dspyCnt) ;
			if (err == kCGErrorSuccess)
			{
				CGImageRef image;

				if (_screenIndex + 1 > static_cast<int>(dspyCnt))
				{
					Error(_log, "Display with index %d is not available.", _screenIndex);
					rc = false;
				}
				else
				{
					_display = activeDspys[_screenIndex];

					image = CGDisplayCreateImage(_display);
					if(image == nullptr)
					{
						setEnabled(false);
						Error(_log, "Failed to open main display, disable capture interface");
						rc = false;
					}
					else
					{
						CGRect rect = CGDisplayBounds(activeDspys[_screenIndex]);
						_displayWidth  = static_cast<int>(rect.size.width);
						_displayHeight = static_cast<int>(rect.size.height);

						setEnabled(true);
						rc = true;
						Info(_log, "Display [%u] opened with resolution: %ux%u@%ubit", _display, CGImageGetWidth(image), CGImageGetHeight(image), CGImageGetBitsPerPixel(image));
					}
					CGImageRelease(image);
				}
			}
		}
		else
		{
			rc=false;
		}
	}
	return rc;
}

QJsonObject OsxFrameGrabber::discover(const QJsonObject& params)
{
	DebugIf(verbose, _log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	QJsonObject inputsDiscovered;

	// get list of displays
	CGDisplayCount dspyCnt = 0 ;
	CGDisplayErr err;
	err = CGGetActiveDisplayList(0, nullptr, &dspyCnt);
	if (err == kCGErrorSuccess && dspyCnt > 0)
	{
		CGDirectDisplayID *activeDspys = new CGDirectDisplayID [dspyCnt] ;
		err = CGGetActiveDisplayList(dspyCnt, activeDspys, &dspyCnt) ;
		if (err == kCGErrorSuccess)
		{
			inputsDiscovered["device"] = "osx";
			inputsDiscovered["device_name"] = "OSX";
			inputsDiscovered["type"] = "screen";

			QJsonArray video_inputs;
			QJsonArray fps = { 1, 5, 10, 15, 20, 25, 30, 40, 50, 60 };

			for (int i = 0; i < static_cast<int>(dspyCnt); ++i)
			{
				QJsonObject in;

				CGDirectDisplayID did = activeDspys[i];

				QString displayName;
				displayName = QString("Display:%1").arg(did);

				in["name"] = displayName;
				in["inputIdx"] = i;

				QJsonArray formats;
				QJsonObject format;

				QJsonArray resolutionArray;

				QJsonObject resolution;

				CGRect rect = CGDisplayBounds(did);
				resolution["width"] = static_cast<int>(rect.size.width);
				resolution["height"] = static_cast<int>(rect.size.height);

				resolution["fps"] = fps;

				resolutionArray.append(resolution);

				format["resolutions"] = resolutionArray;
				formats.append(format);

				in["formats"] = formats;
				video_inputs.append(in);
			}
			inputsDiscovered["video_inputs"] = video_inputs;

			QJsonObject defaults, video_inputs_default, resolution_default;
			resolution_default["fps"] = _fps;
			video_inputs_default["resolution"] = resolution_default;
			video_inputs_default["inputIdx"] = 0;
			defaults["video_input"] = video_inputs_default;
			inputsDiscovered["default"] = defaults;
		}
		delete [] activeDspys;
	}

	if (inputsDiscovered.isEmpty())
	{
		DebugIf(verbose, _log, "No displays found to capture from!");
	}
	DebugIf(verbose, _log, "device: [%s]", QString(QJsonDocument(inputsDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return inputsDiscovered;

}
