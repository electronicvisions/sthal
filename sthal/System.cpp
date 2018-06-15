#include "sthal/System.h"

#include <boost/make_shared.hpp>

#include "sthal/ExperimentRunner.h"
#include "sthal/HardwareDatabase.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/FPGA.h"

namespace sthal {

Wafer& System::operator[] (wafer_coord const& wafer)
{
	auto& ret = mWafers[wafer];
	if (!ret) {
		ret = boost::make_shared<Wafer>(wafer);
	}
	return *ret;
}

Wafer const& System::operator[] (wafer_coord const& wafer) const
{
	return *mWafers.at(wafer);
}

void System::connect(HardwareDatabase const& db)
{
	for (auto& wafer : mWafers) {
		wafer.second->connect(db);
	}
}

void System::disconnect()
{
	for (auto& wafer : mWafers) {
		wafer.second->disconnect();
	}
}

void System::configure(HICANNConfigurator& hicann)
{
	for (auto& wafer : mWafers) {
		wafer.second->configure(hicann);
	}
}

void System::start(ExperimentRunner& runner)
{
	for (auto& wafer : mWafers) {
		wafer.second->start(runner);
	}
}

size_t System::size() const
{
	return mWafers.size();
}

size_t System::capacity() const
{
	size_t cnt = 0;
	for (auto& wafer : mWafers) {
		cnt += wafer.second->capacity();
	}
	return cnt;
}

} // sthal
