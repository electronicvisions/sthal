#include "sthal/ParallelHICANNv4SmartConfigurator.h"

#include <mutex>
#include <boost/serialization/nvp.hpp>

#include "sthal/HICANN.h"
#include "sthal/Settings.h"
#include "sthal/Timer.h"
#include "sthal/Wafer.h"

#include "halco/common/iter_all.h"
#include "hal/backend/HICANNBackend.h"

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace sthal {

ParallelHICANNv4SmartConfigurator::ParallelHICANNv4SmartConfigurator() :
    fg_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    synapse_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    synapse_drv_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    reset_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    repeater_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    repeater_locking_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    syn_drv_locking_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    m_global_l1_bus_changes(true),
    m_started_systime(false)
{
	omp_init_lock(&mLock);
}

ParallelHICANNv4SmartConfigurator::~ParallelHICANNv4SmartConfigurator()
{
	omp_destroy_lock(&mLock);
}

boost::shared_ptr<ParallelHICANNv4SmartConfigurator> ParallelHICANNv4SmartConfigurator::create()
{
	return boost::shared_ptr<ParallelHICANNv4SmartConfigurator>(
	    new ParallelHICANNv4SmartConfigurator);
}

void ParallelHICANNv4SmartConfigurator::config(
    fpga_handle_t const& fpga_handle,
    hicann_handles_t const& hicann_handle,
    hicann_datas_t const& hicann_data,
    ConfigurationStage stage)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	if (stage == ConfigurationStage::INIT) {
		// Inserting/Deleting elements from std::map during concurrent operation may lead to
		// undefined behaviour. Therefore, we pre-populate mWrittenHICANNData with nullptr.
		omp_set_lock(&mLock); // Asserts single thread execution
		for (auto& handle : hicann_handle) {
			if (mWrittenHICANNData.find(handle->coordinate()) == mWrittenHICANNData.end()) {
				mWrittenHICANNData[handle->coordinate()] = nullptr;
			}
		}
		omp_unset_lock(&mLock);
		return;
	}

	// locking unrelated stages will always be called. The execution of different configuration
	// procedures is then handeled by the member functions of this configurator.
	bool const locking_stage = (stage == ConfigurationStage::LOCKING_REPEATER_BLOCKS)
	                            || (stage == ConfigurationStage::LOCKING_SYNAPSE_DRIVERS);
	// whether stage LOCKING_REPEATER_BLOCKS is to be executed
	bool const repeater_locking = ((stage == ConfigurationStage::LOCKING_REPEATER_BLOCKS)
	                                && repeater_locking_wanted());
	// whether stage LOCKING_SYNAPSE_DRIVERS is to be executed
	bool const syn_drv_locking = ((stage == ConfigurationStage::LOCKING_SYNAPSE_DRIVERS)
	                               && syn_drv_locking_wanted_any(hicann_handle, hicann_data));

	if (!locking_stage || repeater_locking || syn_drv_locking) {
		ParallelHICANNv4Configurator::config(fpga_handle, hicann_handle, hicann_data, stage);
	} else {
		LOG4CXX_DEBUG(getLogger(), "Skip stage " << static_cast<size_t>(stage));
	}

	auto const& last_stage = Settings::get().configuration_stages.order.back();
	if (stage == last_stage) {
		LOG4CXX_DEBUG(getLogger(), "Saving old configuration");
		omp_set_lock(&mLock);
		set_hicanns(hicann_data, hicann_handle);
		omp_unset_lock(&mLock);
	}
	LOG4CXX_INFO(getTimeLogger(), "Smart configuration took " << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::config_floating_gates(
    hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");

	if (fg_config_mode == ConfigMode::Force) {
		LOG4CXX_INFO(getLogger(), "Forcing Floating Gate write, ignoring previous configuration");
		return ParallelHICANNv4Configurator::config_floating_gates(handles, hicanns);
	}

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	hicann_handles_t changed_handles;
	hicann_datas_t changed_hicanns;

	// collect changed hicann handles/datas, skip config for others
	for (size_t ii = 0; ii != hicanns.size(); ++ii) {
		const hicann_coord coord = handles[ii]->coordinate();
		const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);
		if (!(fg_config_mode == ConfigMode::Skip) &&
		    (old_hicann == nullptr || old_hicann->floating_gates != hicanns[ii]->floating_gates)) {
			changed_handles.push_back(handles[ii]);
			changed_hicanns.push_back(hicanns[ii]);
		} else {
			const size_t passes = hicanns[ii]->floating_gates.getNoProgrammingPasses();
			if (passes > 0) {
				size_t pass = passes - 1;
				LOG4CXX_DEBUG(
				    getLogger(), "skipping FG config for pass " << pass << " of HICANN " << ii);
				FloatingGates const& fg = hicanns[ii]->floating_gates;
				FGConfig cfg = fg.getFGConfig(Enum(pass));
				for (auto block : iter_all<FGBlockOnHICANN>()) {
					::HMF::HICANN::set_fg_config(*handles[ii], block, cfg);
				}
			}
		}
	}

	LOG4CXX_DEBUG(
	    getLogger(), "Smartly configuring FG, skipping " +
	                     std::to_string(handles.size() - changed_handles.size()) +
	                     " configurations");

	if (changed_handles.size() != 0) {
		ParallelHICANNv4Configurator::config_floating_gates(changed_handles, changed_hicanns);
	}

	LOG4CXX_INFO(getTimeLogger(), "Writing FG blocks took " << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::config_synapse_array(
    hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");

	if (synapse_config_mode == ConfigMode::Force) {
		LOG4CXX_INFO(getLogger(), "Forcing synapse write, ignoring previous configuration");
		return ParallelHICANNv4Configurator::config_synapse_array(handles, hicanns);
	}

	if (mWrittenHICANNData.empty()) {
		LOG4CXX_INFO(getLogger(), "Writing all synapses as no previous configuration was found");
		return ParallelHICANNv4Configurator::config_synapse_array(handles, hicanns);
	}

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	const size_t n_hicanns = hicanns.size();

	hicann_handles_t all_changed_handles;
	hicann_datas_t all_changed_hicanns;

	for (size_t ii = 0; ii != n_hicanns; ++ii) {
		const hicann_coord coord = handles[ii]->coordinate();
		const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);
		if (old_hicann == nullptr || old_hicann->synapses != hicanns[ii]->synapses) {
			all_changed_handles.push_back(handles[ii]);
			all_changed_hicanns.push_back(hicanns[ii]);
		}
	}

	const size_t n_all_changed_hicanns = all_changed_hicanns.size();

	LOG4CXX_DEBUG(
	    getLogger(), "Smartly configure synapses in parallel for "
	                     << n_all_changed_hicanns << " HICANN(s), skipping "
	                     << n_hicanns - n_all_changed_hicanns << " HICANN(s)");

	// TODO: interleave calls to top / bottom synapse controllers
	if (n_all_changed_hicanns != 0) {
		for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
			std::vector<HMF::HICANN::DecoderDoubleRow> decoder_data;
			std::vector<HMF::HICANN::SynapseController> synapse_controllers_dec;
			decoder_data.reserve(n_all_changed_hicanns);
			synapse_controllers_dec.reserve(n_all_changed_hicanns);
			hicann_handles_t drv_changed_handles;
			for (size_t ii = 0; ii != n_all_changed_hicanns; ++ii) {
				const hicann_coord coord = all_changed_handles[ii]->coordinate();
				const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);
				if (!(synapse_config_mode == ConfigMode::Skip) &&
				    (old_hicann == nullptr ||
				     old_hicann->synapses.getDecoderDoubleRow(syndrv) !=
				         all_changed_hicanns[ii]->synapses.getDecoderDoubleRow(syndrv))) {
					decoder_data.push_back(
					    all_changed_hicanns[ii]->synapses.getDecoderDoubleRow(syndrv));
					drv_changed_handles.push_back(all_changed_handles[ii]);
					HMF::HICANN::SynapseController const synapse_controller =
					    static_cast<HMF::HICANN::SynapseController>(
					        all_changed_hicanns[ii]->synapse_controllers[
					           syndrv.toSynapseArrayOnHICANN()]);
					synapse_controllers_dec.push_back(synapse_controller);
				}
			}

			set_decoder_double_row(drv_changed_handles, synapse_controllers_dec, syndrv, decoder_data);
			LOG4CXX_DEBUG(
			    getLogger(), "Smartly set decoder double row, skipped " +
			                     std::to_string(handles.size() - drv_changed_handles.size()) +
			                     " configurations");

			for (auto side : iter_all<SideVertical>()) {
				SynapseRowOnHICANN const synrow(syndrv, RowOnSynapseDriver(side));

				std::vector<HMF::HICANN::WeightRow> weight_data;
				hicann_handles_t row_changed_handles;
				std::vector<HMF::HICANN::SynapseController> synapse_controllers_weight;
				weight_data.reserve(n_all_changed_hicanns);
				row_changed_handles.reserve(n_all_changed_hicanns);
				synapse_controllers_weight.reserve(n_all_changed_hicanns);
				for (size_t ii = 0; ii != n_all_changed_hicanns; ++ii) {
					const hicann_coord coord = all_changed_handles[ii]->coordinate();
					const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);
					if (!(synapse_config_mode == ConfigMode::Skip) &&
					    (old_hicann == nullptr ||
					     old_hicann->synapses[synrow].weights !=
					         all_changed_hicanns[ii]->synapses[synrow].weights)) {
						weight_data.push_back(all_changed_hicanns[ii]->synapses[synrow].weights);
						row_changed_handles.push_back(all_changed_handles[ii]);
					    HMF::HICANN::SynapseController const synapse_controller =
						    static_cast<HMF::HICANN::SynapseController>(
						        all_changed_hicanns[ii]->synapse_controllers[
						            syndrv.toSynapseArrayOnHICANN()]);
						synapse_controllers_weight.push_back(synapse_controller);
					}
				}

				set_weights_row(row_changed_handles, synapse_controllers_weight, synrow, weight_data);
				LOG4CXX_DEBUG(
				    getLogger(), "Smartly configured synapse row " + std::to_string(synrow) +
				                     ", skipped " +
				                     std::to_string(handles.size() - row_changed_handles.size()) +
				                     " configurations");
			}
		}
	} else {
		LOG4CXX_INFO(
		    getLogger(), "Skipping synapse array configuration as no HICANNs have changed");
	}

	LOG4CXX_INFO(
	    getTimeLogger(), "Configure synapses in parallel for "
	                         << n_all_changed_hicanns << " HICANN(s) took " << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::config_synapse_drivers(
    hicann_handle_t const& h, hicann_data_t const& hicann)
{
	if (synapse_drv_config_mode == ConfigMode::Force) {
		LOG4CXX_INFO(getLogger(), "Forcing synapse driver write, ignoring previous configuration");
		return ParallelHICANNv4Configurator::config_synapse_drivers(h, hicann);
	}

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	const hicann_coord coord = h->coordinate();
	const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);

	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure synapse drivers");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		if (!(synapse_drv_config_mode == ConfigMode::Skip) &&
		    (old_hicann == nullptr || old_hicann->synapses[syndrv] != hicann->synapses[syndrv])) {
			::HMF::HICANN::set_synapse_driver(
			    *h,
			    static_cast< ::HMF::HICANN::SynapseController>(
			        hicann->synapse_controllers[syndrv.toSynapseArrayOnHICANN()]),
			    syndrv, hicann->synapses[syndrv]);
			LOG4CXX_DEBUG(getLogger(), "Configuring synapse driver");
		} else {
			LOG4CXX_DEBUG(getLogger(), "Skipping synapse driver configuration");
		}
	}
	LOG4CXX_INFO(
	    getTimeLogger(), short_format(h->coordinate())
	                         << ": configure synapse drivers took " << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::config_repeater(
    hicann_handle_t const& h, hicann_data_t const& hicann)
{
	if (repeater_config_mode == ConfigMode::Force) {
		LOG4CXX_INFO(getLogger(), "Forcing Repeater write, ignoring previous configuration");
		return ParallelHICANNv4Configurator::config_repeater(h, hicann);
	}

	const hicann_coord coord = h->coordinate();
	const hicann_data_t old_hicann = mWrittenHICANNData.at(coord);
	if (!(repeater_config_mode == ConfigMode::Skip) &&
	    (old_hicann == nullptr || old_hicann->repeater != hicann->repeater)) {
		return ParallelHICANNv4Configurator::config_repeater(h, hicann);
	} else {
		LOG4CXX_INFO(getLogger(), "Skipping Repeater configuration as nothing has changed");
	}
}

void ParallelHICANNv4SmartConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     mDidFPGAConfig.find(f->coordinate()) == mDidFPGAConfig.end())) {
		LOG4CXX_DEBUG(getLogger(), "Doing regular FPGA config");
		ParallelHICANNv4Configurator::config_fpga(f, fpga);
		omp_set_lock(&mLock);
		note_fpga_config(f->coordinate());
		// After configuring the FPGA, we have to restart the systime for all FPGAs
		m_started_systime = false;
		omp_unset_lock(&mLock);
		return;
	}
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(
	    getLogger(), "Skipping reset and init of FPGA since Wafer has already been initialized");
	if (fpga->getSpinnakerEnable())
		config_spinnaker_interface(f, fpga);
	config_dnc_link(f, fpga);
	LOG4CXX_INFO(
	    getTimeLogger(), "finished configuring DNC link of FPGA " << f->coordinate() << " in "
	                                                              << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::ensure_correct_l1_init_settings(
	hicann_handle_t const& h, hicann_data_t const& hicann)
{
	// check repeater blocks
	if (repeater_locking_wanted()) {
		// setup regular locking
		LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
			<< ": Setup regular locking of repeater blocks.");
		for (auto addr : iter_all<RepeaterBlockOnHICANN>()) {
			if (hicann->repeater[addr].dllresetb != false) {
				hicann->repeater[addr].dllresetb = false;
				LOG4CXX_WARN(getLogger(),
					addr << ": Overwriting dllreset to active in order to setup locking of "
					        "repeaters during initialization.");
			}
			if (hicann->repeater[addr].drvresetb != true) {
				hicann->repeater[addr].drvresetb = true;
				LOG4CXX_WARN(getLogger(),
					addr << ": Overwriting drvreset to inactive during initialization.");
			}
		}
	} else {
		// setup skipping of locking
		if (m_global_l1_bus_changes) {
			// ReadRepeaterTestdata uses custom SmartConfigurator for synapse driver locking, which
			// is not aware of correct L1 settings -> shows wrong warnings.
			LOG4CXX_WARN(
			    getLogger(), short_format(h->coordinate())
			                     << ": Setup skipping of repeater locking even though changes in "
			                        "L1 were detected. Warning can be ignored if it originates "
			                        "from ReadRepeaterTestdata synapse driver locking (Check logs "
			                        "with marocco loglevel DEBUG)");
		} else {
			LOG4CXX_INFO(getLogger(), short_format(h->coordinate())
				<< ": Setup skipping of repeater locking.");
		}

		for (auto addr : iter_all<RepeaterBlockOnHICANN>()) {
			if (hicann->repeater[addr].dllresetb != true) {
				hicann->repeater[addr].dllresetb = true;
				LOG4CXX_WARN(getLogger(),
					addr << ": Overwriting dllreset to zero in order to skip locking of repeaters "
					        "during initialization.");
			}
			if (hicann->repeater[addr].drvresetb != true) {
				hicann->repeater[addr].drvresetb = true;
				LOG4CXX_WARN(getLogger(),
					addr << ": Overwriting drvreset to inactive during initialization.");
			}
		}
	}

	// check synapse controllers
	if (syn_drv_locking_wanted(h->coordinate(), hicann)) {
		// setup regular locking
		LOG4CXX_DEBUG(
		    getLogger(), short_format(h->coordinate())
		                     << ": Setup regular locking of synapse drivers");
		// check if DLL reset is enabled for all synapse drivers
		if (!hicann->synapse_controllers.is_dllreset_enabled()) {
			hicann->synapse_controllers.enable_dllreset();
			LOG4CXX_WARN(
			    getLogger(), ": Overwriting dllreset of synapse controllers "
			                 "to active in order to perform locking of "
			                 "synapse drivers during initialization.");
		}
	} else {
		// setup skipping of locking
		if (syn_drv_locking_needed(h->coordinate(), hicann)) {
			LOG4CXX_WARN(
			    getLogger(), short_format(h->coordinate())
			                     << ": Setup skipping of synapse driver locking even though "
			                        "changes in L1 were detected.");
		} else {
			LOG4CXX_INFO(
			    getLogger(), short_format(h->coordinate())
			                     << ": Setup skipping of synapse driver locking.");
		}

		// check if DLL reset is disabled for all synapse drivers
		if (!hicann->synapse_controllers.is_dllreset_disabled()) {
			hicann->synapse_controllers.disable_dllreset();
			LOG4CXX_WARN(
			    getLogger(), short_format(h->coordinate())
			                     << ": Overwriting dllreset of synapse controller to "
			                        "zero in order to skip locking of synapse drivers "
			                        "during initialization.");
		}
	}
}

