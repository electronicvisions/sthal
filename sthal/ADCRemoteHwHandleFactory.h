#pragma once

#include "pywrap/compat/macros.hpp"
#include "sthal/ADCHandleFactory.h"

#include "halco/hicann/v2/external.h"

namespace sthal {

struct ADCRemoteHwHandleFactory : public ADCHandleFactory
{
	ADCRemoteHwHandleFactory(::halco::hicann::v2::IPv4 ip, ::halco::hicann::v2::TCPPort port);

	virtual boost::shared_ptr< ::HMF::Handle::ADC > create(const ::HMF::ADC::USBSerial & adc) const PYPP_OVERRIDE;

private:
	::halco::hicann::v2::IPv4 ip;
	::halco::hicann::v2::TCPPort port;
};

} // namespace sthal
