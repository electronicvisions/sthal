#include "sthal/SynapseControllers.h"
#include "halco/common/iter_all.h"
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace sthal {

void SynapseControllers::enable_dllreset()
{
	for (auto& synapse_controller : m_synapse_controller) {
		synapse_controller.dllreset = true;
	}
}

void SynapseControllers::disable_dllreset()
{
	for (auto& synapse_controller : m_synapse_controller) {
		synapse_controller.dllreset = false;
	}
}

bool SynapseControllers::is_dllreset_disabled() const {
	return std::none_of(
	    std::cbegin(m_synapse_controller), std::cend(m_synapse_controller),
	    [](auto const& sc) { return sc.dllreset; });
}

bool SynapseControllers::is_dllreset_enabled() const
{
	return std::all_of(
	    std::cbegin(m_synapse_controller), std::cend(m_synapse_controller),
	    [](auto const& sc) { return sc.dllreset; });
}

bool SynapseControllers::operator==(const SynapseControllers& other) const
{
	return (m_synapse_controller == other.m_synapse_controller);
}

bool SynapseControllers::operator!=(const SynapseControllers& other) const
{
	return !(*this == other);
}

template <typename Archiver>
void SynapseControllers::serialize(Archiver& ar, unsigned int const)
{
	ar & boost::serialization::make_nvp("m_synapse_controller", m_synapse_controller);
}

std::ostream& operator<<(std::ostream& os, SynapseControllers const& c)
{
	os << "Synapse Controllers: ";


	for (auto addr : iter_all<SynapseControllers::syn_array_coord_t>()) {
		os << "Synapse Controller on synapse array " << addr << ":\n"
		   << c.m_synapse_controller[addr];
	}
	return os;
}
} // sthal

BOOST_CLASS_EXPORT_IMPLEMENT(::sthal::SynapseControllers)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(::sthal::SynapseControllers)