void ParallelHICANNv4SmartConfigurator::prime_systime_counter(fpga_handle_t const& f)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     !m_started_systime)) {
		return ParallelHICANNv4Configurator::prime_systime_counter(f);
	}
	LOG4CXX_INFO(
	    getLogger(),
	    "Skipping priming of systime counter of FPGA in since Wafer has already been initialized");
}

void ParallelHICANNv4SmartConfigurator::start_systime_counter(fpga_handle_t const& f)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     !m_started_systime)) {
		return ParallelHICANNv4Configurator::start_systime_counter(f);
	}
	LOG4CXX_INFO(
	    getLogger(),
	    "Skipping starting of systime counter of FPGA in since Wafer has already been initialized");
}

void ParallelHICANNv4SmartConfigurator::disable_global(fpga_handle_t const& f)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     !m_started_systime)) {
		return ParallelHICANNv4Configurator::disable_global(f);
	}
	LOG4CXX_INFO(
	    getLogger(),
	    "Skipping disabling of global synchronization of FPGA since Wafer has already"
	    "been initialized");
}

void ParallelHICANNv4SmartConfigurator::set_hicanns(
    hicann_datas_t hicanns, hicann_handles_t handles)
{
	LOG4CXX_DEBUG(getLogger(), "Setting SmartConfigurators HICANN data...");
	for (size_t ii = 0; ii != hicanns.size(); ++ii) {
		const hicann_coord coord = handles[ii]->coordinate();
		mWrittenHICANNData[coord] = hicann_data_t(new HICANNData);
		mWrittenHICANNData[coord]->copy(*(hicanns[ii].get()));
	}
	LOG4CXX_DEBUG(getLogger(), "Setting HICANN data complete!");
}

