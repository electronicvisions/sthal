#pragma once

#include <boost/serialization/version.hpp>

#include "hal/Coordinate/HMFGeometry.h"
#include "hal/HICANNContainer.h"
#include "hal/DNCContainer.h"
#include "hal/HMFUtil.h" // TODO: DELETE THIS

#include "sthal/CrossbarSwitches.h"
#include "sthal/SynapseSwitches.h"
#include "sthal/FloatingGates.h"
#include "sthal/Layer1.h"
#include "sthal/L1Repeaters.h"
#include "sthal/Neurons.h"
#include "sthal/SynapseArray.h"
#include "sthal/FGStimulus.h"

namespace sthal {

typedef ::HMF::HICANN::Analog AnalogOutput;

class AnalogRecorder;

// HICANN chip
struct HICANNData
{
public:
	HICANNData();
	virtual ~HICANNData();

	FloatingGates    floating_gates;
	AnalogOutput     analog;
	L1Repeaters	     repeater;
	SynapseArray     synapses;
	Neurons          neurons;
	Layer1           layer1;

	SynapseSwitches  synapse_switches;
	CrossbarSwitches crossbar_switches;

	std::array<FGStimulus, ::HMF::Coordinate::FGBlockOnHICANN::enum_type::size> current_stimuli;

	/// Copy HICANN configuration from an other HICANN, needed from python
	void copy(const HICANNData & other);

	friend bool operator==(const HICANNData & a, const HICANNData & b);

	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using namespace boost::serialization;
		if (version < 1)
		{
			// In version 0 we stored an FGControl
			::HMF::HICANN::FGControl tmp;
			ar & make_nvp("floating_gates", tmp);
			floating_gates = tmp;
		}
		else
		{
			ar & make_nvp("floating_gates", floating_gates);
		}
		ar & make_nvp("analog",            analog)
		   & make_nvp("repeater",          repeater)
		   & make_nvp("synapses",		   synapses)
		   & make_nvp("neurons",           neurons)
		   & make_nvp("layer1",            layer1)
		   & make_nvp("synapse_switches",  synapse_switches)
		   & make_nvp("crossbar_switches", crossbar_switches)
		   & make_nvp("current_stimuli", current_stimuli);
	}

};

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::HICANNData, 1)
