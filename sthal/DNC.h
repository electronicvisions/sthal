#pragma once

#include <vector>
#include <boost/shared_ptr.hpp>

#include "hal/Coordinate/HMFGeometry.h"
#include "sthal/macros.h"

namespace boost {
namespace serialization {
class access;
}
}

namespace sthal {

class HICANN;

class DNC
{
public:
	typedef boost::shared_ptr<HICANN>      hicann_t;
	typedef ::HMF::Coordinate::HICANNOnDNC hicann_coord;

	STHAL_ARRAY_OPERATOR(hicann_t, hicann_coord, return mHICANNs[ii.toEnum()];)

	std::vector<hicann_coord> getAllocatedHicanns() const;

	friend bool operator==(DNC const& a, DNC const& b);
	friend bool operator!=(DNC const& a, DNC const& b);

private:
	std::array<hicann_t, hicann_coord::enum_type::size> mHICANNs;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const version);

	friend std::ostream& operator<<(std::ostream& out, DNC const& obj);
};

} // end namespace sthal

#include "sthal/macros_undef.h" // STHAL_ARRAY_OPERATOR
BOOST_CLASS_TRACKING(sthal::DNC, boost::serialization::track_always)