void ParallelHICANNv4SmartConfigurator::set_smart()
{
	fg_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	synapse_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	synapse_drv_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	reset_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	repeater_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	repeater_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
	syn_drv_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Smart;
}
void ParallelHICANNv4SmartConfigurator::set_skip()
{
	fg_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	synapse_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	synapse_drv_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	reset_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	repeater_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	repeater_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	syn_drv_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
}
void ParallelHICANNv4SmartConfigurator::set_force()
{
	fg_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	synapse_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	synapse_drv_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	reset_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	repeater_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	repeater_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	syn_drv_locking_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
}

bool ParallelHICANNv4SmartConfigurator::check_l1_bus_changes(hicann_coord coord,
                                                             hicann_data_t const& hicann) const
{
	auto search_old_hicann = mWrittenHICANNData.find(coord);

	// return if old data is not available
	if (search_old_hicann == mWrittenHICANNData.end()) {
		return true;
	}

	bool change_in_l1 = false;
	const hicann_data_t old_hicann = search_old_hicann->second;

	if (old_hicann) {
		// check for relevant changes in repeaters
		change_in_l1 |= (old_hicann->repeater.mHorizontalRepeater
		                 != hicann->repeater.mHorizontalRepeater);
		change_in_l1 |= (old_hicann->repeater.mVerticalRepeater
		                 != hicann->repeater.mVerticalRepeater);

		// check for changes in L1
		change_in_l1 |= (old_hicann->layer1 != hicann->layer1);

		// check for changes in switches
		change_in_l1 |= (old_hicann->synapse_switches != hicann->synapse_switches);
		change_in_l1 |= (old_hicann->crossbar_switches != hicann->crossbar_switches);

		// check if dllreset or drvreset were not disabled in old configuration
		change_in_l1 |= !old_hicann->repeater.is_drvreset_disabled();
		change_in_l1 |= !old_hicann->repeater.is_dllreset_disabled();
	}

	return change_in_l1;
}

