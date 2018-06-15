#include "ADCHandleFactory.h"

#include "hal/Handle/ADCHw.h"

namespace sthal {

ADCHandleFactory::~ADCHandleFactory()
{
}

boost::shared_ptr< ::HMF::Handle::ADC >
ADCHandleFactory::create(const ::HMF::ADC::USBSerial & adc) const
{
	return boost::shared_ptr< ::HMF::Handle::ADC >(new ::HMF::Handle::ADC(adc));
}

} // end namespace sthal
