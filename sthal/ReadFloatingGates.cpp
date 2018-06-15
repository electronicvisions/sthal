#include "sthal/ReadFloatingGates.h"

#include <algorithm>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "hal/backend/HICANNBackend.h"
#include "hal/backend/FPGABackend.h"
#include "hal/backend/DNCBackend.h"
#include "hal/HICANN/FGBlock.h"
#include "hal/Handle/HICANN.h"
#include "hal/Handle/FPGA.h"
#include "hal/Coordinate/iter_all.h"

#include "sthal/HICANN.h"
#include "sthal/FPGA.h"
#include "sthal/Timer.h"
#include "sthal/AnalogRecorder.h"
#include "sthal/Wafer.h"

#include <log4cxx/logger.h>
#include <iomanip>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger(
		"sthal.ReadFloatingGates");

using namespace ::HMF::Coordinate;

namespace sthal {

const size_t ReadFloatingGates::number_of_cells;

ReadFloatingGates::ReadFloatingGates(bool do_reset, bool do_read_default) :
	mReset(do_reset),
	mRecordTime(1.0e-4) // ~9600 samples
{
	std::fill(mDoRead.begin(), mDoRead.end(), do_read_default);
	std::fill(mMean.begin(), mMean.end(), -1.0);
	std::fill(mVariance.begin(), mVariance.end(), -1.0);
}

static size_t idx(FGBlockOnHICANN block, FGCellOnFGBlock cell)
{
	return block.id().value() * FGCellOnFGBlock::enum_type::size + cell.id().value();
}

void ReadFloatingGates::setReadCell(block_t block, cell_t cell, bool read)
{
	size_t ii = idx(block, cell);
	mDoRead[ii] = read;
}

double ReadFloatingGates::getMean(block_t block, cell_t cell) const
{
	size_t ii = idx(block, cell);
	return mMean[ii];
}

double ReadFloatingGates::getVariance(block_t block, cell_t cell) const
{
	size_t ii = idx(block, cell);
	return mVariance[ii];
}

void ReadFloatingGates::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	if (mReset)
	{
		auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
		LOG4CXX_INFO(logger, "reset FPGA: " << f->coordinate());
		::HMF::FPGA::Reset r;
		r.PLL_frequency = static_cast<uint8_t>(fpga->commonFPGASettings()->getPLL() / 1.0e6);
		::HMF::FPGA::reset(*f, r);
	}
}

void ReadFloatingGates::config(
	fpga_handle_t const&, hicann_handle_t const& h, hicann_data_t const& hicann)
{
	if (mReset)
	{
		Timer t;
		LOG4CXX_DEBUG(logger, "fast init HICANN");
		::HMF::HICANN::init(*h, false);
		LOG4CXX_DEBUG(getTimeLogger(), "init HICANN took " << t.get_ms() << "ms");
	}

	Timer t;
	LOG4CXX_INFO(logger, "Measure floating gates.");

	for (auto block : iter_all<FGBlockOnHICANN>())
	{
		AnalogOnHICANN analog;
		::HMF::HICANN::Analog ac;
		if (block.y() == top)
			analog = AnalogOnHICANN(0);
		else
			analog = AnalogOnHICANN(1);
		if (block.x() == left)
			ac.set_fg_left(analog);
		else
			ac.set_fg_right(analog);

		::HMF::HICANN::set_analog(*h, ac);
		AnalogRecorder record = dynamic_cast<const HICANN&>(*hicann).analogRecorder(analog);

		for (cell_t cell : iter_all<cell_t>())
		{
			namespace ba = boost::accumulators;

			const size_t cell_id = idx(block, cell);
			if (!mDoRead[cell_id])
				continue;

			::HMF::HICANN::set_fg_cell(*h, block, cell);
			flush_hicann(h);

			record.record(mRecordTime);
			ba::accumulator_set<AnalogRecorder::voltage_type,
			                    ba::stats<ba::tag::mean, ba::tag::variance> > acc;
			for (AnalogRecorder::voltage_type value : record.trace())
			{
				acc(value);
			}

			mMean[cell_id] = ba::mean(acc);
			mVariance[cell_id] = ba::variance(acc);
			LOG4CXX_TRACE(logger, "Measured: "
				<< mMean[cell_id] << " +- " << mVariance[cell_id]
				<< "mV for cell " << cell << " on " << block);
		}
	}
	LOG4CXX_INFO(logger, "reading FG blocks took " << t.get_ms() << "ms");
}


} // end namespace sthal

