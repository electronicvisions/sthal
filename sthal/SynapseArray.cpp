#include "SynapseArray.h"
#include "hal/Coordinate/iter_all.h"

namespace sthal {

const size_t SynapseArray::no_drivers;
const size_t SynapseArray::no_lines;

bool operator==(const SynapseArray & a, const SynapseArray & b)
{
	return a.drivers  == b.drivers
		&& a.weights  == b.weights
		&& a.decoders == b.decoders;
}

bool operator!=(const SynapseArray & a, const SynapseArray & b)
{
	return !(a == b);
}

void SynapseArray::clear_drivers()
{
	std::fill(drivers.begin(), drivers.end(), driver_type());
}

void SynapseArray::clear_synapses()
{
	set_all(::HMF::HICANN::SynapseDecoder(0), ::HMF::HICANN::SynapseWeight(0));
}

void SynapseArray::set_all(::HMF::HICANN::SynapseDecoder decoder, ::HMF::HICANN::SynapseWeight weight)
{
	for (auto & row : weights)
	{
		std::fill(row.begin(), row.end(), weight);
	}

	for (auto & doublerow : decoders)
	{
		for (auto & row : doublerow)
		{
			std::fill(row.begin(), row.end(), decoder);
		}
	}
}

std::ostream& operator<<(std::ostream& os, SynapseArray const& a)
{
	os << "active Synapses: " << std::endl;
	for (auto const& syn : HMF::Coordinate::iter_all<SynapseArray::synapse_coordinate>())
	{
		SynapseConstProxy const proxy = a[syn];
		if (proxy.weight.value() > 0)
			os << "\t(" << syn << ": W:" << proxy.weight << ", D:" << proxy.decoder << ")" << std::endl;
	}

	os << "active SyanpseDrivers: " << std::endl;
	for (auto const& d : HMF::Coordinate::iter_all<SynapseArray::driver_coordinate>())
	{
		SynapseArray::driver_type const& drv = a[d];
		if (drv.is_enabled())
			os << "\t(" << d << ": " << drv << ")" << std::endl;;
	}
	return os;
}


} // end namespace sthal
