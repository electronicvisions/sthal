#pragma once

#include <boost/shared_ptr.hpp>

#include "hal/ADC/USBSerial.h"
#include "halco/hicann/v2/fwd.h"

namespace HMF {
namespace Handle {
	struct ADC;
}
}

namespace sthal {

struct ADCHandleFactory
{
	virtual ~ADCHandleFactory();
	virtual boost::shared_ptr< ::HMF::Handle::ADC > create(const ::HMF::ADC::USBSerial & adc) const;
};

} // namespace sthal
