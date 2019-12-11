#pragma once

#include <boost/shared_ptr.hpp>

#include "log4cxx/provisionnode.h"

#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"

#include "sthal/FPGA.h"
#include "sthal/HICANN.h"
#include "sthal/Status.h"

#include "redman/resources/Wafer.h"

namespace boost {
namespace serialization {
class access;
} // serialization
} // boost

namespace sthal {

class HardwareDatabase;
class ExperimentRunner;
class HICANNConfigurator;

class Wafer : private boost::noncopyable
{
public:
	typedef ::HMF::Coordinate::Wafer          wafer_coord;
	typedef ::HMF::Coordinate::HICANNOnWafer  hicann_coord;
	typedef ::HMF::Coordinate::FPGAOnWafer    fpga_coord;
	typedef ::HMF::Coordinate::DNCOnWafer     dnc_coord;
	typedef ::HMF::Coordinate::AnalogOnHICANN analog_coord;
	typedef ::HMF::Coordinate::Enum           enum_t;
	typedef ::HMF::Coordinate::IPv4           ip_t;

	typedef boost::shared_ptr< ::HMF::Handle::FPGA >   fpga_handle_t;
	typedef boost::shared_ptr< ::HMF::Handle::HICANN > hicann_handle_t;

	typedef boost::shared_ptr<sthal::FPGA>   fpga_t;
	typedef boost::shared_ptr<sthal::HICANN> hicann_t;

	typedef boost::shared_ptr<redman::resources::Wafer> defects_t;

	Wafer(const wafer_coord & w = wafer_coord(0));

	HICANN & operator[](const hicann_coord & hicann);
	const HICANN & operator[](const hicann_coord & hicann) const;

	FPGA & operator[](const fpga_coord & fpga);
	const FPGA & operator[](const fpga_coord & fpga) const;
	
	/// Returns synapse proxy
	SynapseProxy operator[](const ::HMF::Coordinate::SynapseOnWafer& s);

	/// Returns constant synapse proxy
	SynapseConstProxy operator[](const ::HMF::Coordinate::SynapseOnWafer& s) const;

	/// Get Wafer Coordiante
	wafer_coord const& index() const;

	/// Gets handle
	void connect(const HardwareDatabase & db);

	/// Frees handle
	void disconnect();

	/// Write complete configuration with the default configurator
	void configure();

	/// Write complete configuration
	void configure(HICANNConfigurator & configurator);

	/// Write pulses and start experiment
	void start(ExperimentRunner & runner);

	/// Restart experiment, same as start, but clears received spikes
	void restart(ExperimentRunner & runner);

	/// returns number of allocated hicanns
	size_t allocated() const;

	/// returns number of allocated hicanns
	size_t size() const;

	/// returns number of allocated denmems in the system (including defect ones)
	size_t capacity() const;

	/// returns generale informations
	Status status() const;

	/// force single FPGA mode even with multiple FPGAs
	bool force_listen_local() const;
	void force_listen_local(bool);

	/// Clear spikes
	void clearSpikes(bool received = true, bool send = true);

	/// Dumps the current configuration into a file
	/// @note the hardware connection cannot be dumped
	void dump(const char * const filename, bool overwrite = false) const;
	/// alias to dump
	void save(const char * const filename, bool overwrite = false) const;

	/// Loads a wafer config from a saved
	/// @note after loading the wafer will be disconnected
	void load(const char * const filename);

	/// Get settings shared between all FPGAs
	boost::shared_ptr<FPGAShared> commonFPGASettings();

	/// Vector of all allocated FPGA coordinates
	std::vector<fpga_coord> getAllocatedFpgaCoordinates();

	/// Vector of all allocated HICANN coordinates
	std::vector<hicann_coord> getAllocatedHicannCoordinates();

#ifndef PYPLUSPLUS
	/// Access FPGA handle (only required for low-level access, e.g., closed-loop experiments)
	fpga_handle_t get_fpga_handle(fpga_coord const& fpga);

	/// Access HICANN handle (only required for low-level access, e.g., closed-loop experiments)
	hicann_handle_t get_hicann_handle(hicann_coord const& hicann);
#endif

	void populate_adc_config(hicann_coord const& hicann, analog_coord const& analog);

	bool has(const hicann_coord& hicann) const;

	void drop_defects();
	void set_defects(defects_t wafer);
	defects_t get_defects();

private:
	void allocate(const hicann_coord & hicann);

	/*
	 * Only relevant if the configurator is smart. Checks if the new configuration affects L1
	 * locking and if so tells the smart configurator that locking is needed.
	 */
	void configure_l1_bus_locking(HICANNConfigurator& configurator);

	wafer_coord mWafer;
	halco::common::typed_array<fpga_t,          fpga_coord>   mFPGA;
	halco::common::typed_array<fpga_handle_t,   fpga_coord>   mFPGAHandle;
	halco::common::typed_array<hicann_t,        hicann_coord> mHICANN;
	halco::common::typed_array<halco::common::typed_array<ADCChannel, analog_coord>, dnc_coord> mADCChannels;
	size_t mNumHICANNs;
	bool mForceListenLocal;
	boost::shared_ptr<FPGAShared> mSharedSettings;
#ifndef PYPLUSPLUS
	boost::shared_ptr<const HardwareDatabase> mHardwareDatabase;
#endif

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version);

	friend std::ostream& operator<<(std::ostream& out, Wafer const& obj);

	friend bool operator==(Wafer const& a, Wafer const& b);
	friend bool operator!=(Wafer const& a, Wafer const& b);

	static log4cxx::LoggerPtr getTimeLogger();

	defects_t mDefects;
};

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::Wafer, 4)
BOOST_CLASS_TRACKING(sthal::Wafer, boost::serialization::track_always)

