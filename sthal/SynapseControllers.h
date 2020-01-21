#pragma once

#include <boost/serialization/export.hpp>

#include "sthal/macros.h"
#include "halco/hicann/v2/synapse.h"
#include "hal/HICANNContainer.h"
#include "halco/common/typed_array.h"

namespace sthal {
/*
 * Manages the synapse controllers, i.e. top and bottom controllers, of a HICANN chip.
 */
class SynapseControllers
{
public:
	typedef ::HMF::HICANN::SynapseController syn_controller_t;
	typedef ::halco::hicann::v2::SynapseArrayOnHICANN syn_array_coord_t;

	STHAL_ARRAY_OPERATOR(syn_controller_t, syn_array_coord_t, return m_synapse_controller[ii];)

	void enable_dllreset();
	void disable_dllreset();
	bool is_dllreset_disabled() const;

	bool operator==(const SynapseControllers& other) const;
	bool operator!=(const SynapseControllers& other) const;

private:
	// represents current hardware state
	halco::common::typed_array<syn_controller_t, syn_array_coord_t> m_synapse_controller;

	friend class boost::serialization::access;
	template <typename Archiver>
	void serialize(Archiver& ar, unsigned int const);

	friend std::ostream& operator<<(std::ostream& os, SynapseControllers const& c);
};

} // sthal

BOOST_CLASS_EXPORT_KEY(sthal::SynapseControllers)
