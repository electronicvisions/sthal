#include "sthal/FloatingGates.h"

#include "hal/Coordinate/iter_all.h"

using namespace ::HMF::Coordinate;

namespace sthal {

	FloatingGates::FloatingGates() :
		::HMF::HICANN::FGControl()
	{
		setDefaultFGConfig();
	}

	FloatingGates::FloatingGates(const ::HMF::HICANN::FGControl & other) :
		::HMF::HICANN::FGControl(other)
	{
		setDefaultFGConfig();
	}

	FloatingGates & FloatingGates::operator=(
			const ::HMF::HICANN::FGControl& other)
	{
		if (&other != this)
		{
			static_cast< ::HMF::HICANN::FGControl & >(*this) = other;
			mFGConfigs.clear();
			setDefaultFGConfig();
		}
		return *this;
	}

	bool operator== (FloatingGates const& a, FloatingGates const& b)
	{
		return static_cast< const ::HMF::HICANN::FGControl & >(a) ==
						static_cast< const ::HMF::HICANN::FGControl & >(b)
						&& a.mFGConfigs == b.mFGConfigs;
	}

	bool operator!= (FloatingGates const& a, FloatingGates const& b)
	{
		return !(a==b);
	}

	void FloatingGates::setFGConfig(Enum pass, FGConfig cfg)
	{
		mFGConfigs.at(pass) = cfg;
	}

	const FGConfig &
	FloatingGates::getFGConfig(Enum pass) const
	{
		return mFGConfigs.at(pass);
	}

	void FloatingGates::setNoProgrammingPasses(Enum passes)
	{
		mFGConfigs.resize(passes);
	}

	Enum FloatingGates::getNoProgrammingPasses() const
	{
		return Enum(mFGConfigs.size());
	}

	void FloatingGates::setDefaultFGConfig()
	{
		::HMF::HICANN::FGConfig cfg1;
		::HMF::HICANN::FGConfig cfg2;
		cfg2.maxcycle = 10;
		cfg2.readtime = 63;
		cfg2.acceleratorstep = 63;
		cfg2.voltagewritetime = 5;
		cfg2.currentwritetime = 20;
		cfg2.pulselength = 1;
		mFGConfigs = std::vector<FGConfig>{
			FGConfig(cfg1, FGConfig::WRITE_DOWN),
			FGConfig(cfg1, FGConfig::WRITE_UP),
			FGConfig(cfg2, FGConfig::WRITE_DOWN),
			FGConfig(cfg2, FGConfig::WRITE_UP)};
	}
} // end namespace sthal
