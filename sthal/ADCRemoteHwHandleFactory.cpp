#include "ADCRemoteHwHandleFactory.h"

#include "hal/Handle/ADCRemoteHw.h"

namespace sthal {

ADCRemoteHwHandleFactory::ADCRemoteHwHandleFactory(::halco::hicann::v2::IPv4 ip_, ::halco::hicann::v2::TCPPort port_)
	: ip(ip_)
	, port(port_)
{}



boost::shared_ptr< ::HMF::Handle::ADC >
ADCRemoteHwHandleFactory::create(const ::HMF::ADC::USBSerial & adc) const
{
    return boost::shared_ptr< ::HMF::Handle::ADC >(new ::HMF::Handle::ADCRemoteHw(ip, port, adc));
}

} // namespace sthal
