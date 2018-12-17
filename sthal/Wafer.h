#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "boost/serialization/array.h"

#include "log4cxx/provisionnode.h"

#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"

#include "sthal/HICANN.h"
#include "sthal/Status.h"

#include "redman/resources/Wafer.h"

namespace sthal {

class HardwareDatabase;
class ExperimentRunner;
class HICANNConfigurator;

class Wafer : private boost::noncopyable
{
	typedef ::HMF::Coordinate::Wafer          wafer_coord;
	typedef ::HMF::Coordinate::HICANNOnWafer  hicann_coord;
	typedef ::HMF::Coordinate::FPGAOnWafer    fpga_coord;
	typedef ::HMF::Coordinate::DNCOnWafer     dnc_coord;
	typedef ::HMF::Coordinate::AnalogOnHICANN analog_coord;
	typedef ::HMF::Coordinate::Enum           enum_t;
	typedef ::HMF::Coordinate::IPv4           ip_t;

public:

	typedef boost::shared_ptr< ::HMF::Handle::FPGA >   fpga_handle_t;
	typedef boost::shared_ptr< ::HMF::Handle::HICANN > hicann_handle_t;

	typedef boost::shared_ptr<sthal::FPGA>   fpga_t;
	typedef boost::shared_ptr<sthal::HICANN> hicann_t;

	Wafer(const wafer_coord & w = wafer_coord(0));

	HICANN & operator[](const hicann_coord & hicann);
	const HICANN & operator[](const hicann_coord & hicann) const;

	FPGA & operator[](const fpga_coord & fpga);
	const FPGA & operator[](const fpga_coord & fpga) const;
	
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

private:
	void allocate(const hicann_coord & hicann);

	wafer_coord mWafer;
	std::array<fpga_t,          fpga_coord::size>   mFPGA;
	std::array<fpga_handle_t,   fpga_coord::size>   mFPGAHandle;
	std::array<hicann_t,        hicann_coord::enum_type::size> mHICANN;
	std::array<std::array<ADCChannel, analog_coord::size>, dnc_coord::enum_type::size> mADCChannels;
	size_t mNumHICANNs;
	bool mForceListenLocal;
	boost::shared_ptr<FPGAShared> mSharedSettings;
#ifndef PYPLUSPLUS
	boost::shared_ptr<const HardwareDatabase> mHardwareDatabase;
#endif

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("wafer", mWafer);
		// When loading version 1 archives with array size != 12 special care is needed
		// saving is handeled by the definition of the archive version
		if (version < 2 && typename Archiver::is_loading()) {
			// This version fixed the size change of FPGAOnWafer from 12 to
			// 48 make the loading, for wafers with id > 2 this change
			// would be hard to fix, when loading.
			// I assume that such archives almost shouldn't exists.
			// If they should you would have to fix this somehow
			throw std::runtime_error("De-serialization of old StHal version (<2) not supported");
		}
		else {
			ar & make_nvp("fpgas", mFPGA);
		}
		ar & make_nvp("hicanns", mHICANN)
		   & make_nvp("adc_channels", mADCChannels)
		   & make_nvp("num_hicanns", mNumHICANNs);
		if (version == 0)
		{
			//mSharedSettings.reset(new FPGAShared());
		}
		else
		{
			ar & make_nvp("fpga_shared_settings", mSharedSettings);
		}
		if (version > 2) {
			ar & make_nvp("wafer_with_backend", mWaferWithBackend);
		}
	}

	friend std::ostream& operator<<(std::ostream& out, Wafer const& obj);

	static log4cxx::LoggerPtr getTimeLogger();

	redman::resources::WaferWithBackend mWaferWithBackend;
};

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::Wafer, 3)
