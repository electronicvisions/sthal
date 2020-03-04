#include "FPGAShared.h"

#include <cmath>
#include <stdexcept>

namespace sthal {

/// Default HICANN clock delay. Value according to measurements in #1820
static const uint16_t fpga_hicann_delay_default = 25;

FPGAShared::FPGAShared():
	pll_freq(100e6),
	fpga_hicann_delay(fpga_hicann_delay_default),
	reset_synapse_array(false)
{
}

FPGAShared::~FPGAShared()
{
}

void FPGAShared::setPLL(double freq)
{
	if ( freq < 100e6 || freq > 250e6 || fmod(freq, 25e6) != 0.0)
	{
		throw std::invalid_argument("only 100, 125, 150, 175, 200, 225 "
				                    "and 250Mhz are supported for PLL");
	}
	// TODO add range check
	pll_freq = freq;
}

double FPGAShared::getPLL() const
{
	return pll_freq;
}

void FPGAShared::setFPGAHICANNDelay(uint16_t delay)
{
	fpga_hicann_delay = delay;
}

uint16_t FPGAShared::getFPGAHICANNDelay() const
{
	return fpga_hicann_delay;
}

void FPGAShared::setSynapseArrayReset(bool reset)
{
	reset_synapse_array = reset;
}

bool FPGAShared::getSynapseArrayReset() const
{
	return reset_synapse_array;
}

bool operator==(const FPGAShared & a, const FPGAShared & b)
{
	return (a.pll_freq          == b.pll_freq)
		&& (a.fpga_hicann_delay == b.fpga_hicann_delay)
		&& (a.reset_synapse_array == b.reset_synapse_array)
	;
}

bool operator!=(const FPGAShared & a, const FPGAShared & b)
{
	return !(a == b);
}

} // end namespace sthal
