#include "ADCEssHandleFactory.h"

#include <boost/make_shared.hpp>

#include "hal/Handle/Ess.h"
#include "hal/Handle/ADCEss.h"

namespace sthal {

ADCEssHandleFactory::ADCEssHandleFactory(
		boost::shared_ptr< ::HMF::Handle::Ess > ess) :
	mEss(ess)
{}

ADCEssHandleFactory::~ADCEssHandleFactory()
{
}

boost::shared_ptr< ::HMF::Handle::ADC >
ADCEssHandleFactory::create(const ::HMF::ADC::USBSerial & adc) const
{
	return boost::shared_ptr< ::HMF::Handle::ADC >(
			new ::HMF::Handle::ADCEss(adc, mEss));
}

} // namespace sthal
