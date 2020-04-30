#include "sthal/SynapseControllerData.h"
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>

namespace sthal {
SynapseControllerData::SynapseControllerData() {}

SynapseControllerData::SynapseControllerData(::HMF::HICANN::SynapseController const& synapse_controller)
{
	// Just set values represented in sthal, leave rest at default values
	readout_acausal_capacitor = synapse_controller.ctrl_reg.sca;
	readout_causal_capacitor = synapse_controller.ctrl_reg.scc;
	reset_capacitors_after_evaluation = !synapse_controller.ctrl_reg.without_reset;
	continuous_weight_updates = synapse_controller.ctrl_reg.continuous;
	correlation_readout = synapse_controller.ctrl_reg.encr;

	synarray_timings = synapse_controller.cnfg_reg.synarray_timings;
	// sthal only supports that all drivers on an array are enabled or disabled
	switch (synapse_controller.cnfg_reg.dllresetb) {
		case HMF::HICANN::SynapseDllresetb::min:
			dllreset = true;
			break;
		case HMF::HICANN::SynapseDllresetb::max:
			dllreset = false;
			break;
		default:
			throw std::runtime_error("sthal does not support different DLL reset states "
			                         "for drivers on the left and right side. All drivers "
			                         "have to be enabled or disabled.");
	}
	gen = synapse_controller.cnfg_reg.gen;
	evaluation_pattern0 = synapse_controller.cnfg_reg.pattern0;
	evaluation_pattern1 = synapse_controller.cnfg_reg.pattern1;

	lut = synapse_controller.lut;
	syndrv_timings = synapse_controller.syndrv_timings;
}

bool SynapseControllerData::operator==(SynapseControllerData const& other) const
{
	return (
	    readout_acausal_capacitor == other.readout_acausal_capacitor &&
	    readout_causal_capacitor == other.readout_causal_capacitor &&
	    reset_capacitors_after_evaluation == other.reset_capacitors_after_evaluation &&
	    continuous_weight_updates == other.continuous_weight_updates &&
	    correlation_readout == other.correlation_readout &&
	    synarray_timings == other.synarray_timings &&
	    dllreset == other.dllreset &&
	    gen == other.gen &&
	    evaluation_pattern0 == other.evaluation_pattern0 &&
	    evaluation_pattern1 == other.evaluation_pattern1 &&
	    lut == other.lut &&
	    syndrv_timings == other.syndrv_timings);
}

bool SynapseControllerData::operator!=(SynapseControllerData const& other) const
{
	return !(*this == other);
}

SynapseControllerData::operator ::HMF::HICANN::SynapseController() const
{
	// Just set values represented in sthal, leave rest at default values
	::HMF::HICANN::SynapseController synapse_controller;

	// Control Register
	synapse_controller.ctrl_reg.sca = readout_acausal_capacitor;
	synapse_controller.ctrl_reg.scc = readout_causal_capacitor;
	synapse_controller.ctrl_reg.without_reset = !reset_capacitors_after_evaluation;
	synapse_controller.ctrl_reg.continuous = continuous_weight_updates;
	synapse_controller.ctrl_reg.encr = correlation_readout;

	// Configuration Register
	synapse_controller.cnfg_reg.synarray_timings = synarray_timings;
	synapse_controller.cnfg_reg.dllresetb = HMF::HICANN::SynapseDllresetb(
		dllreset ? HMF::HICANN::SynapseDllresetb::min : HMF::HICANN::SynapseDllresetb::max);
	synapse_controller.cnfg_reg.gen = gen;
	synapse_controller.cnfg_reg.pattern0 = evaluation_pattern0;
	synapse_controller.cnfg_reg.pattern1 = evaluation_pattern1;


	synapse_controller.lut = lut;
	synapse_controller.syndrv_timings = syndrv_timings;

	return synapse_controller;
}

template <typename Archiver>
void SynapseControllerData::serialize(Archiver& ar, unsigned int const)
{
	ar& boost::serialization::make_nvp("readout_acausal_capacitor",
			readout_acausal_capacitor);
	ar& boost::serialization::make_nvp("readout_causal_capacitor",
			readout_causal_capacitor);
	ar& boost::serialization::make_nvp("reset_capacitors_after_evaluation",
			reset_capacitors_after_evaluation);
	ar& boost::serialization::make_nvp("continuous_weight_updates",
			continuous_weight_updates);
	ar& boost::serialization::make_nvp("correlation_readout", correlation_readout);
	ar& boost::serialization::make_nvp("synarray_timings", synarray_timings);
	ar& boost::serialization::make_nvp("dllreset", dllreset);
	ar& boost::serialization::make_nvp("gen", gen);
	ar& boost::serialization::make_nvp("evaluation_pattern0", evaluation_pattern0);
	ar& boost::serialization::make_nvp("evaluation_pattern1", evaluation_pattern1);
	ar& boost::serialization::make_nvp("lut", lut);
	ar& boost::serialization::make_nvp("syndrv_timings", syndrv_timings);
}

std::ostream& operator<<(std::ostream& os, SynapseControllerData const& c)
{
	os << "readout acausal capacitor" << c.readout_acausal_capacitor << '\n';
	os << "readout causal capacitor" << c.readout_causal_capacitor << '\n';
	os << "reset capacitors after evaluation" << c.reset_capacitors_after_evaluation << '\n';
	os << "continuous weight updates" << c.continuous_weight_updates << '\n';
	os << "correlation readout" << c.correlation_readout << '\n';
	os << "synapse array timings" << c.synarray_timings << '\n';
	os << "dllreset" << c.dllreset << '\n';
	os << "gen" << c.gen << '\n';
	os << "evaluation pattern 0" << c.evaluation_pattern0 << '\n';
	os << "evaluation pattern 1" << c.evaluation_pattern1 << '\n';
	os << "STDP LUT" << c.lut << '\n';
	os << "synapse driver timings" << c.syndrv_timings << '\n';
	return os;
}
} // sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::SynapseControllerData)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::SynapseControllerData)
