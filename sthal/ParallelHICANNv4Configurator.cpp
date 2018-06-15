#include "sthal/ParallelHICANNv4Configurator.h"
#include "sthal/Settings.h"

#include <cstdlib>
#include <sstream>
#include <boost/ref.hpp>
#include <log4cxx/logger.h>

#include "pythonic/enumerate.h"
#include "pythonic/zip.h"

#include "hal/Coordinate/iter_all.h"
#include "hal/Coordinate/FormatHelper.h"
#include "hal/HICANNContainer.h"
#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"
#include "hal/Handle/HICANNHw.h"
#include "hal/backend/HICANNBackend.h"

#include "sthal/HICANN.h"
#include "sthal/Timer.h"

using namespace ::HMF::Coordinate;

namespace sthal {

namespace {

typedef ParallelHICANNv4Configurator::row_list_t row_list_t;

row_list_t make_rows(size_t start, size_t offset)
{
	using namespace ::HMF::HICANN;
	row_list_t rows;
	for (size_t ii = start; ii < FGRowOnFGBlock::size; ii += offset) {
		rows.push_back(FGRowOnFGBlock4{FGRowOnFGBlock(ii), FGRowOnFGBlock(ii), FGRowOnFGBlock(ii),
		                               FGRowOnFGBlock(ii)});
	}
	return rows;
}

FGConfig createFastUpwardsConfig(const FloatingGates& fg)
{
	FGConfig fgconfig;
	size_t passes = fg.getNoProgrammingPasses();
	if (passes > 0) {
		fgconfig = fg.getFGConfig(Enum(passes - 1));
	}
	fgconfig.voltagewritetime = 63;
	fgconfig.currentwritetime = 63;
	fgconfig.maxcycle = 31;
	fgconfig.pulselength = 15;
	fgconfig.readtime = 30;
	fgconfig.acceleratorstep = 2;
	fgconfig.writeDown = fgconfig.WRITE_UP;
	return fgconfig;
}

FGConfig createZeroFgConfig(const FloatingGates& fg)
{
	FGConfig fgconfig;
	size_t passes = fg.getNoProgrammingPasses();
	if (passes > 0) {
		fgconfig = fg.getFGConfig(Enum(0));
	}
	fgconfig.voltagewritetime = 63;
	fgconfig.currentwritetime = 63;
	fgconfig.maxcycle = 23;
	fgconfig.pulselength = 15;
	fgconfig.readtime = 30;
	fgconfig.acceleratorstep = 2;
	fgconfig.writeDown = fgconfig.WRITE_DOWN;
	return fgconfig;
}

const std::array< ::HMF::HICANN::neuron_parameter, 12> CURRENT_PARAMETERS{{
	::HMF::HICANN::neuron_parameter::I_bexp,
	::HMF::HICANN::neuron_parameter::I_convi,
	::HMF::HICANN::neuron_parameter::I_convx,
	::HMF::HICANN::neuron_parameter::I_fire,
	::HMF::HICANN::neuron_parameter::I_gl,
	::HMF::HICANN::neuron_parameter::I_gladapt,
	::HMF::HICANN::neuron_parameter::I_intbbi,
	::HMF::HICANN::neuron_parameter::I_intbbx,
	::HMF::HICANN::neuron_parameter::I_pl,
	::HMF::HICANN::neuron_parameter::I_radapt,
	::HMF::HICANN::neuron_parameter::I_rexp,
	::HMF::HICANN::neuron_parameter::I_spikeamp,
}};

::HMF::HICANN::FGRow::value_type getFGRowMin(const ::HMF::HICANN::FGRow& row)
{
	::HMF::HICANN::FGRow::value_type minimum = row.getShared();
	for (auto nrn : iter_all<NeuronOnFGBlock>()) {
		minimum = std::min(row.getNeuron(nrn), minimum);
	}
	return minimum;
}

std::pair<row_list_t, row_list_t> get_current_rows(
	const HICANNData& hicann, ::HMF::HICANN::FGRow::value_type lower_limit)
{
	using namespace ::HMF::HICANN;
	std::vector<bool> parameter_is_high(CURRENT_PARAMETERS.size(), false);
	for (auto block : iter_all<FGBlockOnHICANN>()) {
		const FGBlock& blk = hicann.floating_gates[block];
		for (auto it : pythonic::enumerate(CURRENT_PARAMETERS)) {
			FGRow row = blk.getFGRow(getNeuronRow(block, it.second));
			parameter_is_high[it.first] =
				parameter_is_high[it.first] | (getFGRowMin(row) >= lower_limit);
		}
	}
	row_list_t low, high;
	for (auto it : pythonic::zip(parameter_is_high, CURRENT_PARAMETERS)) {
		row_list_t& l = (it.first ? high : low);
		l.push_back(FGRowOnFGBlock4{getNeuronRow(FGBlockOnHICANN(Enum(0)), it.second),
		                            getNeuronRow(FGBlockOnHICANN(Enum(1)), it.second),
		                            getNeuronRow(FGBlockOnHICANN(Enum(2)), it.second),
		                            getNeuronRow(FGBlockOnHICANN(Enum(3)), it.second)});
	}
	return std::make_pair(low, high);
}

} // namespace

const ParallelHICANNv4Configurator::row_list_t ParallelHICANNv4Configurator::CURRENT_ROWS =
	make_rows(1, 2);

const ParallelHICANNv4Configurator::row_list_t ParallelHICANNv4Configurator::VOLTAGE_ROWS =
	make_rows(0, 2);

void ParallelHICANNv4Configurator::setFastUpwardsLimit(size_t limit)
{
	if (limit > 1023)
		throw std::invalid_argument("limit out of bounds");
	mFastUpwardsLimit = limit;
}

void ParallelHICANNv4Configurator::config(fpga_handle_t const& f,
                                          hicann_handles_t const& handles,
                                          hicann_datas_t const& hicanns, ConfigurationStage stage) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");

