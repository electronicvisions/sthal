#pragma once

#include "sthal/HICANNConfigurator.h"
#include "hal/Coordinate/HMFGeometry.h"

#include "boost/io/ios_state.hpp"

namespace sthal {

class HICANN;
class Wafer;

class ProgrammAndReadFloatingGatesConfigurator : public HICANNConfigurator
{
public:
	typedef ::HMF::Coordinate::FGBlockOnHICANN block_t;
	typedef ::HMF::Coordinate::FGRowOnFGBlock row_t;
	typedef ::HMF::Coordinate::Enum Enum;

	struct Result
	{
		std::array<double, 129> means;
		std::array<double, 129> variances;
		size_t samples;
		double write_time; // For rows in all 4 blocks!

		friend std::ostream & operator<<(std::ostream & out, const Result & r);
	};

	struct Measurment
	{
		block_t block;
		row_t row;
		std::vector<Result> results;
	};

	ProgrammAndReadFloatingGatesConfigurator(bool do_reset=true);

	void doReset(bool reset);
	void clear();
	void meassure_all();
	void meassure(row_t);
	void meassure(block_t, row_t);

	Measurment getMeasurment(block_t block, row_t row) const;

	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
	virtual void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg);

	virtual bool do_readout(size_t pass, size_t passes);

private:
	bool mReset;
	double mRecordTime;
	std::vector<Measurment> mMeasurments;
};

} // end namespace sthal
