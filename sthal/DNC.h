#pragma once

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "hal/Coordinate/HMFGeometry.h"
#include "sthal/macros.h"

namespace sthal {

class HICANN;

class DNC
{
	typedef boost::shared_ptr<HICANN>      hicann_t;
	typedef ::HMF::Coordinate::HICANNOnDNC hicann_coord;

public:
	STHAL_ARRAY_OPERATOR(hicann_t, hicann_coord, return mHICANNs[ii.id()];)

	std::vector<hicann_coord> getAllocatedHicanns() const;

private:
	std::array<hicann_t, hicann_coord::enum_type::size> mHICANNs;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("hicanns", mHICANNs);
	}

	friend std::ostream& operator<<(std::ostream& out, DNC const& obj);
};

} // end namespace sthal

#include "sthal/macros_undef.h"
