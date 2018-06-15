#pragma once

#include "pywrap/compat/macros.hpp"
#include "sthal/ADCHandleFactory.h"
#include "hal/ADC/USBSerial.h"

namespace sthal {

struct ADCHwHandleFactory : public ADCHandleFactory
{
	virtual boost::shared_ptr< ::HMF::Handle::ADC > create(const ::HMF::ADC::USBSerial & adc) const PYPP_OVERRIDE;
};

} // namespace sthal
