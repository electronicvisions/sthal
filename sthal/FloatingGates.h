#pragma once

#include <boost/serialization/vector.hpp>

#include "hal/HICANN/FGControl.h"
#include "hal/HICANN/FGStimulus.h"

#include "sthal/FGConfig.h"

namespace sthal {

// As simplificat
class FloatingGates : public ::HMF::HICANN::FGControl
{
public:
	typedef ::halco::hicann::v2::FGBlockOnHICANN FGBlockOnHICANN;
	typedef ::halco::common::Enum Enum;

	FloatingGates();
	FloatingGates(const ::HMF::HICANN::FGControl &);
	FloatingGates & operator=(const ::HMF::HICANN::FGControl &);

	PYPP_DEFAULT(FloatingGates(FloatingGates const&));
	PYPP_DEFAULT(FloatingGates& operator=(FloatingGates const&));

	friend bool operator== (FloatingGates const& a, FloatingGates const& b);
	friend bool operator!= (FloatingGates const& a, FloatingGates const& b);

	void setFGConfig(Enum pass, FGConfig cfg);
	const FGConfig & getFGConfig(Enum pass) const;

	void setNoProgrammingPasses(Enum passes);
	Enum getNoProgrammingPasses() const;

	void setDefaultFGConfig();
private:
	std::vector<FGConfig> mFGConfigs;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using namespace boost::serialization;
		ar & make_nvp("base", base_object< ::HMF::HICANN::FGControl >(*this));
		if (version < 1)
		{
			// TODO best I can do without making it terrible complicated
			setDefaultFGConfig();
		}
		else
		{
		   ar & make_nvp("fg_config", mFGConfigs);
		}
	}
};

} // end namespace sthal

BOOST_CLASS_VERSION(::sthal::FloatingGates, 1)
