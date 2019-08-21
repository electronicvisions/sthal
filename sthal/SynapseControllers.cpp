#include "sthal/SynapseControllers.h"
#include "hal/Coordinate/iter_all.h"
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>

using namespace ::HMF::Coordinate;

namespace sthal {

SynapseControllers::SynapseControllers() :
	m_synapse_controller{
		{syn_controller_t(syn_array_coord_t(top)), syn_controller_t(syn_array_coord_t(bottom))}}
{}

void SynapseControllers::enable_dllreset()
{
	for (auto& synapse_controller : m_synapse_controller) {
		synapse_controller.cnfg_reg.enable_dllreset();
	}
}

void SynapseControllers::disable_dllreset()
{
	for (auto& synapse_controller : m_synapse_controller) {
		synapse_controller.cnfg_reg.disable_dllreset();
	}
}

bool SynapseControllers::is_dllreset_disabled() const {
	return std::all_of(std::cbegin(m_synapse_controller),
	                   std::cend(m_synapse_controller),
	                   [](auto const& sc){return sc.cnfg_reg.dllresetb
	                                        == ::HMF::HICANN::SynapseDllresetb::max;});
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