	ensure_correct_fg_biases(hicanns);

	LOG4CXX_DEBUG(HICANNConfigurator::getLogger(),
	              short_format(f->coordinate()) << " ParallelHICANNv4Configurator stage "
	                                            << static_cast<size_t>(stage));

	switch (stage) {
		case ConfigurationStage::TIMING_UNCRITICAL: {
			// interleaved
			config_floating_gates(handles, hicanns);

			// interleaved
			config_synapse_array(handles, hicanns);

			// TODO: make parallel/interleaved (no bottleneck here?)
			auto it_data = hicanns.begin();
			for (auto handle : handles) {
				auto hicann = *it_data;
				config_phase(handle, hicann);
				config_gbitlink(handle, hicann);
				config_fg_stimulus(handle, *it_data);
				config_synapse_switch(handle, hicann);
				config_stdp(handle, hicann);
				config_crossbar_switches(handle, hicann);
				config_merger_tree(handle, hicann);
				config_dncmerger(handle, hicann);
				config_background_generators(handle, hicann);
				flush_hicann(handle);
				++it_data;
			}
			break;
		}

		case ConfigurationStage::CONFIG_REPEATER: {
			auto it_data = hicanns.begin();
			for (auto handle : handles) {
				auto hicann = *it_data;
				config_repeater(handle, hicann);
				flush_hicann(handle);
				++it_data;
			}
			break;
		}

		case ConfigurationStage::LOCK_REPEATER: {
			auto it_data = hicanns.begin();
			for (auto handle : handles) {
				auto hicann = *it_data;
				lock_repeater(handle, hicann);
				flush_hicann(handle);
				++it_data;
			}
			break;
		}
		case ConfigurationStage::LOCK_SYNAPSEDRIVERS: {
			auto it_data = hicanns.begin();
			for (auto handle : handles) {
				auto hicann = *it_data;
				config_synapse_drivers(handle, hicann);
				flush_hicann(handle);
				++it_data;
			}
			break;
		}
		case ConfigurationStage::NEURONS: {
			auto it_data = hicanns.begin();
			for (auto handle : handles) {
				auto hicann = *it_data;
				config_neuron_config(handle, hicann);
				config_neuron_quads(handle, hicann);
				config_analog_readout(handle, hicann);
				flush_hicann(handle);
				++it_data;
			}
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
	                              << static_cast<size_t>(stage) << " in " << t.get_ms()
	                              << "ms");
}

void ParallelHICANNv4Configurator::config(fpga_handle_t const& fpga_handle,
                                          hicann_handle_t const& hicann_handle,
                                          hicann_data_t const& hicann_data) {
	auto const& settings = Settings::get();

	for(auto stage : settings.configuration_stages.order) {
		config(fpga_handle, hicann_handles_t{hicann_handle}, hicann_datas_t{hicann_data}, stage);
	}
}

void ParallelHICANNv4Configurator::ensure_correct_fg_biases(hicann_datas_t const& hicanns)
{
	if (std::getenv("STHAL_ALLOW_FG_BIAS_TINKERING") != nullptr) {
		LOG4CXX_WARN(
			getLogger(), "Known-good FG bias values will not be enforced "
			"due to STHAL_ALLOW_FG_BIAS_TINKERING environment variable");
		return;
	}

	for (auto& hicann : hicanns) {
		const size_t passes = hicann->floating_gates.getNoProgrammingPasses();
		for (size_t pass = 0; pass < passes; ++pass) {
			FGConfig cfg = hicann->floating_gates.getFGConfig(Enum(pass));
			if (cfg.fg_biasn != 0 || cfg.fg_bias != 0) {
				cfg.fg_biasn = 0;
				cfg.fg_bias = 0;
				hicann->floating_gates.setFGConfig(Enum(pass), cfg);
			}
		}
	}
}

void ParallelHICANNv4Configurator::config_floating_gates(
	hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	using namespace ::HMF::HICANN;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles, data containers has to be equal");
	const size_t n_hicanns = hicanns.size();

	row_lists_t current_lows, current_highs;
	for (auto hicann : hicanns) {
		row_list_t current_low, current_high;
		std::tie(current_low, current_high) = get_current_rows(*hicann, mFastUpwardsLimit);
		current_lows.push_back(current_low);
		current_highs.push_back(current_high);
	}

	// interleaved
	zero_fg(handles, hicanns);

	row_list_t row_int_op_bias;
	const size_t intop = ::HMF::HICANN::shared_parameter::int_op_bias;
	row_int_op_bias.push_back(FGRowOnFGBlock4{FGRowOnFGBlock(intop), FGRowOnFGBlock(intop),
	                                          FGRowOnFGBlock(intop), FGRowOnFGBlock(intop)});
	row_lists_t row_int_op_bias_per_hicann(n_hicanns, row_int_op_bias);

	program_normal(handles, hicanns, row_int_op_bias_per_hicann, /*zero_neuron_parameters=*/true);

	row_lists_t VOLTAGE_ROWS_PER_HICANN(n_hicanns, VOLTAGE_ROWS);
	program_normal(handles, hicanns, VOLTAGE_ROWS_PER_HICANN);

	program_normal(handles, hicanns, current_lows);
	program_high(handles, hicanns, current_highs);

	LOG4CXX_DEBUG(getTimeLogger(), "writing FG blocks took " << t.get_ms() << "ms");
}


void ParallelHICANNv4Configurator::config_synapse_array(
	hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");

	const size_t n_hicanns = hicanns.size();

	LOG4CXX_DEBUG(getLogger(), "configure synapses in parallel for " << n_hicanns
	                                                                 << " HICANN(s)");

	// TODO: interleave calls to top / bottom synapse controllers
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		std::vector<HMF::HICANN::DecoderDoubleRow> decoder_data;
		decoder_data.reserve(n_hicanns);
		for (auto data : hicanns) {
			decoder_data.push_back(data->synapses.getDecoderDoubleRow(syndrv));
		}

		set_decoder_double_row(handles, syndrv, decoder_data);
		for (auto side : iter_all<SideVertical>()) {
			SynapseRowOnHICANN const synrow(syndrv, RowOnSynapseDriver(side));

			std::vector<HMF::HICANN::WeightRow> weight_data;
			weight_data.reserve(n_hicanns);
			for (auto data : hicanns) {
				weight_data.push_back(data->synapses[synrow].weights);
			}

			set_weights_row(handles, synrow, weight_data);
		}
	}

