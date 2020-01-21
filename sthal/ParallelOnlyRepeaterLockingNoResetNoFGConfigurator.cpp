#include "sthal/ParallelOnlyRepeaterLockingNoResetNoFGConfigurator.h"

#include <log4cxx/logger.h>

#include "hal/Handle/FPGA.h"
#include "hal/backend/HICANNBackend.h"
#include "halco/hicann/v2/format_helper.h"

#include "sthal/Timer.h"
#include "sthal/HICANN.h"

using namespace ::halco::hicann::v2;

namespace sthal {

ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::ParallelOnlyRepeaterLockingNoResetNoFGConfigurator(
	std::set<::halco::hicann::v2::RepeaterBlockOnWafer> rbs)
{
	for (auto rb : rbs) {
		add_repeater_block(rb);
	}
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::add_repeater_block(
	::halco::hicann::v2::RepeaterBlockOnWafer rb)
{
	m_repeater_blocks[rb.toHICANNOnWafer()].insert(rb.toRepeaterBlockOnHICANN());
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::config(fpga_handle_t const& f,
                                                                hicann_handles_t const& handles,
                                                                hicann_datas_t const& hicanns,
                                                                ConfigurationStage stage)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");

	LOG4CXX_DEBUG(HICANNConfigurator::getLogger(),
	              short_format(f->coordinate())
	              	<< " ParallelOnlyRepeaterLockingNoResetNoFGConfigurator stage "
	              	<< static_cast<size_t>(stage));

	switch (stage) {
		case ConfigurationStage::TIMING_UNCRITICAL: {
			auto it_data = hicanns.begin();

			for (auto handle : handles) {
				auto& hicann = *it_data;

				// continue with next HICANN if no relocking is needed for this HICANN
				if (m_repeater_blocks.find(handle->coordinate().toHICANNOnWafer())
				    == m_repeater_blocks.end()) {
					continue;
				}
					auto rbs_to_relock = m_repeater_blocks[handle->coordinate().toHICANNOnWafer()];

				// write configuration in HICANNData object
				for (auto rb : rbs_to_relock) {
					LOG4CXX_DEBUG(getLogger(),
					              short_format(handle->coordinate())
					              	<< ": pull dllreset for repeater block " << rb);
					hicann->repeater[rb].dllresetb = false;
				}

				// write configuration to hardware
				config_repeater_blocks(handle, hicann);

				++it_data;
			}
			break;
		}
		case ConfigurationStage::LOCKING_REPEATER_BLOCKS: {
			auto it_data = hicanns.begin();

			for (auto handle : handles) {
				auto& hicann = *it_data;

				// continue with next HICANN if no relocking is needed for this HICANN
				if (m_repeater_blocks.find(handle->coordinate().toHICANNOnWafer())
				    == m_repeater_blocks.end()) {
					continue;
				}
					auto rbs_to_relock = m_repeater_blocks[handle->coordinate().toHICANNOnWafer()];

				// write configuration in HICANNData object
				for (auto rb : rbs_to_relock) {
					LOG4CXX_DEBUG(getLogger(),
                                  short_format(handle->coordinate())
                                  	<< ": release dllreset for repeater block" << rb);
					hicann->repeater[rb].dllresetb = true;
				}

				// write configuration to hardware
				config_repeater_blocks(handle, hicann);

				++it_data;
			}
			break;
		}
		case ConfigurationStage::LOCKING_SYNAPSE_DRIVERS: {
			LOG4CXX_DEBUG(HICANNConfigurator::getLogger(), "- skip this stage");
			break;
		}
		case ConfigurationStage::NEURONS: {
			LOG4CXX_DEBUG(HICANNConfigurator::getLogger(), "- skip this stage");
			break;
		}
		default: {
			std::stringstream error_msg;
			error_msg << "Unhandled configuration stage " << static_cast<size_t>(stage);
			LOG4CXX_ERROR(getLogger(), error_msg.str());
			throw std::runtime_error(error_msg.str());
		}
	}

	LOG4CXX_INFO(getLogger(), "finished configuring HICANNs on FPGA "
	                          	<< short_format(f->coordinate()) << " for stage "
	                          	<< static_cast<size_t>(stage) << " in " << t.get_ms() << "ms");
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::config_fpga(fpga_handle_t const&,
                                                                     fpga_t const&)
{
	LOG4CXX_INFO(
		getLogger(),
		"Skipping reset and init of FPGA in ParallelOnlyRepeaterLockingNoResetNoFGConfigurator");
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::prime_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(
		getLogger(),
		"Skipping prime of systime counter in ParallelOnlyRepeaterLockingNoResetNoFGConfigurator");
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::start_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(
		getLogger(),
		"Skipping prime of start counter in ParallelOnlyRepeaterLockingNoResetNoFGConfigurator");
}

void ParallelOnlyRepeaterLockingNoResetNoFGConfigurator::disable_global(fpga_handle_t const&)
{
	LOG4CXX_INFO(
		getLogger(),
		"Skipping disable global in ParallelOnlyRepeaterLockingNoResetNoFGConfigurator");
}

} // end namespace sthal
