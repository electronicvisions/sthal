#pragma once

#include <boost/shared_ptr.hpp>
#include <vector>

#include "pywrap/compat/array.hpp"
#include "pywrap/compat/macros.hpp"
// GCCXML doesn't like the new log4cxx headers, avoid including them
#ifdef PYPLUSPLUS
namespace log4cxx {
class Logger;
typedef boost::shared_ptr<log4cxx::Logger> LoggerPtr;
}
#else
#include "log4cxx/provisionnode.h"
#endif

#include "halco/hicann/v2/fg.h"


namespace HMF {
namespace Handle {
	class FPGA;
	class HICANN;
} // Handle
} // HMF

namespace sthal {

class HICANNData;
class FPGA;
class FloatingGates;

// FIXME add PLL again
// TODO add sync steps or barriers
class HICANNConfigurator
{
public:
	typedef boost::shared_ptr< ::HMF::Handle::FPGA> fpga_handle_t;
	typedef boost::shared_ptr< ::HMF::Handle::HICANN> hicann_handle_t;
	typedef boost::shared_ptr<sthal::FPGA> fpga_t;
	typedef boost::shared_ptr<sthal::HICANNData> hicann_data_t;

	typedef std::vector<hicann_handle_t> hicann_handles_t;

	HICANNConfigurator();
#ifndef PYPLUSPLUS
template<typename T, typename... Ps>
static boost::shared_ptr<HICANNConfigurator> create(Ps&... ps) {
	return boost::shared_ptr<HICANNConfigurator>(new T(std::forward<Ps>(ps)...));
}
#endif

	// ECM (2016-08-31): Some child classes DO throw, but (i.e. FIXME)
	// boost::python::instance_holder::~instance_holder() is noexcept(true),
	// so all directly wrapped classes need noexcept(true) (which is the default
	// for user-defined dtors). I added the keyword explicitly to
	// MAKE IT VISIBLE :).
	virtual ~HICANNConfigurator() PYPP_NOEXCEPT(true);

	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
	virtual void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg);

	/**
	 * Sets several L1 related settings to values which are desired during initial configuration.
	 * Tests whether dll-reset is pulled and drv-reset is disabled for repeater blocks in the hicann
	 * object. Furthermore, it checks if dll-reset is enabled in all synapse controllers. All wrong
	 * reset values are actively set and a warning is produced.
	 * The synapse controllers in the hicann object are supposed to have the IDLE command. If not a
	 * runtime error is thrown.
	 *
	 * @param hicann Configuration data to be checked and modified if necessary.
	 */
	virtual void ensure_correct_l1_init_settings(
		hicann_handle_t const&, hicann_data_t const& hicann);

	/**
	 * Writes initial values as specified in hicann to the repeater blocks, synapse controllers
	 * and neurons of the HICANN represented by h.
	 *
	 * @param h HICANN for which the controllers have to be initialized.
	 * @param hicann Data which is written to the controllers.
	 */
	virtual void init_controllers(hicann_handle_t const& h, hicann_data_t const& hicann);

	/* Stop and reset FPGA and all HICANN time counter and set if they will
	 * listen for local or global start signal. Latter is determined by globalListen
	 * flag in the FPGA handle */
	virtual void prime_systime_counter(fpga_handle_t const& f);
	/* Start systime counter of FPGA and HICANNs. If FPGA is in local mode,
	 * each FPGA gets own singal from host. If global mode ,i.e., multpile FPGAs
	 * only master FPGA gets signal from host. The master FPGA then simultanioulsy
	 * thriggers all FPGAs. */
	virtual void start_systime_counter(fpga_handle_t const& f);
	virtual void disable_global(fpga_handle_t const& f);

	virtual void flush_hicann(hicann_handle_t const& h);

	virtual void config_spinnaker_interface(fpga_handle_t const& f, fpga_t const& fpga);
	virtual void config_dnc_link(fpga_handle_t const& f, fpga_t const& fg);

	virtual void config_fg_stimulus(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_floating_gates(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_analog_readout(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_merger_tree(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_dncmerger(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_gbitlink(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_phase(hicann_handle_t const& h, hicann_data_t const& hicann);

	virtual void config_repeater(hicann_handle_t const& h, hicann_data_t const& hicann);

	/**
	 * Writes repeater blocks configuration, which is stored in hicann, to the HICANN
	 * specified by h. This includes the SRAM timings, test output data as well
	 * as the configuration.
	 *
	 * @param h HICANN for which the repeater blocks should be configured.
	 * @param hicann Holds the configuration data.
	 */
	virtual void config_repeater_blocks(hicann_handle_t const& h, hicann_data_t const& hicann);

	/**
	 * Writes the configuration of synapse controllers, which is stored in hicann, to the HICANN
	 * specified by h. This includes the following registers for each array:
	 * control, configuration, reset and STDP LUTs.
	 * Furthermore, the SRAM timings of the synapse controllers are set.
	 *
	 * @param h HICANN for which the synapse controllers should be configured.
	 * @param hicann Holds the configuration data.
	 */
	virtual void config_synapse_controllers(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_synapse_switch(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_crossbar_switches(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_synapse_drivers(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_synapse_array(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_stdp(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_neuron_config(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_background_generators(
		hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_neuron_quads(
		hicann_handle_t const& h, hicann_data_t const& hicann, bool disable_spl1_output = false);

	/**
	 * Waits until 1. Host-FPGA and 2. all FPGA-HICANN communication channels are idle, i.e.
	 * similar to a "sync barrier".
	 *
	 * @param fpga_handle FPGA handle.
	 * @param hicann_handles Handles of HICANNs to be included in the synchronization.
	 */
	void sync_command_buffers(fpga_handle_t const& fpga_handle,
	                          hicann_handles_t const& hicann_handles);

	virtual void write_fg_row(
		hicann_handle_t const& h,
		const ::halco::hicann::v2::FGRowOnFGBlock& row,
		const FloatingGates& fg,
		bool writeDown);

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();
private:
	// dummy serialization as the HICANNConfigurator class doesn't actually have any
        // persistent data that needs to be serialized
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& /* ar */, unsigned int const /* version */) {}
};

} // end namespace sthal