	LOG4CXX_DEBUG(getTimeLogger(), "configure synapses in parallel for "
	                                   << n_hicanns << " HICANN(s) took " << t.get_ms()
	                                   << "ms");
}

void ParallelHICANNv4Configurator::zero_fg(
	hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "zero floating gates");

	if (handles.size() != hicanns.size())
		throw std::invalid_argument("the number of handles and data containers has to be equal");

	const size_t n_hicanns = hicanns.size();

	std::vector<FGConfig> fgconfigs;
	fgconfigs.reserve(n_hicanns);
	for (size_t i = 0; i != n_hicanns; ++i)
		fgconfigs.push_back(createZeroFgConfig(hicanns[i]->floating_gates));

	for (auto block : iter_all<FGBlockOnHICANN>())
		for (size_t i = 0; i != n_hicanns; ++i)
			::HMF::HICANN::set_fg_config(*handles[i], block, fgconfigs[i]);

	auto write_row_on_all_blocks = [&handles, &n_hicanns,
	                                &fgconfigs](::HMF::HICANN::FGRowOnFGBlock4 const& row) {
		::HMF::HICANN::FGRow4 const row_data; // zeroed data
		// write FGRow4 to all blocks
		for (size_t i = 0; i != n_hicanns; ++i)
			::HMF::HICANN::set_fg_row_values(
				*handles[i], row, row_data, fgconfigs[i].writeDown, /* blocking */ false);
		// wait for fg controller to finish
		for (size_t i = 0; i != n_hicanns; ++i)
			::HMF::HICANN::wait_fg(*handles[i]);
	};

	for (auto row : CURRENT_ROWS)
		write_row_on_all_blocks(row);

	for (auto row : VOLTAGE_ROWS)
		write_row_on_all_blocks(row);

	LOG4CXX_DEBUG(getTimeLogger(), "zero floating gates took " << t.get_ms() << "ms");
}