bool ParallelHICANNv4SmartConfigurator::syn_drv_locking_needed(hicann_coord coord,
                                                               hicann_data_t const& hicann) const
{
	auto search_old_hicann = mWrittenHICANNData.find(coord);

	// return if old data is not available
	if (search_old_hicann == mWrittenHICANNData.end()) {
		return true;
	}

	bool driver_changes = m_global_l1_bus_changes;
	const hicann_data_t old_hicann = search_old_hicann->second;

	if (old_hicann) {
		for (auto const ii : iter_all<::halco::hicann::v2::SynapseDriverOnHICANN>()) {
			driver_changes |= (old_hicann->synapses[ii] != hicann->synapses[ii]);
		}
		// Check if dll-reset was not disabled in old configuration
		driver_changes |= !old_hicann->synapse_controllers.is_dllreset_disabled();
	}

	return driver_changes;
}

bool ParallelHICANNv4SmartConfigurator::repeater_locking_wanted() const
{
	return  ((repeater_locking_config_mode == ConfigMode::Force)
	          || (m_global_l1_bus_changes
	              && (repeater_locking_config_mode != ConfigMode::Skip)));
}

bool ParallelHICANNv4SmartConfigurator::syn_drv_locking_wanted(
	hicann_coord const& coord,
	hicann_data_t const& hicann) const
{

	return  ((syn_drv_locking_config_mode == ConfigMode::Force)
	          || (syn_drv_locking_needed(coord, hicann)
	              && (syn_drv_locking_config_mode != ConfigMode::Skip)));
}

bool ParallelHICANNv4SmartConfigurator::syn_drv_locking_wanted_any(
	hicann_handles_t const& handles,
	hicann_datas_t const& hicanns) const
{
	bool locking_needed = false;
	auto it_data = hicanns.begin();
	for (auto const& h : handles) {
		auto hicann = *it_data;
		locking_needed |= syn_drv_locking_wanted(h->coordinate(), hicann);
		++it_data;
	}

	return locking_needed;
}

void ParallelHICANNv4SmartConfigurator::note_fpga_config(
    fpga_coord const& fpga_c)
{
	mDidFPGAConfig.insert(fpga_c);
	LOG4CXX_DEBUG(
	    getLogger(),
	    "Noted configuration of " << fpga_c << "in SmartConfigurator.");
}

void ParallelHICANNv4SmartConfigurator::note_systime_start()
{
	m_started_systime = true;
	LOG4CXX_DEBUG(
	    getLogger(),
	    "Noted systime start of all allocated FPGAs in SmartConfigurator.");
}
} // end namespace sthal
