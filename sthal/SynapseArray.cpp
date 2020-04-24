#include "SynapseArray.h"
#include "halco/common/iter_all.h"

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

::HMF::HICANN::L1Address SynapseArray::get_address(halco::hicann::v2::SynapseOnHICANN const& s) const
{
	driver_type const& synapse_driver = (*this)[s.toSynapseDriverOnHICANN()];
	::HMF::HICANN::RowConfig const& row_config =
	    synapse_driver[s.toSynapseRowOnHICANN().toRowOnSynapseDriver()];

	::HMF::HICANN::DriverDecoder const& driver_decoder = row_config.get_decoder(
	    s.toSynapseColumnOnHICANN().toEnum().value() % 2 == 0 ? halco::common::top
	                                                          : halco::common::bottom);

	::HMF::HICANN::SynapseDecoder const& synapse_decoder = (*this)[s].decoder;

	return ::HMF::HICANN::L1Address(driver_decoder, synapse_decoder);
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
	for (auto const& syn : halco::common::iter_all<SynapseArray::synapse_coordinate>())
	{
		SynapseConstProxy const proxy = a[syn];
		if (proxy.weight.value() > 0)
			os << "\t(" << syn << ": W:" << proxy.weight << ", D:" << proxy.decoder << ")" << std::endl;
	}

	os << "active SyanpseDrivers: " << std::endl;
	for (auto const& d : halco::common::iter_all<SynapseArray::driver_coordinate>())
	{
		SynapseArray::driver_type const& drv = a[d];
		if (drv.is_enabled())
			os << "\t(" << d << ": " << drv << ")" << std::endl;;
	}
	return os;
}


} // end namespace sthal
