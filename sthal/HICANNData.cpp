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

template<typename Archiver>
void HICANNData::serialize(Archiver & ar, unsigned int const version)
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

} // end namespace sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::HICANNData)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::HICANNData)
