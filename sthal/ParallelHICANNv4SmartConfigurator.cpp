#include "sthal/ParallelHICANNv4SmartConfigurator.h"

#include <mutex>
#include <boost/serialization/nvp.hpp>

#include "sthal/HICANN.h"
#include "sthal/Settings.h"
#include "sthal/Timer.h"
#include "sthal/Wafer.h"

#include "hal/Coordinate/iter_all.h"
#include "hal/backend/HICANNBackend.h"

using namespace ::HMF::Coordinate;

namespace sthal {

ParallelHICANNv4SmartConfigurator::ParallelHICANNv4SmartConfigurator() :
    fg_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    synapse_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    synapse_drv_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    reset_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart),
    repeater_config_mode(ParallelHICANNv4SmartConfigurator::ConfigMode::Smart)
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
	ParallelHICANNv4Configurator::config(fpga_handle, hicann_handle, hicann_data, stage);
	auto const& last_stage = Settings::get().configuration_stages.order.back();
	if (stage == last_stage) {
		LOG4CXX_DEBUG(getLogger(), "Saving old configuration");
		omp_set_lock(&mLock);
		set_hicanns(hicann_data, hicann_handle);
		mDidFPGAReset.insert(fpga_handle->coordinate());
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
		const hicann_data_t old_hicann = mWrittenHICANNData[coord];
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
		const hicann_data_t old_hicann = mWrittenHICANNData[coord];
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
			decoder_data.reserve(n_all_changed_hicanns);
			hicann_handles_t drv_changed_handles;
			for (size_t ii = 0; ii != n_all_changed_hicanns; ++ii) {
				const hicann_coord coord = all_changed_handles[ii]->coordinate();
				const hicann_data_t old_hicann = mWrittenHICANNData[coord];
				if (old_hicann == nullptr ||
				    old_hicann->synapses.getDecoderDoubleRow(syndrv) !=
				        all_changed_hicanns[ii]->synapses.getDecoderDoubleRow(syndrv)) {
					decoder_data.push_back(
					    all_changed_hicanns[ii]->synapses.getDecoderDoubleRow(syndrv));
					drv_changed_handles.push_back(handles[ii]);
				}
			}

			set_decoder_double_row(drv_changed_handles, syndrv, decoder_data);
			LOG4CXX_DEBUG(
			    getLogger(), "Smartly set decoder double row, skipped " +
			                     std::to_string(handles.size() - drv_changed_handles.size()) +
			                     " configurations");

			for (auto side : iter_all<SideVertical>()) {
				SynapseRowOnHICANN const synrow(syndrv, RowOnSynapseDriver(side));

				std::vector<HMF::HICANN::WeightRow> weight_data;
				hicann_handles_t row_changed_handles;
				weight_data.reserve(n_all_changed_hicanns);
				row_changed_handles.reserve(n_all_changed_hicanns);
				for (size_t ii = 0; ii != n_all_changed_hicanns; ++ii) {
					const hicann_coord coord = all_changed_handles[ii]->coordinate();
					const hicann_data_t old_hicann = mWrittenHICANNData[coord];
					if (!(synapse_config_mode == ConfigMode::Skip) &&
					    (old_hicann == nullptr ||
					     old_hicann->synapses[synrow].weights !=
					         all_changed_hicanns[ii]->synapses[synrow].weights)) {
						weight_data.push_back(all_changed_hicanns[ii]->synapses[synrow].weights);
						row_changed_handles.push_back(all_changed_handles[ii]);
					}
				}

				set_weights_row(row_changed_handles, synrow, weight_data);
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
	const hicann_data_t old_hicann = mWrittenHICANNData[coord];

	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure synapse drivers");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		if (!(synapse_drv_config_mode == ConfigMode::Skip) &&
		    (old_hicann == nullptr || old_hicann->synapses[syndrv] != hicann->synapses[syndrv])) {
			::HMF::HICANN::set_synapse_driver(*h, syndrv, hicann->synapses[syndrv]);
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
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	const hicann_coord coord = h->coordinate();
	const hicann_data_t old_hicann = mWrittenHICANNData[coord];
	if (!(repeater_config_mode == ConfigMode::Skip) &&
	    (old_hicann == nullptr || old_hicann->repeater != hicann->repeater)) {
		return ParallelHICANNv4Configurator::config_repeater(h, hicann);
	} else {
		LOG4CXX_INFO(getLogger(), "Skipping Repeater configuration as nothing has changed");
	}
}

void ParallelHICANNv4SmartConfigurator::lock_repeater(
    hicann_handle_t const& h, hicann_data_t const& hicann)
{
	if (repeater_config_mode == ConfigMode::Force) {
		LOG4CXX_INFO(getLogger(), "Forcing Repeater Lock write, ignoring previous configuration");
		return ParallelHICANNv4Configurator::lock_repeater(h, hicann);
	}
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	const hicann_coord coord = h->coordinate();
	const hicann_data_t old_hicann = mWrittenHICANNData[coord];
	if (!(repeater_config_mode == ConfigMode::Skip) &&
	    (old_hicann == nullptr || old_hicann->repeater != hicann->repeater)) {
		return ParallelHICANNv4Configurator::lock_repeater(h, hicann);
	} else {
		LOG4CXX_INFO(getLogger(), "Skipping Repeater Lock configuration as nothing has changed");
	}
}

void ParallelHICANNv4SmartConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     mDidFPGAReset.find(f->coordinate()) == mDidFPGAReset.end())) {
		LOG4CXX_DEBUG(getLogger(), "Doing regular FPGA config");
		return ParallelHICANNv4Configurator::config_fpga(f, fpga);
	}
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(
	    getLogger(), "Skipping reset and init of FPGA since Wafer has already been initialized");
	config_dnc_link(f, fpga);
	LOG4CXX_INFO(
	    getTimeLogger(), "finished configuring DNC link of FPGA " << f->coordinate() << " in "
	                                                              << t.get_ms() << "ms");
}

void ParallelHICANNv4SmartConfigurator::prime_systime_counter(fpga_handle_t const& f)
{
	if (!(reset_config_mode == ConfigMode::Skip) &&
	    (reset_config_mode == ConfigMode::Force ||
	     mDidFPGAReset.find(f->coordinate()) == mDidFPGAReset.end())) {
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
	     mDidFPGAReset.find(f->coordinate()) == mDidFPGAReset.end())) {
		return ParallelHICANNv4Configurator::start_systime_counter(f);
	}
	LOG4CXX_INFO(
	    getLogger(),
	    "Skipping starting of systime counter of FPGA in since Wafer has already been initialized");
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
}
void ParallelHICANNv4SmartConfigurator::set_skip()
{
	fg_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	synapse_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	synapse_drv_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	reset_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
	repeater_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Skip;
}
void ParallelHICANNv4SmartConfigurator::set_force()
{
	fg_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	synapse_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	synapse_drv_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	reset_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
	repeater_config_mode = ParallelHICANNv4SmartConfigurator::ConfigMode::Force;
}

} // end namespace sthal
