#pragma once

#include <boost/ref.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/shared_ptr.hpp>

#include "hal/Handle/HICANN.h"
#include "sthal/HICANNData.h"
#include "sthal/ParallelHICANNv4Configurator.h"

extern "C"
{
#include <omp.h>
}

namespace sthal {

class Wafer;

// Parallel HICANN v4 Configurator that only configures hardware where
// the configuration has changed since the last config call
// The previous configuration state is saved
// Smart configuration is done for floating gates, synapse drivers, synapse array,
// repeaters and fpgas
class ParallelHICANNv4SmartConfigurator : public ParallelHICANNv4Configurator
{
public:
	typedef ::halco::hicann::v2::HICANNOnWafer hicann_coord;
	typedef ::halco::hicann::v2::FPGAOnWafer fpga_coord;
	PYPP_CLASS_ENUM(ConfigMode){Skip, Smart, Force};

	ParallelHICANNv4SmartConfigurator();
	~ParallelHICANNv4SmartConfigurator();

	static boost::shared_ptr<ParallelHICANNv4SmartConfigurator> create();

	virtual void config(
	    fpga_handle_t const& f,
	    hicann_handles_t const& handles,
	    hicann_datas_t const& hicanns,
	    ConfigurationStage stage) PYPP_OVERRIDE;

	virtual void config_floating_gates(
	    hicann_handles_t const& handles, hicann_datas_t const& hicanns) PYPP_OVERRIDE;
	virtual void config_synapse_array(
	    hicann_handles_t const& handles, hicann_datas_t const& hicanns) PYPP_OVERRIDE;
	virtual void config_synapse_drivers(hicann_handle_t const& h, hicann_data_t const& hicann)
	    PYPP_OVERRIDE;
	virtual void config_repeater(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
	virtual void ensure_correct_l1_init_settings(
	    hicann_handle_t const& h, hicann_data_t const& hicann) PYPP_OVERRIDE;
	virtual void prime_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	virtual void start_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	virtual void disable_global(fpga_handle_t const& f) PYPP_OVERRIDE;

	// smart functions
	void set_hicanns(hicann_datas_t hicanns, hicann_handles_t handles);
	// insert fpga to the set of already configured FPGAs. Not thread-safe!
	void note_fpga_config(fpga_coord const& fpga);
	// note systime start of all currently allocated FPGAs. Not thread-safe!
	void note_systime_start();

	void set_smart();
	void set_skip();
	void set_force();

	ConfigMode fg_config_mode;
	ConfigMode synapse_config_mode;
	ConfigMode synapse_drv_config_mode;
	ConfigMode reset_config_mode;
	ConfigMode repeater_config_mode;
	ConfigMode repeater_locking_config_mode;
	ConfigMode syn_drv_locking_config_mode;

private:
	friend class Wafer;
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, unsigned int const version)
	{
		using namespace boost::serialization;
		ar& make_nvp("mWrittenHICANNData", mWrittenHICANNData);
		if (version == 0) {
			ar& make_nvp("mDidFPGAReset", mDidFPGAConfig);
		}
		if (version > 0) {
			ar& make_nvp("mDidFPGAConfig", mDidFPGAConfig);
			ar& make_nvp("m_started_systime", m_started_systime);
		}
		// m_global_l1_bus_changes is not serialized since it is determined during
		// each configuration
	}
	// pointer to the Wafers HICANN data that was written previously
	std::map<hicann_coord, hicann_data_t> mWrittenHICANNData;
	std::set<fpga_coord> mDidFPGAConfig;

	// global variable to check if relocking is needed. A change on a single HICANN may requires
	// to do the locking for all HICANNs.
	bool m_global_l1_bus_changes;

	// variable to check if systime is started for all allocated FPGAs. Has to be repeated
	// if at least one FPGA is configured.
	bool m_started_systime;

	// need lock since access to mWrittenHICANNData and mDidFPGAConfig can be
	// multithreaded
	omp_lock_t mLock;

	// determines if changes on a single HICANN could affect the global locking of repeaters
	bool check_l1_bus_changes(hicann_coord coord, hicann_data_t const& hicann) const;

	// checks if settings in the synapse drivers have changed. If this is true or changes in the
	// l1 bus were detected this function returns true
	bool syn_drv_locking_needed(hicann_coord coord, hicann_data_t const& hicann) const;

	// determines if locking of repeaters is wanted, i.e. it considers whether relocking is
	// needed and the config mode of repeater locking.
	bool repeater_locking_wanted() const;

	// determines if locking of synapse drivers is wanted, i.e. it takes the config mode as
	// well as the need for locking of the drivers into account.
	bool syn_drv_locking_wanted(hicann_coord const& coord, hicann_data_t const& hicann) const;

	// determines if locking of synapse drivers is wanted for any HICANN in hicann_handels
	bool syn_drv_locking_wanted_any(hicann_handles_t const& handles,
	                                hicann_datas_t const& hicanns) const;
};

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::ParallelHICANNv4SmartConfigurator, 1)