void ParallelHICANNv4Configurator::program_normal(
	hicann_handles_t const& handles,
	hicann_datas_t const& hicanns,
	row_lists_t const& rows,
	bool zero_neuron_parameters)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "update normal rows");

	if ((handles.size() != hicanns.size()) || (hicanns.size() != rows.size()))
		throw std::invalid_argument(
			"the number of handles, data containers and rows parameter has to be equal");
	const size_t n_hicanns = hicanns.size();

	LOG4CXX_DEBUG(getLogger(), "Have " << n_hicanns << " HICANNs");

	// maximum number of passes for each hicann
	std::vector<size_t> maxProgrammingPasses(hicanns.size());
	std::transform(
		hicanns.begin(), hicanns.end(), maxProgrammingPasses.begin(),
		[](boost::shared_ptr<HICANNData const> h) {
			return h->floating_gates.getNoProgrammingPasses();
		});

	// maximum FG programming time over all HICANNs
	size_t const totalMaxProgrammingPasses =
		*(std::max_element(maxProgrammingPasses.begin(), maxProgrammingPasses.end()));

	std::vector<size_t> maxRows(rows.size());
	std::transform(
		rows.begin(), rows.end(), maxRows.begin(), [](row_list_t r) { return r.size(); });
	size_t const maxRowsOnAnyHICANN = *(std::max_element(maxRows.begin(), maxRows.end()));

	for (size_t pass = 0; pass != totalMaxProgrammingPasses; ++pass) {
		LOG4CXX_DEBUG(
			getLogger(),
			"FG: writing FG blocks (pass " << pass + 1
			<< " out of " << totalMaxProgrammingPasses << "): ");

		// configure all hicanns
		for (size_t i = 0; i != n_hicanns; ++i) {
			if (pass < maxProgrammingPasses[i]) {
				LOG4CXX_DEBUG(getLogger(), "FG: configuring HICANN " << i);
				const FloatingGates& fg = hicanns[i]->floating_gates;
				FGConfig cfg = fg.getFGConfig(Enum(pass));
				for (auto block : iter_all<FGBlockOnHICANN>()) {
					::HMF::HICANN::set_fg_config(*handles[i], block, cfg);
				}
			}
		}

		// all rows on all hicanns
		for (size_t r = 0; r < maxRowsOnAnyHICANN; r++) {
			for (size_t i = 0; i != n_hicanns; ++i) {
				LOG4CXX_DEBUG(
					getLogger(),
					"Have " << r << "/" << rows[i].size() << " on HICANN " << i
					<< " (" << n_hicanns << " HICANNs in total)");
				if ((r < rows[i].size()) && (pass < maxProgrammingPasses[i])) {
					auto row = rows[i][r];
					// row in list...
					LOG4CXX_DEBUG(getLogger(), "FG: writing HICANN " << i);
					const FloatingGates& fg = hicanns[i]->floating_gates;
					FGConfig cfg = fg.getFGConfig(Enum(pass));
					::HMF::HICANN::FGRow4 row_data{
						{hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(0))].getFGRow(row[0]),
						 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(1))].getFGRow(row[1]),
						 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(2))].getFGRow(row[2]),
						 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(3))].getFGRow(row[3])}};

					if (zero_neuron_parameters) {
						for (::HMF::HICANN::FGRow& fgrow : row_data) {
							for (auto nrn_c : iter_all<NeuronOnFGBlock>()) {
								fgrow.setNeuron(nrn_c, 0);
							}
						}
					}

					LOG4CXX_TRACE(getLogger(), "updating rows " << row);
					::HMF::HICANN::set_fg_row_values(
						*handles[i], row, row_data, cfg.writeDown, /* blocking */ false);
				}
			}

			// wait for for on all hicanns
			for (size_t i = 0; i != n_hicanns; ++i) {
				if (r < rows[i].size()) {
					if (pass < maxProgrammingPasses[i]) {
						LOG4CXX_DEBUG(getLogger(), "FG: waiting for HICANN " << i);
						::HMF::HICANN::wait_fg(*handles[i]);
					}
				}
			}
		} // loop over Rows
	}     // loop over passes

	LOG4CXX_DEBUG(getTimeLogger(), "update normal rows took " << t.get_ms() << "ms");
}

