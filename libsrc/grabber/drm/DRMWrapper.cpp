#include <grabber/drm/DRMWrapper.h>

DRMWrapper::DRMWrapper(int updateRate_Hz,
					   int deviceIdx,
					   int pixelDecimation,
					   int cropLeft, int cropRight, int cropTop, int cropBottom)
	: GrabberWrapper(GRABBERTYPE, &_grabber, updateRate_Hz)
	, _grabber(deviceIdx, cropLeft, cropRight, cropTop, cropBottom)
{
	_grabber.setPixelDecimation(pixelDecimation);
}

DRMWrapper::DRMWrapper(const QJsonDocument &grabberConfig)
	: DRMWrapper(GrabberWrapper::DEFAULT_RATE_HZ,
				 grabberConfig["input"].toInt(0),
				 GrabberWrapper::DEFAULT_PIXELDECIMATION,
				 0, 0, 0, 0)
{
	if (_grabber.isAvailable())
	{
		DRMWrapper::handleSettingsUpdate(settings::SYSTEMCAPTURE, grabberConfig);
	}
}

void DRMWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type != settings::SYSTEMCAPTURE)
	{
		return;
	}

	const QJsonObject obj = config.object();
	const SandEdgeSettings defaults;
	const auto toUnsigned = [&obj](const char* key, unsigned def, int minValue)
	{
		return static_cast<unsigned>(qMax(minValue, obj.value(key).toInt(static_cast<int>(def))));
	};

	SandEdgeSettings sand;
	sand.enabled = obj.value("sandEdgeEnable").toBool(defaults.enabled);
	sand.bandPx = toUnsigned("sandBandPx", defaults.bandPx, 2);
	sand.xDecim = toUnsigned("sandXDecim", defaults.xDecim, 1);
	sand.yDecim = toUnsigned("sandYDecim", defaults.yDecim, 1);
	sand.cellsTop = toUnsigned("sandCellsTop", defaults.cellsTop, 0);
	sand.cellsBottom = toUnsigned("sandCellsBottom", defaults.cellsBottom, 0);
	sand.cellsLeft = toUnsigned("sandCellsLeft", defaults.cellsLeft, 0);
	sand.cellsRight = toUnsigned("sandCellsRight", defaults.cellsRight, 0);
	sand.borderPx = toUnsigned("sandBorderPx", defaults.borderPx, 1);
	sand.bt2020 = obj.value("sandMatrix").toString("bt709").compare("bt2020", Qt::CaseInsensitive) == 0;
	sand.fullRange = obj.value("sandFullRange").toBool(defaults.fullRange);
	_grabber.setSandEdgeSettings(sand);

	GrabberWrapper::handleSettingsUpdate(settings::SYSTEMCAPTURE, config);
}

void DRMWrapper::action()
{
	if (!_grabber.isAvailable())
	{
		return;
	}

	transferFrame(_grabber);
}
