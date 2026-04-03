#include "utils/ImageResampler.h"
#include <utils/ColorSys.h>
#include <utils/Logger.h>

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

ImageResampler::ImageResampler()
	: _horizontalDecimation(8)
	, _verticalDecimation(8)
	, _cropLeft(0)
	, _cropRight(0)
	, _cropTop(0)
	, _cropBottom(0)
	, _videoMode(VideoMode::VIDEO_2D)
	, _flipMode(FlipMode::NO_CHANGE)
{
}

void ImageResampler::setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom)
{
	_cropLeft   = cropLeft;
	_cropRight  = cropRight;
	_cropTop    = cropTop;
	_cropBottom = cropBottom;
}

void ImageResampler::processImage(const uint8_t * data, int width, int height, size_t lineLength, PixelFormat pixelFormat, Image<ColorRgb> &outputImage) const
{
	int cropLeft = _cropLeft;
	int cropRight  = _cropRight;
	int cropTop = _cropTop;
	int cropBottom = _cropBottom;

	// handle 3D mode
	switch (_videoMode)
	{
	case VideoMode::VIDEO_3DSBS:
		cropRight =  (width >> 1) + (cropRight >> 1);
		cropLeft = cropLeft >> 1;
		break;
	case VideoMode::VIDEO_3DTAB:
		cropBottom = (height >> 1) + (cropBottom >> 1);
		cropTop = cropTop >> 1;
		break;
	default:
		break;
	}

	// calculate the output size
	int outputWidth = (width - cropLeft - cropRight - (_horizontalDecimation >> 1) + _horizontalDecimation - 1) / _horizontalDecimation;
	int outputHeight = (height - cropTop - cropBottom - (_verticalDecimation >> 1) + _verticalDecimation - 1) / _verticalDecimation;

	outputImage.resize(outputWidth, outputHeight);

	int xDestStart {0};
	int xDestEnd = {outputWidth-1};
	int yDestStart = {0};
	int yDestEnd = {outputHeight-1};

	switch (_flipMode)
	{
		case FlipMode::NO_CHANGE:
			break;
		case FlipMode::HORIZONTAL:
			xDestStart = 0;
			xDestEnd = outputWidth-1;
			yDestStart = -(outputHeight-1);
			yDestEnd = 0;
			break;
		case FlipMode::VERTICAL:
			xDestStart = -(outputWidth-1);
			xDestEnd = 0;
			yDestStart = 0;
			yDestEnd = outputHeight-1;
			break;
		case FlipMode::BOTH:
			xDestStart = -(outputWidth-1);
			xDestEnd = 0;
			yDestStart = -(outputHeight-1);
			yDestEnd = 0;
			break;
	}

	switch (pixelFormat)
	{
		case PixelFormat::UYVY:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 1);
					uint8_t y = data[index+1];
					uint8_t u = ((xSource&1) == 0) ? data[index  ] : data[index-2];
					uint8_t v = ((xSource&1) == 0) ? data[index+2] : data[index  ];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::YUYV:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 1);
					uint8_t y = data[index];
					uint8_t u = ((xSource&1) == 0) ? data[index+1] : data[index-1];
					uint8_t v = ((xSource&1) == 0) ? data[index+3] : data[index+1];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::BGR16:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 1);
					rgb.blue  = static_cast<uint8_t>((data[index] & 0x1f) << 3);
					rgb.green = static_cast<uint8_t>((((data[index+1] & 0x7) << 3) | (data[index] & 0xE0) >> 5) << 2);
					rgb.red   = (data[index+1] & 0xF8);
				}
			}
			break;
		}

		case PixelFormat::RGB24:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 1) + xSource;
					rgb.red   = data[index  ];
					rgb.green = data[index+1];
					rgb.blue  = data[index+2];
				}
			}
			break;
		}

		case PixelFormat::BGR24:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 1) + xSource;
					rgb.blue  = data[index  ];
					rgb.green = data[index+1];
					rgb.red   = data[index+2];
				}
			}
			break;
		}

		case PixelFormat::RGB32:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 2);
					rgb.red   = data[index  ];
					rgb.green = data[index+1];
					rgb.blue  = data[index+2];
				}
			}
			break;
		}

		case PixelFormat::BGR32:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					size_t index = lineLength * ySource + (xSource << 2);
					rgb.blue  = data[index  ];
					rgb.green = data[index+1];
					rgb.red   = data[index+2];
				}
			}
			break;
		}

		case PixelFormat::NV12:
		case PixelFormat::P010:
		{
			const bool is10bit = (pixelFormat == PixelFormat::P010);

			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));

					uint8_t y, u, v;
					if (is10bit)
					{
						const uint16_t* yRow = (const uint16_t*)(data + lineLength * (size_t)ySource);
						const uint16_t* uvRow = (const uint16_t*)(data + lineLength * (size_t)(height + ySource / 2));
						y = (uint8_t)(yRow[xSource] >> 8);
						u = (uint8_t)(uvRow[(xSource >> 1) << 1] >> 8);
						v = (uint8_t)(uvRow[((xSource >> 1) << 1) + 1] >> 8);
					}
					else
					{
						size_t uOffset = (height + ySource / 2) * lineLength;
						y = data[lineLength * ySource + xSource];
						u = data[uOffset + ((xSource >> 1) << 1)];
						v = data[uOffset + ((xSource >> 1) << 1) + 1];
					}

					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::NV21:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				size_t vOffset = (height + ySource / 2) * lineLength;
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					uint8_t y = data[lineLength * ySource + xSource];
					uint8_t v = data[vOffset + ((xSource >> 1) << 1)];
					uint8_t u = data[vOffset + ((xSource >> 1) << 1) + 1];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::P030:
		{
			const size_t uvLineLength = (size_t)DIV_ROUND_UP(width / 2, 3) * 2 * 4;
			const size_t yPlaneBytes  = (size_t)lineLength * (size_t)height;

			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				const size_t uvRowOffset = yPlaneBytes + (size_t)(ySource / 2) * uvLineLength;
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));

					// Y sample
					const size_t yWordIdx = (size_t)(xSource / 3);
					const int ySubPixel = xSource % 3;
					const uint32_t yWord = ((const uint32_t*)(data + (size_t)lineLength * (size_t)ySource))[yWordIdx];
					uint16_t y10 = (yWord >> (ySubPixel * 10)) & 0x3FF;
					uint8_t y = (uint8_t)(y10 >> 2);

					// UV sample
					const int chromaX = xSource / 2;
					const size_t uvPairIdx = (size_t)(chromaX / 3);
					const int uvSubPixel = chromaX % 3;

					const uint32_t* uvWords = (const uint32_t*)(data + uvRowOffset) + uvPairIdx * 2;
					const uint32_t wordA = uvWords[0];
					const uint32_t wordB = uvWords[1];

					uint16_t u10, v10;
					switch (uvSubPixel)
					{
						case 0:
							u10 = wordA & 0x3FF;
							v10 = (wordA >> 10) & 0x3FF;
							break;
						case 1:
							u10 = (wordA >> 20) & 0x3FF;
							v10 = wordB & 0x3FF;
							break;
						case 2:
						default:
							u10 = (wordB >> 10) & 0x3FF;
							v10 = (wordB >> 20) & 0x3FF;
							break;
					}
					uint8_t u = (uint8_t)(u10 >> 2);
					uint8_t v = (uint8_t)(v10 >> 2);

					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::I420:
		{
			for (int yDest = yDestStart, ySource = cropTop + (_verticalDecimation >> 1); yDest <= yDestEnd; ySource += _verticalDecimation, ++yDest)
			{
				int uOffset = width * height + (ySource/2) * width/2;
				int vOffset = width * height + (width * height / 4) + (ySource/2) * width/2;
				for (int xDest = xDestStart, xSource = cropLeft + (_horizontalDecimation >> 1); xDest <= xDestEnd; xSource += _horizontalDecimation, ++xDest)
				{
					ColorRgb & rgb = outputImage(abs(xDest), abs(yDest));
					uint8_t y = data[lineLength * ySource + xSource];
					uint8_t u = data[uOffset + (xSource >> 1)];
					uint8_t v = data[vOffset + (xSource >> 1)];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
			}
			break;
		}

		case PixelFormat::MJPEG:
			break;

		case PixelFormat::NO_CHANGE:
			Error(Logger::getInstance("ImageResampler"), "Invalid pixel format given");
			break;
	}
}
