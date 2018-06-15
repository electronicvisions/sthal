#pragma once

#include "pywrap/compat/macros.hpp"
#include "sthal/ADCHandleFactory.h"
#include "hal/ADC/USBSerial.h"

namespace HMF {
namespace Handle {
	class Ess;
}
}

namespace sthal {

struct ADCEssHandleFactory : public ADCHandleFactory
{
	ADCEssHandleFactory(boost::shared_ptr< ::HMF::Handle::Ess>);
	~ADCEssHandleFactory();

	virtual boost::shared_ptr< ::HMF::Handle::ADC >
	create(const ::HMF::ADC::USBSerial & adc) const PYPP_OVERRIDE;

private:
	boost::shared_ptr< ::HMF::Handle::Ess> mEss;
};

} // namespace sthal
