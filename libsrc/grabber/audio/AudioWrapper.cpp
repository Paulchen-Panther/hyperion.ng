#include <grabber/audio/AudioWrapper.h>
#include <hyperion/GrabberWrapper.h>
#include <QObject>
#include <QMetaType>

AudioWrapper::AudioWrapper()
	: GrabberWrapper("AudioGrabber", &_grabber)
{
	// register the image type
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");

	connect(&_grabber, &AudioGrabber::newFrame, this, &AudioWrapper::newFrame, Qt::DirectConnection);
}

AudioWrapper::~AudioWrapper()
{
	AudioWrapper::stop();
}

bool AudioWrapper::start()
{
	return (_grabber.start() && GrabberWrapper::start());
}

void AudioWrapper::stop()
{
	_grabber.stop();
	GrabberWrapper::stop();
}

void AudioWrapper::action()
{
	// Dummy we will push the audio images
}

void AudioWrapper::newFrame(const Image<ColorRgb>& image)
{
	emit systemImage(_grabber.getGrabberName(), image);
}

void AudioWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::AUDIO)
	{
		const QJsonObject& obj = config.object();

		// set global grabber state
		setAudioGrabberState(obj["enable"].toBool(false));

		if (getAudioGrabberState())
		{
			_grabber.setDevice(obj["device"].toString());

#ifdef ENABLE_PROJECTM
			const QString audioEffect = obj["audioEffect"].toString();
			if (audioEffect == "projectM")
			{
				const QJsonObject pmConfig = obj["projectM"].toObject();
				const int  width      = pmConfig["width"].toInt(64);
				const int  height     = pmConfig["height"].toInt(64);
				const QString presets = pmConfig["presetPath"].toString();
				_grabber.setProjectMEnabled(true, width, height, presets);
			}
			else
			{
				_grabber.setProjectMEnabled(false);
				_grabber.setConfiguration(obj);
			}
#else
			_grabber.setConfiguration(obj);
#endif

			_grabber.restart();
		}
		else
		{
			stop();
		}
	}
}
