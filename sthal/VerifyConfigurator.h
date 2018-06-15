#pragma once

#ifndef PYPLUSPLUS
#include <tbb/concurrent_vector.h>
#endif

#include "hal/Coordinate/HICANN.h"

#include "sthal/HICANNConfigurator.h"

namespace sthal {

class HICANN;
class Wafer;

struct VerificationResult
{
	::HMF::Coordinate::HICANNGlobal hicann; ///< result from HICANN
	std::string subsystem;                  ///< Read HICANN subsystem
	std::string msg;                        ///< Description of error occured
	size_t tested; ///< number of tested entities, e.g. a synapse weight, or a FGConfig
	size_t errors; ///< number of tested entities with errors
	bool reliable; ///< Are the read values reliable, false if not readable
	bool readable; ///< False if the subsystem can't be read

	friend std::ostream& operator<<(std::ostream& out, const VerificationResult& r);
};

/// This Configurator reads back the HICANN configuration and compares it to
/// the stored configuration.
/// The results are stored in a vector of VerificationResults. They are not
/// cleared between runs!
/// Note: Not all values can be properly read back from the hardware
class VerifyConfigurator : public HICANNConfigurator
{
public:
	VerifyConfigurator();

	/// Clear stored results
	void clear();
	/// Access stored results
	std::vector<VerificationResult> results() const;
	/// Count occurred errors (sum over VerificationResult.errors)
	/// If include_unreliable is true, this are also counted
	size_t error_count(bool include_unreliable = false) const;

	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
	virtual void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg);

	virtual void read_fg_stimulus(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_floating_gates(
		fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_analog_readout(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_merger_tree(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_dncmerger(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_gbitlink(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_phase(hicann_handle_t const& h, hicann_data_t const& hicann);

	virtual void read_repeater(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_synapse_switch(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_crossbar_switches(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_synapse_drivers(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_synapse_weights(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_synapse_decoders(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_stdp(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_neuron_config(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_background_generators(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_neuron_quads(hicann_handle_t const& h, hicann_data_t const& hicann);

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();


	friend std::ostream& operator<<(std::ostream& out, const VerifyConfigurator& cfg);

private:
	void data_not_readable(::HMF::Coordinate::HICANNGlobal hicann, std::string subsystem);
	void post_merge_errors(
	    ::HMF::Coordinate::HICANNGlobal hicann,
	    std::string subsystem,
	    std::vector<std::string>& errors,
	    bool reliable = true);
	void post_error(
	    ::HMF::Coordinate::HICANNGlobal hicann,
	    std::string subsystem,
	    std::string errors,
	    bool reliable = true);

#ifndef PYPLUSPLUS
	tbb::concurrent_vector<VerificationResult> mErrors;
#endif
};

} // end namespace sthal
