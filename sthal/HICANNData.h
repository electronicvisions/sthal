#pragma once

#include <boost/serialization/export.hpp>
#include <boost/serialization/version.hpp>

#include "halco/hicann/v2/fwd.h"
#include "hal/HICANNContainer.h"
#include "hal/DNCContainer.h"

#include "sthal/CrossbarSwitches.h"
#include "sthal/SynapseSwitches.h"
#include "sthal/FloatingGates.h"
#include "sthal/Layer1.h"
#include "sthal/L1Repeaters.h"
#include "sthal/Neurons.h"
#include "sthal/SynapseArray.h"
#include "sthal/FGStimulus.h"
#include "sthal/SynapseControllers.h"

namespace sthal {

typedef ::HMF::HICANN::Analog AnalogOutput;

class AnalogRecorder;

// HICANN chip
struct HICANNData
{
public:
	HICANNData();
	virtual ~HICANNData();

	FloatingGates      floating_gates;
	AnalogOutput       analog;
	L1Repeaters        repeater;
	SynapseArray       synapses;
	Neurons            neurons;
	Layer1             layer1;
	SynapseControllers synapse_controllers;

	SynapseSwitches  synapse_switches;
	CrossbarSwitches crossbar_switches;

	std::array<FGStimulus, ::halco::hicann::v2::FGBlockOnHICANN::enum_type::size> current_stimuli;

	/// Copy HICANN configuration from an other HICANN, needed from python
	void copy(const HICANNData & other);

	friend bool operator==(const HICANNData & a, const HICANNData & b);

	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version);

};

} // end namespace sthal

BOOST_CLASS_EXPORT_KEY(sthal::HICANNData)
BOOST_CLASS_VERSION(sthal::HICANNData, 2)
