#include "sthal/ADCConfig.h"

namespace sthal
{

std::ostream& operator<<(std::ostream& out, ADCConfig const& obj)
{
	out << "ADCConfig(";
	out << "coord=" << obj.coord << ", ";
	out << "channel=" << obj.channel << ", ";
	out << "trigger=" << obj.trigger << ", ";
	out << "loadCalibration=";
	switch (obj.loadCalibration) {
		case ADCConfig::CalibrationMode::LOAD_CALIBRATION:
			out << "LOAD_CALIBRATION";
			break;
		case ADCConfig::CalibrationMode::ESS_CALIBRATION:
			out << "ESS_CALIBRATION";
			break;
		case ADCConfig::CalibrationMode::DEFAULT_CALIBRATION:
			out << "DEFAULT_CALIBRATION";
			break;
	}
	out << ")";
	return out;
}

} // end namespace sthal
