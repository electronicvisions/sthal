/* This configurator enables DNC Loopback and PlaybackSystimeReplacing */

#include "sthal/DNCLoopbackConfigurator.h"
#include "halco/common/iter_all.h"
#include "hal/Handle/HMFRun.h"
#include "hal/Handle/FPGAHw.h"
#include "hal/backend/DNCBackend.h"
#include "host_al_controller.h"

namespace sthal {

void DNCLoopbackConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& /*fg*/)
{
	HMF::Handle::FPGAHw& hw = dynamic_cast<HMF::Handle::FPGAHw&>(*f);
	HostALController& host_al = hw.getPowerBackend().get_host_al(hw);
	host_al.setPlaybackSystimeReplacing(true);

	for (auto dnc : halco::common::iter_all<halco::hicann::v2::DNCOnFPGA>() )
	{
		if (!f->dnc_active(dnc))
			continue;
                
		HMF::DNC::Loopback loopback;
		for (auto hicann : halco::common::iter_all<halco::hicann::v2::HICANNOnDNC>() )
		{
			loopback[hicann] = true;
		}
		HMF::DNC::set_loopback(*f, dnc, loopback);
	}
}

} // end namespace sthal