void ParallelHICANNv4Configurator::program_high(
	hicann_handles_t const& handles, hicann_datas_t const& hicanns, row_lists_t const& rows)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "program up fast");
	if ((handles.size() != hicanns.size()) || (hicanns.size() != rows.size()))
		throw std::runtime_error(
			"the number of handles, data containers and rows parameter has to be equal");
	const size_t n_hicanns = hicanns.size();

	std::vector<FGConfig> fgconfigs;
	for (size_t i = 0; i != n_hicanns; ++i)
		fgconfigs.push_back(createFastUpwardsConfig(hicanns[i]->floating_gates));

	for (auto block : iter_all<FGBlockOnHICANN>()) {
		for (size_t i = 0; i != n_hicanns; ++i) {
			::HMF::HICANN::set_fg_config(*handles[i], block, fgconfigs[i]);
		}
	}

	std::vector<size_t> maxRows(rows.size());
	std::transform(
		rows.begin(), rows.end(), maxRows.begin(), [](row_list_t r) { return r.size(); });
	size_t const maxRowsOnAnyHICANN = *(std::max_element(maxRows.begin(), maxRows.end()));

	for (size_t r = 0; r < maxRowsOnAnyHICANN; r++) {
		for (size_t i = 0; i != n_hicanns; ++i) {
			LOG4CXX_DEBUG(
				getLogger(), "Have " << r << "/" << rows[i].size() << " on HICANN " << i
				<< " (" << n_hicanns << " HICANNs in total)");
			if (r < rows[i].size()) {
				auto row = rows[i][r];
				::HMF::HICANN::FGRow4 row_data{
					{hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(0))].getFGRow(row[0]),
					 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(1))].getFGRow(row[1]),
					 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(2))].getFGRow(row[2]),
					 hicanns[i]->floating_gates[FGBlockOnHICANN(Enum(3))].getFGRow(row[3])}};
				LOG4CXX_TRACE(getLogger(), "updating rows " << row);
				::HMF::HICANN::set_fg_row_values(
					*handles[i], row, row_data, fgconfigs[i].writeDown, /* blocking */ false);
			}
		}

		// wait for for on all hicanns
		for (size_t i = 0; i != n_hicanns; ++i) {
			if (r < rows[i].size()) {
				LOG4CXX_DEBUG(getLogger(), "FG: waiting for HICANN " << i);
				::HMF::HICANN::wait_fg(*handles[i]);
			}
		}
	}

	LOG4CXX_DEBUG(getTimeLogger(), "program up fast took " << t.get_ms() << "ms");
}

} // end namespace sthal
