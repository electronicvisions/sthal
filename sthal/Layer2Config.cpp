#if __cplusplus >= 201103L

#include "sthal/Layer2Config.h"
#include "sthal/HMFContainers.h"
#include "hal/backend/FPGABackend.h"
#include "hal/backend/DNCBackend.h"

namespace HMF {
namespace FPGA {
void config(Handle::FPGA& f, const SpinnIFConfig & cfg) {
	// spinnaker stuff

	FPGA::set_spinnaker_routing_table(f, cfg.routing_table);

	FPGA::set_spinnaker_pulse_upsampler(f, cfg.upsample_count);
	FPGA::set_spinnaker_pulse_downsampler(f, cfg.downsample_count);

	FPGA::set_spinnaker_receive_port(f, cfg.receive_port);
	FPGA::set_spinnaker_sender_config(f, cfg.sender_cfg);
	FPGA::set_spinnaker_address_config(f, cfg.addr_cfg);
}
} // namespace FPGA

namespace DNC{

void config(Handle::FPGA& f, Coordinate::DNCOnFPGA const & dnc, const DNCConfig& cfg) {
	set_hicann_directions(f, dnc, cfg.gbit_reticle);
	set_loopback(f, dnc, cfg.loopback);
}

} // namespace DNC
} // HMF
#endif
