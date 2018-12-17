#include "sthal/YAMLHardwareDatabase.h"

#include <cctype>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/make_shared.hpp>
#include <fstream>
#include <log4cxx/logger.h>

#include "hal/Handle/FPGAHw.h"
#include "hal/Coordinate/iter_all.h"
#include "sthal/ADCHwHandleFactory.h"
#include "sthal/ADCRemoteHwHandleFactory.h"
#include "sthal/HardwareDatabaseErrors.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("YAMLHardwareDAtabase");

using namespace HMF::Coordinate;

namespace sthal
{

std::ostream& operator<<(std::ostream& out, ::hwdb4cpp::HICANNEntry const& data)
{
	out << "HICANN (v" << data.version;
	if (!data.label.empty()) {
		out << ", " << data.label;
	}
	out << ")";
	return out;
}

std::ostream& operator<<(std::ostream& out, const YAMLHardwareDatabase& database)
{
	out << "YAMLHardwareDatabase:\n";
	database.store(out);
	return out;
}

YAMLHardwareDatabase::YAMLHardwareDatabase(std::string path)
    : mAdcFactory(boost::make_shared<ADCHwHandleFactory>())
{
	if (!path.empty()) {
		load(path);
	}
}

YAMLHardwareDatabase::~YAMLHardwareDatabase()
{
}

bool YAMLHardwareDatabase::has_adc_of_hicann(
	const global_hicann_coord& hicann, const analog_coord& analog) const
{
	return mData.has_adc_entry(::hwdb4cpp::GlobalAnalog_t{hicann.toFPGAGlobal(), analog});
}

ADCConfig YAMLHardwareDatabase::get_adc_of_hicann(const global_hicann_coord& hicann,
                                                  const analog_coord& analog) const
{
	auto entry = mData.get_adc_entry(::hwdb4cpp::GlobalAnalog_t{hicann.toFPGAGlobal(), analog});
	// convert to sthal entry-type
	ADCEntry config;
	config.coord = ::HMF::ADC::USBSerial(entry.coord);
	config.channel = entry.channel;
	config.trigger = entry.trigger;
	config.remote_ip = entry.remote_ip;
	config.remote_port = entry.remote_port;
	config.loadCalibration = entry.loadCalibration;
	if (config.remote_ip.is_unspecified() ) {
		config.factory = mAdcFactory;
	} else {
		config.factory = boost::make_shared<ADCRemoteHwHandleFactory>(config.remote_ip, config.remote_port);
	}
	return config;
}

YAMLHardwareDatabase::fpga_handle_t YAMLHardwareDatabase::get_fpga_handle(
    const global_fpga_coord& fpga,
    const Wafer::fpga_t& fpga_data,
    const std::vector<Wafer::hicann_t>& hicanns) const
{
	const WaferEntry& wafer = get_wafer(fpga.toWafer());

	auto it = wafer.fpgas.find(fpga);
	if (it == wafer.fpgas.end()) {
		throw HardwareDatabaseKeyError("Couldn't find FPGA in database; key =", fpga);
	}
	std::set<DNCOnFPGA> dncs;
	for (auto hicann : hicanns) {
		HICANNGlobal hicann_g(hicann->index(), fpga.toWafer());
		dncs.insert(hicann_g.toDNCOnFPGA());
	}
	if (dncs.size() > 1) {
		throw std::runtime_error(
		    "sorry, multiple DNCs on a single FPGA are not supported by HALbe");
	}
	DNCOnFPGA dnc = *dncs.begin();

	std::set<HICANNOnDNC> available_hicanns;
	std::set<HICANNOnDNC> highspeed_hicanns;
	std::set<HICANNOnDNC> usable_hicanns;
	for (auto hicann_on_dnc : iter_all<HICANNOnDNC>()) {
		global_hicann_coord hicann(hicann_on_dnc.toHICANNOnWafer(dnc.toDNCOnWafer(fpga)),
		                           fpga.toWafer());
		if (has_hicann(hicann)) {
			available_hicanns.insert(hicann_on_dnc);
			if (fpga_data->getHighspeed(hicann_on_dnc)) {
				highspeed_hicanns.insert(hicann_on_dnc);
			}
			if (!fpga_data->getBlacklisted(hicann_on_dnc)) {
				usable_hicanns.insert(hicann_on_dnc);
			}
		}
	}

	for (auto hicann : hicanns) {
		auto const hicann_on_dnc = hicann->index().toHICANNOnDNC();
		if (available_hicanns.find(hicann_on_dnc) == available_hicanns.end()) {
			throw HardwareDatabaseKeyError(
			    "Requested hicann is not available on this FPGA, key =", hicann->index());
		}
	}

	::HMF::Handle::FPGAHw::HandleParameter handleparam{fpga, it->second.ip, dnc, available_hicanns,
	                                                   highspeed_hicanns, usable_hicanns,
	                                                   wafer.setup_type, wafer.macu};
	return fpga_handle_t(new ::HMF::Handle::FPGAHw(handleparam));
}

IPv4 YAMLHardwareDatabase::get_fpga_ip(const global_fpga_coord& fpga) const
{
	auto const& fpga_entry = mData.get_fpga_entry(fpga);
	return fpga_entry.ip;
}

size_t YAMLHardwareDatabase::get_hicann_version(global_hicann_coord hicann) const
{
	auto const& hicann_entry = mData.get_hicann_entry(hicann);
	return hicann_entry.version;
}

std::string YAMLHardwareDatabase::get_hicann_label(global_hicann_coord hicann) const
{
	auto const& hicann_entry = mData.get_hicann_entry(hicann);
	return hicann_entry.label;
}

HMF::Coordinate::SetupType YAMLHardwareDatabase::get_setup_type(wafer_coord wafer) const
{
	auto const& wafer_entry = mData.get_wafer_entry(wafer);
	return wafer_entry.setup_type;
}

::HMF::Coordinate::IPv4 YAMLHardwareDatabase::get_macu(wafer_coord wafer) const
{
	auto const& wafer_entry = mData.get_wafer_entry(wafer);
	return wafer_entry.macu;
}

void YAMLHardwareDatabase::clear()
{
	mData.clear();
}

void YAMLHardwareDatabase::add_wafer(
    wafer_coord wafer, SetupType type, ::HMF::Coordinate::IPv4 macu, size_t macu_version)
{
	::hwdb4cpp::WaferEntry entry;
	entry.setup_type = type;
	entry.macu = macu;
	entry.macu_version = macu_version;
	mData.add_wafer_entry(wafer, entry);
}

void YAMLHardwareDatabase::add_fpga(global_fpga_coord fpga, IPv4 ip, bool highspeed)
{
	::hwdb4cpp::FPGAEntry entry;
	entry.ip = ip;
	entry.highspeed = highspeed;
	mData.add_fpga_entry(fpga, entry);
}

void YAMLHardwareDatabase::add_hicann(global_hicann_coord hicann, size_t version, std::string label)
{
	::hwdb4cpp::HICANNEntry entry;
	entry.version = version;
	entry.label = label;
	mData.add_hicann_entry(hicann, entry);
}

void YAMLHardwareDatabase::add_adc(global_fpga_coord fpga, analog_coord analog, ::HMF::ADC::USBSerial adc,
                                   ChannelOnADC channel, TriggerOnADC trigger,
								   IPv4 ip, TCPPort port)
{
	::hwdb4cpp::ADCEntry entry;
	entry.coord = adc.value();
	entry.channel = channel;
	entry.trigger = trigger;
	entry.loadCalibration = ::hwdb4cpp::ADCEntry::CalibrationMode::LOAD_CALIBRATION;
	entry.remote_ip = ip;
	entry.remote_port = port;
	::hwdb4cpp::GlobalAnalog_t global_analog{fpga, analog};
	mData.add_adc_entry(global_analog, entry);
}

void YAMLHardwareDatabase::add_macu(wafer_coord wafer, ::HMF::Coordinate::IPv4 macu)
{
	WaferEntry& wafer_entry = get_wafer(wafer);
	wafer_entry.macu = macu;
}

void YAMLHardwareDatabase::remove_fpga(global_fpga_coord fpga)
{
	mData.remove_fpga_entry(fpga);
}

void YAMLHardwareDatabase::remove_hicann(global_hicann_coord hicann)
{
	mData.remove_hicann_entry(hicann);
}

void YAMLHardwareDatabase::remove_adc(global_fpga_coord fpga, analog_coord analog)
{
	::hwdb4cpp::GlobalAnalog_t coord{fpga, analog};
	mData.remove_adc_entry(coord);
}

bool YAMLHardwareDatabase::has_fpga(global_fpga_coord fpga) const
{
	return mData.has_fpga_entry(fpga);
}

bool YAMLHardwareDatabase::has_hicann(global_hicann_coord hicann) const
{
	return mData.has_hicann_entry(hicann);
}

bool YAMLHardwareDatabase::has_adc(global_fpga_coord fpga, analog_coord analog) const
{
	return mData.has_adc_entry(::hwdb4cpp::GlobalAnalog_t{fpga, analog});
}

YAMLHardwareDatabase::WaferEntry& YAMLHardwareDatabase::get_wafer(wafer_coord wafer)
{
	return mData.get_wafer_entry(wafer);
}

const YAMLHardwareDatabase::WaferEntry& YAMLHardwareDatabase::get_wafer(wafer_coord wafer) const
{
	return mData.get_wafer_entry(wafer);
}

void YAMLHardwareDatabase::store(std::string path) const
{
	std::ofstream fout(path);
	mData.dump(fout);
}

void YAMLHardwareDatabase::load(std::string path)
{
	mData.load(path);
}


void YAMLHardwareDatabase::store(std::ostream& out) const
{
	mData.dump(out);
}

boost::shared_ptr<HardwareDatabase> YAMLHardwareDatabase::clone() const
{
	return boost::shared_ptr<HardwareDatabase>(new YAMLHardwareDatabase(*this));
}

} // end namespace sthal
