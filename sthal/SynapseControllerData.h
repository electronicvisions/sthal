#pragma once

#include <boost/serialization/export.hpp>
#include "hal/HICANNContainer.h"
#include "pywrap/compat/macros.hpp"

namespace sthal {
/*
 * Represents the configuration of a single Synapse Controller.
 */
class SynapseControllerData
{
public:

	SynapseControllerData();

	/*
	 * Creates an object which represents the halbe configuration given by `synapse_controller`.
	 */
	explicit SynapseControllerData(::HMF::HICANN::SynapseController const& synapse_controller);

	// TODO: Implement STDP functionality

	/*
	 * Settings of the control register
	 *
	 * For a more detailed explanation see ::HMF::HICANN:SynapseControlRegister,
	 * the names of the corresponding member variables are given as comments.
	 */
	PYPP_INIT(bool readout_acausal_capacitor, true);          // == sca
	PYPP_INIT(bool readout_causal_capacitor, true);           // == scc
	PYPP_INIT(bool reset_capacitors_after_evaluation, true);  // == !without_reset
	PYPP_INIT(bool continuous_weight_updates, false);          // == continuous
	PYPP_INIT(bool correlation_readout, false);               // == encr

	/*
	 * Settings of the configuration register
	 *
	 * For a more detailed explanation see ::HMF::HICANN:SynapseConfigurationRegister,
	 * the names of the corresponding member variables are given as comments (if not
	 * identical).
	 */
	PYPP_INIT(HMF::HICANN::SynapseControllerTimings synarray_timings,
		HMF::HICANN::SynapseControllerTimings(::HMF::HICANN::SynapseWriteDelay(2),
		                                      ::HMF::HICANN::SynapseOutputDelay(0xf),
		                                      ::HMF::HICANN::SynapseSetupPrecharge(0xf),
		                                      ::HMF::HICANN::SynapseEnableDelay(0xf)));
	// Combines the DLL reset setting of synapse drives on the left and on the right side of
	// the synapse array
	PYPP_INIT(bool dllreset, true);
	PYPP_INIT(HMF::HICANN::SynapseGen gen, HMF::HICANN::SynapseGen(0));
	HMF::HICANN::STDPEvaluationPattern evaluation_pattern0; // pattern0
	HMF::HICANN::STDPEvaluationPattern evaluation_pattern1; // pattern1

	// LUT for STDP weight updates
	::HMF::HICANN::STDPLUT lut;

	// SRAM timings of synapse driver
	::HMF::HICANN::SRAMControllerTimings syndrv_timings;

	bool operator==(SynapseControllerData const& other) const;
	bool operator!=(SynapseControllerData const& other) const;

	/*
	 * Fills a default constructed halbe container of the synapse controller with
	 * the data stored in this object.
	 */
	PYPP_EXPLICIT_CAST operator ::HMF::HICANN::SynapseController() const;

private:
	friend class boost::serialization::access;
	template <typename Archiver>
	void serialize(Archiver& ar, unsigned int const);

	friend std::ostream& operator<<(std::ostream& os, SynapseControllerData const& c);
};


} // sthal

BOOST_CLASS_EXPORT_KEY(sthal::SynapseControllerData)
