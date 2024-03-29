#include "Status.h"

#include "pythonic/enumerate.h"
#include "halco/common/iter_all.h"

namespace sthal {

bool operator==(const ADCChannel & a, const ADCChannel & b)
{
	return
	   	a.board_id == b.board_id &&
		a.channel == b.channel &&
		a.bitfile_version == b.bitfile_version;
}

uint16_t Status::get_dropped_pulses_from_fpga(Status::fpga_coord fpga,
						Status::hicann_on_dnc_coord hicann)
{
	return fpga_drops[fpga][hicann];
}

std::ostream & operator<<(std::ostream & stream, const Status & st)
{
	std::stringstream out;
	out << "Status of " << st.wafer << ":\n"
		<< "git revisions in use:\n"
		<< "\thicann-system: " << st.git_rev_hicann_system << "\n"
		<< "\thalbe:         " << st.git_rev_halbe << "\n"
		<< "\tstahl:         " << st.git_rev_sthal << "\n"
		<< "Hicanns in use: ";
	for (auto h : st.hicanns)
	{
		out << h << " ";
	}
	out << "\n";
	out << "FPGA revisions (0, means not used): ";
	for (auto rev : st.fpga_rev) {
		out << std::hex << rev << std::dec << " ";
	}
	out << "\n";
	out << "FPGA ids (0, means not used): ";
	for (auto id : st.fpga_id) {
		out << std::hex << id << std::dec << " ";
	}
	out << "\n";
	out << "FPGA TX FIFO Drop counts: ";
	for (auto it : pythonic::enumerate(st.fpga_drops)) {
		for (auto hicann : halco::common::iter_all<halco::hicann::v2::HICANNOnDNC>()) {
			auto hicann_drops = it.second[hicann];
			if (hicann_drops) {
				out << "FPGA " << it.first << ", " << hicann <<
					" dropped " << hicann_drops << " pulses; ";
			}
		}
	}
	out << "\n";
	out << "ADCChannels:" << std::endl;
	auto empty = HMF::ADC::USBSerial();
	for (auto it : st.adc_channels) {
		for (auto ac : pythonic::enumerate(it)) {
			if (ac.second.board_id != empty) {
				out << "AnalogOnHICANN: " << ac.first << '\n';
				out << '\t' << ac.second << std::endl;
			}
		}
	}

	stream << out.str();
	return stream;
}

std::ostream& operator<<(std::ostream& out, ADCChannel const& obj)
{
	out << "ADCChannel(";
	out << "board_id=" << obj.board_id << ", ";
	out << "channel=" << obj.channel << ", ";
	out << "bitfile_version=\"" << obj.bitfile_version << "\")";
	return out;
}
}  // end namespace sthal

