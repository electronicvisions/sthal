#pragma once

#ifndef PYPLUSPLUS
#include <tbb/concurrent_vector.h>
#endif

#include "halco/hicann/v2/hicann.h"

#include "sthal/HICANNConfigurator.h"

namespace C = ::halco::hicann::v2;

namespace sthal {

class HICANN;
class Wafer;

struct VerificationResult
{
	::halco::hicann::v2::HICANNGlobal hicann; ///< result from HICANN
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
	enum SynapsePolicy
	{
		All,
		None,
		Mask
	};

	/// "verify only enabled" only compares the settings of enabled components but also
	/// checks if disabled component are wrongfully configured to be enabled.
	/// synapse_policy specifies which synapses are verified in the synapse weight test
	/// If the policy "Mask" is specified, only synapses listed in m_synapse_mask are verified.
	VerifyConfigurator(bool verify_only_enabled = false, SynapsePolicy synapse_policy = All);

	void set_synapse_policy(SynapsePolicy const sp);
	SynapsePolicy get_synapse_policy() const;
	/// set synapses to be verified
	void set_synapse_mask(std::vector<C::SynapseOnWafer> const& syn_mask);
	/// get synapses to be verified
	std::vector<C::SynapseOnWafer> get_synapse_mask() const;
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
	virtual void read_synapse_controllers(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_neuron_config(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_background_generators(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void read_neuron_quads(hicann_handle_t const& h, hicann_data_t const& hicann);

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();


	friend std::ostream& operator<<(std::ostream& out, const VerifyConfigurator& cfg);

private:
	void data_not_readable(::halco::hicann::v2::HICANNGlobal hicann, std::string subsystem);
	void post_merge_errors(
	    ::halco::hicann::v2::HICANNGlobal hicann,
	    std::string subsystem,
	    std::vector<std::string>& errors,
	    bool reliable = true);
	void post_error(
	    ::halco::hicann::v2::HICANNGlobal hicann,
	    std::string subsystem,
	    std::string errors,
	    bool reliable = true);

#ifndef PYPLUSPLUS
	tbb::concurrent_vector<VerificationResult> mErrors;
#endif
	bool const m_verify_only_enabled;

	/**
	 * @brief: Specifies which synapses are verified during the test
	 * one of [All, None, Mask]
	 * All: Verify all synapses
	 * None: Skip verification for all synapses
	 * Mask: Only verify synapses specified in m_synapse_mask
	 * default: All
	 */
	SynapsePolicy m_synapse_policy;
	// Mask of synapses to be verified
	std::vector<C::SynapseOnWafer> m_synapse_mask;
};

} // end namespace sthal
