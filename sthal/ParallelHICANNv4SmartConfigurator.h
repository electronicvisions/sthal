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

// Parallel HICANN v4 Configurator that only configures hardware where
// the configuration has changed since the last config call
// The previous configuration state is saved
// Smart configuration is done for floating gates, synapse drivers, synapse array,
// repeaters and fpgas
class ParallelHICANNv4SmartConfigurator : public ParallelHICANNv4Configurator
{
public:
	typedef ::HMF::Coordinate::HICANNOnWafer hicann_coord;
	typedef ::HMF::Coordinate::FPGAOnWafer fpga_coord;
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
	virtual void lock_repeater(hicann_handle_t const& h, hicann_data_t const& hicann);
	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
	virtual void prime_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	virtual void start_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;

	// smart functions
	void set_hicanns(hicann_datas_t hicanns, hicann_handles_t handles);

	void set_smart();
	void set_skip();
	void set_force();

	ConfigMode fg_config_mode;
	ConfigMode synapse_config_mode;
	ConfigMode synapse_drv_config_mode;
	ConfigMode reset_config_mode;
	ConfigMode repeater_config_mode;

private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, unsigned int const /* version */)
	{
		using namespace boost::serialization;
		ar& make_nvp("mWrittenHICANNData", mWrittenHICANNData);
		ar& make_nvp("mDidFPGAReset", mDidFPGAReset);
	}
	// pointer to the Wafers HICANN data that was written previously
	std::map<hicann_coord, hicann_data_t> mWrittenHICANNData;
	std::set<fpga_coord> mDidFPGAReset;

	// need lock since access to mWrittenHICANNData and mDidFPGAReset can be
	// multithreaded
	omp_lock_t mLock;
};

} // end namespace sthal
