#pragma once

#ifdef ENABLE_PROJECTM

#include <cstdint>
#include <QString>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Logger.h>

// Forward declarations to avoid including EGL/projectM headers in this header
struct ProjectMWrapperPrivate;

///
/// @brief ProjectM visualizer wrapper
///
/// Sets up a headless OpenGL context via EGL, initialises a projectM instance,
/// feeds PCM audio samples into it, renders frames and returns the result as
/// Image<ColorRgb> objects.
///
class ProjectMWrapper
{
public:
	ProjectMWrapper();
	~ProjectMWrapper();

	///
	/// @brief Initialise the headless EGL context and projectM
	///
	/// @param width   Render width in pixels
	/// @param height  Render height in pixels
	/// @param presetPath  Directory containing projectM preset files
	/// @return true on success, false otherwise
	///
	bool init(int width, int height, const QString& presetPath);

	///
	/// @brief Release all OpenGL / projectM resources
	///
	void cleanup();

	///
	/// @brief Feed a buffer of 16-bit mono PCM samples into projectM
	///
	/// @param buffer  Pointer to mono 16-bit samples
	/// @param length  Number of samples in the buffer
	///
	void addPCMData(const int16_t* buffer, int length);

	///
	/// @brief Render one projectM frame and return it as an RGB image
	///
	/// @param[out] image  Receives the rendered frame
	/// @return true on success, false if not initialised or rendering failed
	///
	bool renderFrame(Image<ColorRgb>& image);

	///
	/// @return true if the wrapper has been successfully initialised
	///
	bool isInitialised() const { return _initialised; }

private:
	ProjectMWrapperPrivate* _p;
	bool _initialised;
	int  _width;
	int  _height;
	QSharedPointer<Logger> _log;
};

#endif // ENABLE_PROJECTM
