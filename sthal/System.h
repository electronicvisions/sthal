#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/serialization/nvp.hpp>
#include <map>

#include "sthal/Wafer.h"
#include "sthal/HICANN.h"

#include "sthal/macros.h"

namespace sthal {

class HardwareDatabase;
class ExperimentRunner;
class HICANNConfigurator;

class System :
	private boost::noncopyable
{
public:
	typedef ::halco::hicann::v2::Wafer        wafer_coord;
	typedef ::halco::hicann::v2::HICANNGlobal hicann_coord;


	Wafer&       operator[] (wafer_coord const& wafer);
	Wafer const& operator[] (wafer_coord const& wafer) const;
	STHAL_ARRAY_OPERATOR(HICANN, hicann_coord,
			return (*this)[ii.toWafer()][ii.toHICANNOnWafer()];);

	/// Gets handle
	void connect(HardwareDatabase const& db);

	/// Free's handle
	void disconnect();

	/// Write complete configuration
	void configure(HICANNConfigurator& hicann);

	/// Write pulses and start experiment
	void start(ExperimentRunner& runner);

	/// returns number of allocated wafers
	size_t size() const;

	/// returns number of allocated denmems in the system (including defect ones)
	size_t capacity() const;

private:
	std::map<wafer_coord, boost::shared_ptr<Wafer> > mWafers;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("wafers", mWafers);
	}
};

} // sthal

#include "sthal/macros_undef.h"
