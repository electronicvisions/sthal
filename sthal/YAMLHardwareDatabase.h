#pragma once

#include "hal/ADC/USBSerial.h"
#include "hal/Coordinate/HMFGrid.h"
#include "hwdb4cpp/hwdb4cpp.h"
#include "sthal/HardwareDatabase.h"

namespace sthal
{

/// This class interfaces the YAML-based Hardware Database (aka `hwdb`).
class YAMLHardwareDatabase : public HardwareDatabase
{
public:
	typedef ::HMF::Coordinate::AnalogOnHICANN analog_coord;
	typedef ::HMF::Coordinate::DNCGlobal global_dnc_coord;
	typedef ::HMF::Coordinate::Wafer wafer_coord;

	typedef ::hwdb4cpp::FPGAEntry FPGAEntry;
	typedef ::hwdb4cpp::HICANNEntry HICANNEntry;

	struct ADCEntry : ADCConfig {
		::HMF::Coordinate::IPv4 remote_ip;
		::HMF::Coordinate::TCPPort remote_port;
	};

	/// @arg path: Path to the database file, if empty no file is loaded
	YAMLHardwareDatabase(std::string = "");
	~YAMLHardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
	    const global_fpga_coord& fpga,
	    const Wafer::fpga_t& fpga_data,
	    std::vector<Wafer::hicann_t> const& hicanns) const PYPP_OVERRIDE;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const PYPP_OVERRIDE;
	virtual ADCConfig get_adc_of_hicann(const global_hicann_coord& hicann,
	                                    const analog_coord& analog) const PYPP_OVERRIDE;

	virtual ::HMF::Coordinate::IPv4 get_fpga_ip(const global_fpga_coord& fpga) const PYPP_OVERRIDE;

	size_t get_hicann_version(global_hicann_coord hicann) const PYPP_OVERRIDE;
	HMF::Coordinate::SetupType get_setup_type(wafer_coord wafer) const;

	::HMF::Coordinate::IPv4 get_macu(wafer_coord wafer) const;

	std::string get_hicann_label(global_hicann_coord hicann) const;

	/// Insert new Wafer into the database.
	/// All entries for an existing wafer will be replaced by the new one
	void add_wafer(wafer_coord wafer, HMF::Coordinate::SetupType type,
	               ::HMF::Coordinate::IPv4 macu = ::HMF::Coordinate::IPv4());

	/// Insert an FPGA into the database
	/// An existing FPGA will be replaced
	void add_fpga(global_fpga_coord fpga, HMF::Coordinate::IPv4 ip, bool highspeed = true);

	/// Insert a HICANN into the database, the FPGA belonging to the HICANN
	/// must be already inserted
	/// An existing HICANN will be replaced
	void add_hicann(global_hicann_coord hicann, size_t version, std::string label = "");

	/// Add an ADC to the database
	/// An existing ADC will be replaced
	void add_adc(global_fpga_coord fpga, analog_coord analog, ::HMF::ADC::USBSerial adc,
	             ::HMF::Coordinate::ChannelOnADC channel, ::HMF::Coordinate::TriggerOnADC trigger,
				 ::HMF::Coordinate::IPv4 ip = ::HMF::Coordinate::IPv4(),
				 ::HMF::Coordinate::TCPPort port = ::HMF::Coordinate::TCPPort());

	void add_macu(wafer_coord wafer, ::HMF::Coordinate::IPv4 macu);

	/// remove fpga from database, all HICANN belonging to the FPGA ar also
	/// removed
	void remove_fpga(global_fpga_coord fpga);
	/// remove a HICANN from the database
	void remove_hicann(global_hicann_coord hicann);
	/// remove an ADC from the database
	void remove_adc(global_fpga_coord fpga, analog_coord analog);

	/// Check for the existents of keys
	bool has_fpga(global_fpga_coord fpga) const;
	bool has_hicann(global_hicann_coord fpga) const;
	bool has_adc(global_fpga_coord fpga, analog_coord analog) const;

	/// Clears the complete database
	void clear();
	/// Load the database from file
	void load(std::string path);
	/// Store the database in file
	void store(std::string path) const;

	boost::shared_ptr<HardwareDatabase> clone() const;

	friend std::ostream& operator<<(std::ostream& out, const YAMLHardwareDatabase& database);

private:
	typedef HMF::Coordinate::FPGAOnWafer fpga_key;
	typedef HMF::Coordinate::HICANNOnWafer hicann_key;

	typedef std::pair<HMF::Coordinate::DNCOnWafer, HMF::Coordinate::AnalogOnHICANN> adc_key;

	typedef ::hwdb4cpp::WaferEntry WaferEntry;

	void store(std::ostream& out) const;

	WaferEntry& get_wafer(wafer_coord wafer);
	const WaferEntry& get_wafer(wafer_coord wafer) const;

	hwdb4cpp::database mData;
	boost::shared_ptr<const ADCHandleFactory> mAdcFactory;
};

} // end namespace sthal
