#pragma once

#include "pywrap/compat/macros.hpp"
#include "halco/hicann/v2/l1.h"
#include "sthal/ParallelHICANNv4Configurator.h"

namespace sthal {

 /**
 * @brief Configurator that only locks repeater blocks which were specified during construction or
 *        explicitly added with the member function add_repeater_block().
 */
class ParallelOnlyRepeaterLockingNoResetNoFGConfigurator : public ParallelHICANNv4Configurator
{
public:
	/**
	 * Constructor
	 *
	 * @param rbs	Repeater blocks which should be relocked when config() is called.
	 *
	 */
	ParallelOnlyRepeaterLockingNoResetNoFGConfigurator(
		std::set<::halco::hicann::v2::RepeaterBlockOnWafer> rbs);

	void add_repeater_block(::halco::hicann::v2::RepeaterBlockOnWafer rb);

	void config(fpga_handle_t const& f,
	            hicann_handles_t const& handles,
	            hicann_datas_t const& hicanns,
	            ConfigurationStage stage) PYPP_OVERRIDE;

	// disable all member functions which are directly called in Wafer::configure()
	void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
	void prime_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	void start_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	void disable_global(fpga_handle_t const& f) PYPP_OVERRIDE;

private:
	std::map<::halco::hicann::v2::HICANNOnWafer, std::set<::halco::hicann::v2::RepeaterBlockOnHICANN>>
		m_repeater_blocks;
};

} // end namespace sthal
