#pragma once

#include <boost/shared_ptr.hpp>

#include "hal/ADC/USBSerial.h"
#include "hwdb4cpp/hwdb4cpp.h"

namespace sthal
{

struct ADCHandleFactory;

// FIXME: "ADCConfig()" => segfault! WTF ;)
struct ADCConfig
{
	typedef hwdb4cpp::ADCEntry::CalibrationMode CalibrationMode;
	::HMF::ADC::USBSerial coord;
	halco::hicann::v2::ChannelOnADC channel;
	halco::hicann::v2::TriggerOnADC trigger;
	boost::shared_ptr<const ADCHandleFactory> factory;
	CalibrationMode loadCalibration;
};

std::ostream& operator<<(std::ostream& out, ADCConfig const& obj);

} // end namespace sthal
