#include "ADCHwHandleFactory.h"

#include "hal/Handle/ADCHw.h"

namespace sthal {

boost::shared_ptr< ::HMF::Handle::ADC >
ADCHwHandleFactory::create(const ::HMF::ADC::USBSerial & adc) const
{
	return boost::shared_ptr< ::HMF::Handle::ADC >(new ::HMF::Handle::ADCHw(adc));
}

} // namespace sthal
