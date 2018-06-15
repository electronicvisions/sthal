#include "sthal/HICANNData.h"

namespace sthal {

HICANNData::HICANNData()
{}

HICANNData::~HICANNData()
{
}

void HICANNData::copy(const HICANNData & other)
{
	this->operator=(other);
}

bool operator==(const HICANNData & a, const HICANNData & b)
{
	return
		   a.floating_gates    == b.floating_gates
		&& a.analog            == b.analog
		&& a.repeater          == b.repeater
		&& a.synapses          == b.synapses
		&& a.neurons           == b.neurons
		&& a.layer1            == b.layer1
		&& a.synapse_switches  == b.synapse_switches
		&& a.crossbar_switches == b.crossbar_switches;
}

} // end namespace sthal
