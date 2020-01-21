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
	typedef ::halco::hicann::v2::AnalogOnHICANN analog_coord;
	typedef ::halco::hicann::v2::DNCGlobal global_dnc_coord;
	typedef ::halco::hicann::v2::Wafer wafer_coord;

	typedef ::hwdb4cpp::FPGAEntry FPGAEntry;
	typedef ::hwdb4cpp::HICANNEntry HICANNEntry;

	struct ADCEntry : ADCConfig {
		::halco::hicann::v2::IPv4 remote_ip;
		::halco::hicann::v2::TCPPort remote_port;
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

	virtual ::halco::hicann::v2::IPv4 get_fpga_ip(const global_fpga_coord& fpga) const PYPP_OVERRIDE;

	size_t get_hicann_version(global_hicann_coord hicann) const PYPP_OVERRIDE;
	halco::hicann::v2::SetupType get_setup_type(wafer_coord wafer) const;

	::halco::hicann::v2::IPv4 get_macu(wafer_coord wafer) const;

	std::string get_hicann_label(global_hicann_coord hicann) const;

	/// Insert new Wafer into the database.
	/// All entries for an existing wafer will be replaced by the new one
	void add_wafer(
	    wafer_coord wafer,
	    halco::hicann::v2::SetupType type,
	    ::halco::hicann::v2::IPv4 macu = ::halco::hicann::v2::IPv4(),
	    size_t macu_version = 0);

	/// Insert an FPGA into the database
	/// An existing FPGA will be replaced
	void add_fpga(global_fpga_coord fpga, halco::hicann::v2::IPv4 ip, bool highspeed = true);

	/// Insert a HICANN into the database, the FPGA belonging to the HICANN
	/// must be already inserted
	/// An existing HICANN will be replaced
	void add_hicann(global_hicann_coord hicann, size_t version, std::string label = "");

	/// Add an ADC to the database
	/// An existing ADC will be replaced
	void add_adc(global_fpga_coord fpga, analog_coord analog, ::HMF::ADC::USBSerial adc,
	             ::halco::hicann::v2::ChannelOnADC channel, ::halco::hicann::v2::TriggerOnADC trigger,
				 ::halco::hicann::v2::IPv4 ip = ::halco::hicann::v2::IPv4(),
				 ::halco::hicann::v2::TCPPort port = ::halco::hicann::v2::TCPPort());

	void add_macu(wafer_coord wafer, ::halco::hicann::v2::IPv4 macu);

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
	typedef halco::hicann::v2::FPGAOnWafer fpga_key;
	typedef halco::hicann::v2::HICANNOnWafer hicann_key;

	typedef std::pair<halco::hicann::v2::DNCOnWafer, halco::hicann::v2::AnalogOnHICANN> adc_key;

	typedef ::hwdb4cpp::WaferEntry WaferEntry;

	void store(std::ostream& out) const;

	WaferEntry& get_wafer(wafer_coord wafer);
	const WaferEntry& get_wafer(wafer_coord wafer) const;

	hwdb4cpp::database mData;
	boost::shared_ptr<const ADCHandleFactory> mAdcFactory;
};

} // end namespace sthal
