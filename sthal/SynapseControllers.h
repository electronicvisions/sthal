#pragma once

#include <boost/serialization/export.hpp>

#include "halco/common/typed_array.h"
#include "halco/hicann/v2/synapse.h"
#include "sthal/SynapseControllerData.h"
#include "sthal/macros.h"

namespace sthal {
/*
 * Manages the synapse controllers, i.e. top and bottom controllers, of a HICANN chip.
 */
class SynapseControllers
{
public:
	typedef ::halco::hicann::v2::SynapseArrayOnHICANN syn_array_coord_t;

	STHAL_ARRAY_OPERATOR(SynapseControllerData, syn_array_coord_t, return m_synapse_controller[ii];)

	/*
	 * Enables dllreset for all synapse drivers
	 */
	void enable_dllreset();

	/*
	 * Disables dllreset for all synapse drivers
	 */
	void disable_dllreset();

	/*
	 * Returns true if dllreset is disabled for all synapse drivers
	 */
	bool is_dllreset_disabled() const;

	/*
	 * Returns true if dllreset is enabled for all synapse drivers
	 */
	bool is_dllreset_enabled() const;

	bool operator==(const SynapseControllers& other) const;
	bool operator!=(const SynapseControllers& other) const;

private:
	halco::common::typed_array<SynapseControllerData, syn_array_coord_t> m_synapse_controller;

	friend class boost::serialization::access;
	template <typename Archiver>
	void serialize(Archiver& ar, unsigned int const);

	friend std::ostream& operator<<(std::ostream& os, SynapseControllers const& c);
};

} // sthal

BOOST_CLASS_EXPORT_KEY(sthal::SynapseControllers)
