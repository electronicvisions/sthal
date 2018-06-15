#pragma once

#include "hal/HICANN/L1Address.h"
#include <boost/serialization/shared_ptr.hpp>

namespace sthal {

struct Spike
{
	typedef ::HMF::HICANN::L1Address L1Address;
	explicit Spike(L1Address a, double t) :
		addr(a), time(t) {}

	explicit Spike() :
		addr(0), time(0.0) {}

	// Avoid implict converision of builtins
	PYPP_DELETE(template <typename T>\
	explicit Spike(L1Address a, T t));

	L1Address addr;
	double    time;

	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("addr", addr)
		   & make_nvp("time", time);
	}
};

typedef std::vector<Spike> SpikeVector;
PYPP_INSTANTIATE(std::vector<Spike>)

std::ostream& operator<<(std::ostream& out, Spike const& obj);
std::ostream& operator<<(std::ostream& out, SpikeVector const& obj);

} // end namespace sthal
