#pragma once

#include <set>

#include "sthal/HICANNConfigurator.h"
#include "halco/hicann/v2/fwd.h"

namespace sthal {

class HICANN;
class Wafer;

class HICANNReadoutConfigurator : public HICANNConfigurator
{
public:
	// @param read_hicanns: HICANNs to be read, reads all when empty
	HICANNReadoutConfigurator(
	    sthal::Wafer& wafer, std::vector< ::halco::hicann::v2::HICANNOnWafer> read_hicanns);

	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
	virtual void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg);

	virtual void read_dnc_link(fpga_handle_t const& f, fpga_t const& fg);
	virtual void read_fg_stimulus(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_floating_gates(
		fpga_handle_t const& f,
		hicann_handle_t const& h,
		HICANN& hicann,
		hicann_data_t const& original_data);
	virtual void read_analog_readout(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_merger_tree(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_dncmerger(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_gbitlink(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_phase(hicann_handle_t const& h, HICANN& hicann);

	virtual void read_repeater(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_synapse_switch(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_crossbar_switches(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_synapse_drivers(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_synapse_array(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_stdp(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_neuron_config(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_background_generators(hicann_handle_t const& h, HICANN& hicann);
	virtual void read_neuron_quads(hicann_handle_t const& h, HICANN& hicann);

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();

private:
	Wafer & wafer;
	HICANN& getHICANN(hicann_data_t const& hicann);
	std::set< ::halco::hicann::v2::HICANNOnWafer> read_hicanns;
};

} // end namespace sthal
