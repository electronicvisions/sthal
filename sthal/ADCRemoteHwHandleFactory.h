#pragma once

#include "pywrap/compat/macros.hpp"
#include "sthal/ADCHandleFactory.h"

namespace sthal {

struct ADCRemoteHwHandleFactory : public ADCHandleFactory
{
	ADCRemoteHwHandleFactory(::HMF::Coordinate::IPv4 ip, ::HMF::Coordinate::TCPPort port);

	virtual boost::shared_ptr< ::HMF::Handle::ADC > create(const ::HMF::ADC::USBSerial & adc) const PYPP_OVERRIDE;

private:
	::HMF::Coordinate::IPv4 ip;
	::HMF::Coordinate::TCPPort port;
};

} // namespace sthal
