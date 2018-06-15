#pragma once

#include "sthal/HICANNConfigurator.h"
#include "hal/Coordinate/HMFGeometry.h"

#include "boost/io/ios_state.hpp"

namespace sthal {

class HICANN;
class Wafer;

// FIXME: Why is there no "Configurator" suffix here?
class ReadFloatingGates : public HICANNConfigurator
{
public:
	typedef ::HMF::Coordinate::FGBlockOnHICANN block_t;
	typedef ::HMF::Coordinate::FGRowOnFGBlock row_t;
	typedef ::HMF::Coordinate::FGCellOnFGBlock cell_t;

	static const size_t number_of_cells = cell_t::enum_type::size * block_t::enum_type::size;

	ReadFloatingGates(bool do_reset=true, bool do_read_default=true);

	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fpga);
	virtual void config(
		fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& hicann);

	void setReadCell(block_t block, cell_t cell, bool read);
	double getMean(block_t, cell_t) const;
	double getVariance(block_t, cell_t) const;
private:
	typedef std::array<double, number_of_cells> result_t;

	bool mReset;
	double mRecordTime;

	result_t mMean;
	result_t mVariance;
	std::array<bool, number_of_cells> mDoRead;
};

} // end namespace sthal
