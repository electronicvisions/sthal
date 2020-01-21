#include "test/CommandLineArgs.h"

#include "sthal/Wafer.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/MagicHardwareDatabase.h"

class SthalHWTest : public ::testing::Test
{
public:
	SthalHWTest() :
		wafer_c{::halco::common::Enum{0}},
		hicann_c{getHICANNCoordinate()},
		wafer{wafer_c},
		hicann(allocateHICANN(wafer, hicann_c))
	{}

	virtual void SetUp() {
	}

	virtual void TearDown() {
	}

private:
	::halco::hicann::v2::Wafer         wafer_c;
	::halco::hicann::v2::HICANNOnWafer hicann_c;

	static ::halco::hicann::v2::HICANNOnWafer getHICANNCoordinate()
	{
		::halco::hicann::v2::HICANNOnDNC h = g_conn.h;
		::halco::hicann::v2::DNCGlobal   d = g_conn.d.toDNCOnWafer(g_conn.f);

		return h.toHICANNOnWafer(d);
	}

	static sthal::HICANN & allocateHICANN(sthal::Wafer & w, ::halco::hicann::v2::HICANNOnWafer h)
	{
		return w[h];
	}

protected:
	sthal::Wafer wafer;
	sthal::HICANN & hicann;
};
