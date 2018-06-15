#pragma once

#include "pywrap/compat/macros.hpp"

#include "sthal/HICANNConfigurator.h"

namespace sthal {

/**
 * @brief Configurator that does no resets and only configures analog readout and neurons.
 * This is useful if one only wants to present new external input spikes or switch the
 * firing denmem of a hardware neuron during one step of an experiment.
 */
class OnlyNeuronNoResetNoFGConfigurator : public HICANNConfigurator
{
public:
	void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg)
		PYPP_OVERRIDE;
	void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
};

} // end namespace sthal
