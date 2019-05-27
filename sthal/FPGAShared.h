#pragma once

#include <stdint.h>
#include <boost/serialization/nvp.hpp>

namespace sthal {

// Common data for all FPGAs
class FPGAShared
{
public:
	/// constructor.
	/// sets the FPGA HICANN delay to the default value
	FPGAShared();
	~FPGAShared();

	/// pll frequency of the hicann, musst be 50.0e6, 100.0e6, 150.0e6, 200.0e6 or 250.0e6
	void setPLL(double freq);
	double getPLL() const;

	/// set FPGA HICANN delay in fpga clock cycles (8ns)
	void setFPGAHICANNDelay(uint16_t delay);

	/// get FPGA HICANN delay in fpga clock cycles (8ns)
	uint16_t getFPGAHICANNDelay() const;

	friend bool operator==(const FPGAShared & a, const FPGAShared & b);
	friend bool operator!=(const FPGAShared & a, const FPGAShared & b);

private:
	/// pll frequency of the hicann, musst be 50.0e6, 100.0e6, 150.0e6, 200.0e6 or 250.0e6
	double pll_freq;

	/// FPGA HICANN delay in fpga clock cycles (8ns)
	uint16_t fpga_hicann_delay;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("pll_freq", pll_freq)
		   & make_nvp("fpga_hicann_delay", fpga_hicann_delay);
	}
};

} // end namespace sthal
