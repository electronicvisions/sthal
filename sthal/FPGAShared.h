#pragma once

#include <stdint.h>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/version.hpp>

namespace sthal {

// Common data for all FPGAs
class FPGAShared
{
public:
	/// constructor.
	/// sets the FPGA HICANN delay to the default value
	FPGAShared();
	~FPGAShared();

	/// PLL frequency (Hz) of the HICANNs associated to this FPGA
	/// Must be 100, 125, 150, 175, 200, 225 or 250e6
	void setPLL(double freq);
	double getPLL() const;

	/// set FPGA HICANN delay in fpga clock cycles (8ns)
	void setFPGAHICANNDelay(uint16_t delay);

	/// get FPGA HICANN delay in fpga clock cycles (8ns)
	uint16_t getFPGAHICANNDelay() const;

	/// set or unset reset of synapse array during FPGA init
	void setSynapseArrayReset(bool reset);
	bool getSynapseArrayReset() const;

	friend bool operator==(const FPGAShared & a, const FPGAShared & b);
	friend bool operator!=(const FPGAShared & a, const FPGAShared & b);

private:
	/// PLL frequency (Hz) of the HICANNs associated to this FPGA
	double pll_freq;

	/// FPGA HICANN delay in fpga clock cycles (8ns)
	uint16_t fpga_hicann_delay;

	/// Reset synapse array during FPGA init
	bool reset_synapse_array;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("pll_freq", pll_freq)
		   & make_nvp("fpga_hicann_delay", fpga_hicann_delay);
		if (version >= 1) {
			ar & make_nvp("reset_synapse_array", reset_synapse_array);
		}
	}
};

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::FPGAShared, 1)
