#pragma once

#include "pywrap/compat/macros.hpp"

#include "hal/HICANNContainer.h"

#include "sthal/ConfigurationStages.h"
#include "sthal/HICANNConfigurator.h"

PYPP_INSTANTIATE(std::vector< ::HMF::HICANN::FGRowOnFGBlock4 >)
PYPP_INSTANTIATE(std::vector< std::vector< ::HMF::HICANN::FGRowOnFGBlock4 > >)

namespace sthal {

/// Parallel HICANN Configurator for HICANNv4 chips
///
/// This configurator implements an improved floating gate programming required
/// for HICANN v4. To program the reversal potentials of the chip correctly, it
/// seems necessary to turn off the synaptic inputs completely. This is done by
/// implementing the following floating gates writing scheme.
///  - Write all cells to zero
///  - Write all voltage cells normal
///  - Write current cells that have their minimum value below mFastUpwardsLimit
///        (default 800) normal
///  - Write the other current cells fast upwards (All current cells within a
///        row having at least one value over mFastUpwardsLimit)
///
class ParallelHICANNv4Configurator : public HICANNConfigurator
{
public:
	typedef std::vector<hicann_data_t> hicann_datas_t;
	typedef std::vector< ::HMF::HICANN::FGRowOnFGBlock4> row_list_t;
	typedef std::vector<row_list_t> row_lists_t;
	PYPP_INSTANTIATE(std::vector<hicann_handle_t>)
	PYPP_INSTANTIATE(std::vector<hicann_data_t>)

	/// If the smallest value of an FGRow is larger than limit, programm_high
	/// is used to programm this line
	void setFastUpwardsLimit(size_t limit);

	virtual void config(fpga_handle_t const& f, hicann_handles_t const& handles,
	                    hicann_datas_t const& hicanns, ConfigurationStage stage);

	void config(fpga_handle_t const& fpga_handle, hicann_handle_t const& hicann_handle,
	            hicann_data_t const& hicann_data) PYPP_OVERRIDE;

	virtual void config_floating_gates(
		hicann_handles_t const& handles, hicann_datas_t const& hicanns);
	virtual void config_synapse_array(
		hicann_handles_t const& handles, hicann_datas_t const& hicanns);

	void ensure_correct_fg_biases(hicann_datas_t const& hicanns);

	/// Writes all floating gate fast to zero
	void zero_fg(hicann_handles_t const& handles, hicann_datas_t const& hicanns);

	/// Updates the rows to a high value with special fg settings, for fast
	/// updating at the cost of some precision. This should be only used for
	/// bias currents or floating gates that are near the maximum value.
	/// This scheme aims to reduce cross-talk to other cells
	//TODO names fast normal?
	void program_high(
		hicann_handles_t const& handles, hicann_datas_t const& hicanns, row_lists_t const& rows);

	/**
	 * @param zero_neuron_parameters Whether to write zeros instead of the actual
	 *        per-neuron parameters in all rows.
	 */
	void program_normal(
		hicann_handles_t const& handles,
		hicann_datas_t const& hicanns,
		row_lists_t const& rows,
		bool zero_neuron_parameters = false);

#ifndef PYPLUSPLUS
	::HMF::HICANN::FGRow::value_type mFastUpwardsLimit = 800;
#endif // !PYPLUSPLUS

	static const row_list_t CURRENT_ROWS;
	static const row_list_t VOLTAGE_ROWS;
};

} // end namespace sthal
