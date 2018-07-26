#include "sthal/ProgrammAndReadFloatingGatesConfigurator.h"

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
		"sthal.ProgrammAndReadFloatingGatesConfigurator");

using namespace ::HMF::Coordinate;

namespace sthal {

std::ostream & operator<<(
		std::ostream & out,
		const ProgrammAndReadFloatingGatesConfigurator::Result & r)
{
	boost::io::ios_flags_saver ifs(out);
	out << "Result:\n";
	out << std::setprecision(4);
    out << "Means: ";
	double x = sqrt(r.samples);
	for (size_t ii = 0; ii < r.means.size(); ++ii)
		out << ii << ": " <<  r.means[ii] << "+-" << r.variances[ii]/x << ", ";
	return out;
}

ProgrammAndReadFloatingGatesConfigurator::ProgrammAndReadFloatingGatesConfigurator(
		bool reset) :
	mReset(reset),
	mRecordTime(10e-6) // ~960 samples
{
}

void ProgrammAndReadFloatingGatesConfigurator::config_fpga(
	fpga_handle_t const& f, fpga_t const& fpga)
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

void ProgrammAndReadFloatingGatesConfigurator::config(
	fpga_handle_t const&, hicann_handle_t const& h, hicann_data_t const& hicann)
{
	if (mReset)
	{
		Timer t;
		LOG4CXX_DEBUG(logger, "fast init HICANN");
		::HMF::HICANN::init(*h, false);
		LOG4CXX_DEBUG(getTimeLogger(), "init HICANN took " << t.get_ms() << "ms");
	}

	std::map<row_t, double> rows;
	for (const Measurment & measurment : mMeasurments)
		rows[measurment.row] = 0.0;

	const FloatingGates& fg = hicann->floating_gates;
	size_t passes = fg.getNoProgrammingPasses();
	for (size_t pass = 0; pass < passes; ++pass)
	{
		LOG4CXX_INFO(logger, "writing FG blocks (pass " << pass + 1
			<< " out of " << passes << "): ");

		FGConfig cfg = fg.getFGConfig(Enum(pass));
		LOG4CXX_DEBUG(logger, cfg);
		for (auto block : iter_all<FGBlockOnHICANN>())
		{
			::HMF::HICANN::set_fg_config(*h, block, cfg);
		}
		for (auto & item : rows)
		{
			Timer t_write;
			::HMF::HICANN::set_fg_row_values(*h, item.first, fg, cfg.writeDown);
			item.second = t_write.get_ms();
			LOG4CXX_DEBUG(logger, "Wrote " << (cfg.writeDown ? "down " : "up ")
					<< item.first << " in " << item.second << "ms.");
		}

		if (!do_readout(pass, passes))
		{
			continue;
		}

		Timer t;
		LOG4CXX_INFO(logger, "Measure floating gates.");
		for (Measurment & measurment : mMeasurments)
		{
			Timer t_row;
			FGBlockOnHICANN block = measurment.block;
			FGRowOnFGBlock row = measurment.row;

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
			AnalogRecorder record = dynamic_cast<HICANN&>(*hicann).analogRecorder(analog);

			Result result;
			for (size_t x = 0; x < 129; ++x)
			{
				namespace ba = boost::accumulators;

				FGCellOnFGBlock cell(X(x), row);
				::HMF::HICANN::set_fg_cell(*h, block, cell);
				flush_hicann(h);
				ba::accumulator_set<AnalogRecorder::voltage_type,
				                    ba::stats<ba::tag::mean, ba::tag::variance> > acc;
				record.record(10e-6); // ~960 samples
				std::vector<AnalogRecorder::voltage_type> trace = record.trace();
				for (AnalogRecorder::voltage_type value : trace)
				{
					acc(value);
				}
				result.samples = trace.size();
				result.means.at(x) = ba::mean(acc);
				result.variances.at(x) = ba::variance(acc);
				LOG4CXX_TRACE(getLogger(), "Measured: " << result.means[x]
						<< " +- " << result.variances[x] << "mV for cell " << cell);
			}
			result.write_time = rows.at(row);
			measurment.results.push_back(result);
			LOG4CXX_DEBUG(getTimeLogger(), "reading " << block << "/" << row
					<< " took " << t_row.get_ms() << "ms");
		}
		LOG4CXX_INFO(getTimeLogger(),
					  "reading FG blocks took " << t.get_ms() << "ms");
	}
}

bool ProgrammAndReadFloatingGatesConfigurator::do_readout(size_t, size_t)
{
	return true;
}

void ProgrammAndReadFloatingGatesConfigurator::doReset(bool reset)
{
	mReset = reset;
}

void ProgrammAndReadFloatingGatesConfigurator::clear()
{
	mMeasurments.clear();
}

void ProgrammAndReadFloatingGatesConfigurator::meassure_all()
{
	for (auto block : iter_all<FGBlockOnHICANN>())
	{
		for (auto row : iter_all<FGRowOnFGBlock>())
		{
			mMeasurments.push_back({block, row, std::vector<Result>()});
		}
	}
}

void ProgrammAndReadFloatingGatesConfigurator::meassure(row_t row)
{
	for (auto block : iter_all<FGBlockOnHICANN>())
	{
		mMeasurments.push_back({block, row, std::vector<Result>()});
	}
}

void ProgrammAndReadFloatingGatesConfigurator::meassure(block_t block, row_t row)
{
	mMeasurments.push_back({block, row, std::vector<Result>()});
}

ProgrammAndReadFloatingGatesConfigurator::Measurment
ProgrammAndReadFloatingGatesConfigurator::getMeasurment(
		block_t block, row_t row) const
{
	auto it = std::find_if(mMeasurments.begin(), mMeasurments.end(),
			 [block, row](const Measurment & m) {
			 return m.block == block && m.row == row; });
	if (it != mMeasurments.end())
		return *it;
	else
		throw std::out_of_range("Invalid Measurment");
}

} // end namespace sthal

