#include "test/CommandLineArgs.h"

#include "sthal/Wafer.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/MagicHardwareDatabase.h"

class SthalHWTest : public ::testing::Test
{
public:
	SthalHWTest() :
		wafer_c{::HMF::Coordinate::Enum{0}},
		hicann_c{getHICANNCoordinate()},
		wafer{wafer_c},
		hicann(allocateHICANN(wafer, hicann_c))
	{}

	virtual void SetUp() {
	}

	virtual void TearDown() {
	}

private:
	::HMF::Coordinate::Wafer         wafer_c;
	::HMF::Coordinate::HICANNOnWafer hicann_c;

	static ::HMF::Coordinate::HICANNOnWafer getHICANNCoordinate()
	{
		::HMF::Coordinate::HICANNOnDNC h = g_conn.h;
		::HMF::Coordinate::DNCGlobal   d = g_conn.d.toDNCOnWafer(g_conn.f);

		return h.toHICANNOnWafer(d);
	}

	static sthal::HICANN & allocateHICANN(sthal::Wafer & w, ::HMF::Coordinate::HICANNOnWafer h)
	{
		return w[h];
	}

protected:
	sthal::Wafer wafer;
	sthal::HICANN & hicann;
};
