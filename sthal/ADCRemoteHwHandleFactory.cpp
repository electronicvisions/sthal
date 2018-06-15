#include "ADCRemoteHwHandleFactory.h"

#include "hal/Handle/ADCRemoteHw.h"

namespace sthal {

ADCRemoteHwHandleFactory::ADCRemoteHwHandleFactory(::HMF::Coordinate::IPv4 ip_, ::HMF::Coordinate::TCPPort port_)
	: ip(ip_)
	, port(port_)
{}



boost::shared_ptr< ::HMF::Handle::ADC >
ADCRemoteHwHandleFactory::create(const ::HMF::ADC::USBSerial & adc) const
{
    return boost::shared_ptr< ::HMF::Handle::ADC >(new ::HMF::Handle::ADCRemoteHw(ip, port, adc));
}

} // namespace sthal
