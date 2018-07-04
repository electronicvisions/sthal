#include <fstream>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <random>

#include <boost/program_options.hpp>

#include "hal/Coordinate/iter_all.h"
#include "test/hwtest.h"
#include "sthal/Wafer.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/ParallelHICANNNoFGConfigurator.h"
#include "sthal/ParallelHICANNNoResetNoFGConfigurator.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/MagicHardwareDatabase.h"
#include "sthal/Spike.h"
#include "sthal/Timer.h"
#include "logger.h"

namespace po = boost::program_options;
using namespace HMF::Coordinate;

int main(int argc, char* argv[]) {
	sthal::Timer t;
	unsigned int wafer_no;
	double begin;
	size_t mean, no_spikes, maximum_allowed_diff, max_num_print;
	std::vector<unsigned int> hicann_enum;
	std::vector<unsigned int> fpga_enum;
	int seed;
	bool poisson, log_to_file, skip_reset;

	po::options_description bpo_desc("Allowed options");
	bpo_desc.add_options()
		("help", "produce help message")
		("poisson,p", po::value<bool>(&poisson)->default_value(false), "Poisson distributed spikes?")
		("num_spikes,n", po::value<size_t>(&no_spikes)->default_value(100000), "set number of spikes per FPGA")
		("mean,m", po::value<size_t>(&mean)->default_value(500000), "set mean spike rate in HW time")
		("max_diff,d", po::value<size_t>(&maximum_allowed_diff)->default_value(500), "set max allowed diff between sent and received spike time in check")
		("max_diff_print,s", po::value<size_t>(&max_num_print)->default_value(12), "Max count of shown error messages in case of time spike differences")
		("wafer,w", po::value<unsigned int>(&wafer_no)->required(), "set wafer number")
		("begin,b", po::value<double>(&begin)->default_value(2e-6), "set time of first spike")
		("hicann,h", po::value< std::vector<unsigned int>>(&hicann_enum)->multitoken(), "space separated list of HICANNs to be tested")
		("fpga,f", po::value< std::vector<unsigned int>>(&fpga_enum)->multitoken(), "space separated list of FPGAs to be tested")
		("log_to_file,l", po::value<bool>(&log_to_file)->default_value(false), "log is and ought spiketimes to logfile. Filename: spiketimes_<fpganumber>.txt Format: <is> 1 <ought> 1.1 (gnuplot stlye)")
		("skip_reset,r", po::value<bool>(&skip_reset)->default_value(false), "use NoReset configurator?")
		("seed,s", po::value<int>(&seed)->default_value(123), "seed for Poisson generated ISIs");

	po::variables_map parse;
	po::store(po::parse_command_line(argc, argv, bpo_desc), parse);
	po::notify(parse);

	sthal::Wafer wafer(Wafer(Enum{wafer_no}));

	if (parse.count("help")) {
		std::cout << bpo_desc << "\n";
		return 1;
	}

	logger_default_config(log4cxx::Level::getInfo());
	log4cxx::LoggerPtr test_logger = log4cxx::Logger::getLogger("sthal.MultiFPGALoopbackHWTest");

	double dt = 1./double(mean);
	std::vector<double> spike_times;
	for (size_t spike_it = 0; spike_it < no_spikes; spike_it++) {
		spike_times.push_back(spike_it * dt + begin);
	}
	// spike loopback takes at least 26 dnc clock cycles so we need to add an offset for the runtime
	double runtime = spike_times.back() + 1e-6;

	std::set<HICANNOnWafer> used_hicanns;
	if (!hicann_enum.empty()) {
		for(auto hicann_it : hicann_enum) {
			auto ret = used_hicanns.insert(HICANNOnWafer(Enum(hicann_it)));
			if (!ret.second) {
				LOG4CXX_ERROR(test_logger, "Multiple HICANN entry given: " << *(ret.first));
				return EXIT_FAILURE;
			}
		}
	}
	if (!fpga_enum.empty()) {
		for (auto fpga_it : fpga_enum) {
			DNCOnWafer dnc =
			    gridLookupDNCGlobal(FPGAGlobal(FPGAOnWafer(fpga_it), wafer.index()), DNCOnFPGA(0))
			        .toDNCOnWafer();
			for (auto hicann_on_dnc : iter_all<HICANNOnDNC>()) {
				auto ret = used_hicanns.insert(hicann_on_dnc.toHICANNOnWafer(dnc));
				if (!ret.second) {
					LOG4CXX_ERROR(test_logger, "Multiple HICANN entry via FPGA given: " << *(ret.first));
					return EXIT_FAILURE;
				}
			}
		}
	}
	if (hicann_enum.empty() && fpga_enum.empty()) {
		LOG4CXX_ERROR(test_logger, "Neither HICANN nor FPGA given, abort");
		return EXIT_FAILURE;
	}

	std::uniform_real_distribution<double> uniform_distr(0.0,1.0);

	// setup configuration and spiketrains for all hicanns
	for(auto hicann_coord : used_hicanns) {
		// Configure mergers and DNC to output spikes to the dnc
		for (auto merger : iter_all<DNCMergerOnHICANN>() )
		{
			bool odd = bool(merger.value() % 2);
			auto & m = wafer[hicann_coord].layer1[merger];
			m.config = m.MERGE;
			m.slow =  false;
			m.loopback = !odd;
		}

		for(auto channel : iter_all<GbitLinkOnHICANN>() ) {
			if (channel.value()%2) // 1, 3, 5, 7
				wafer[hicann_coord].layer1[channel] = HMF::HICANN::GbitLink::Direction::TO_DNC;
			else
				wafer[hicann_coord].layer1[channel] = HMF::HICANN::GbitLink::Direction::TO_HICANN;
		}

		HMF::HICANN::L1Address neuron_addr{63};
		GbitLinkOnHICANN link;
		double spike_time = begin;
		for(unsigned int linkitter = 0; linkitter < 4; linkitter++) {
			sthal::SpikeVector spikes;
			size_t spikes_per_gbitlink = no_spikes / 4;
			if (no_spikes % 4 > linkitter) {
				spikes_per_gbitlink += 1;
			}
			spikes.reserve(spikes_per_gbitlink);
			std::default_random_engine tx_generator(seed);
			for(size_t itter = 0; itter < spikes_per_gbitlink; itter++) {
				link = GbitLinkOnHICANN( (linkitter) *2 );
				if(poisson) {
					spike_time += (-log(uniform_distr(tx_generator)) / mean );
					spikes.push_back(sthal::Spike(neuron_addr, spike_time));
				} else {
					spikes.push_back(sthal::Spike(neuron_addr, spike_times.at(itter * 4 + linkitter)));
				}
			}
			wafer[hicann_coord].sendSpikes(link, spikes);
		}
	}
	std::vector< ::HMF::Coordinate::FPGAOnWafer> fpgas = wafer.getAllocatedFpgaCoordinates();

	sthal::ExperimentRunner runner{runtime};
	wafer.connect(sthal::MagicHardwareDatabase());
	if (skip_reset) {
		sthal::ParallelHICANNNoResetNoFGConfigurator no_reset_cfg;
		wafer.configure(no_reset_cfg);
	} else {
		sthal::ParallelHICANNNoFGConfigurator cfg;
		wafer.configure(cfg);
	}

	wafer.clearSpikes(true, false);
	wafer.start(runner);

	bool test_failed = false;

	std::vector<double> first_spikes;
	//read out of spike events per FPGA
	for(std::vector< ::HMF::Coordinate::FPGAOnWafer>::iterator fpga_it = fpgas.begin(); fpga_it != fpgas.end(); ++fpga_it) {
		const ::HMF::FPGA::PulseEventContainer & received_spikes = wafer[*fpga_it].getReceivedSpikes();
		const ::HMF::FPGA::PulseEventContainer & sent_spikes = wafer[*fpga_it].getSendSpikes();
		if(received_spikes.size() != sent_spikes.size()) {
			LOG4CXX_ERROR(test_logger, "FPGA " << fpga_it->value() << " unexpected spike count, received: "
			                           << received_spikes.size() << " sent: " << sent_spikes.size());
			test_failed = true;
		} else {
			LOG4CXX_INFO(test_logger, "FPGA " << fpga_it->value() << " sum of Spikes: " << received_spikes.size());
		}
		size_t const num_print_max = received_spikes.size()>max_num_print?max_num_print:received_spikes.size();
		size_t num_print = 0;
		typed_array<size_t, HICANNOnDNC> hicann_failures;
		for( size_t spikenum = 0; spikenum < std::min(received_spikes.size(), sent_spikes.size()) ; spikenum++) {
			uint64_t is_time = received_spikes[spikenum].getTime();
			uint64_t ought_time = sent_spikes[spikenum].getTime();
			int64_t diff = static_cast<int64_t>(is_time) - static_cast<int64_t>(ought_time);
			if (static_cast<unsigned int>(abs(diff)) > maximum_allowed_diff) {
				hicann_failures.at(sent_spikes[spikenum].getChipAddress())++;
				if (num_print < num_print_max) {
					LOG4CXX_ERROR(
					    test_logger, "Spikenum: " << spikenum << "; IS: " << is_time << "; OUGHT: "
					                              << ought_time << "; DIFF: " << diff);
					num_print++;
				}
			}
		}
		if (num_print > 0) {
			test_failed = true;
			for (auto hicann_on_dnc : iter_all<HICANNOnDNC>()) {
				LOG4CXX_ERROR(
				    test_logger,
				    "HICANN : " << hicann_on_dnc << " " << hicann_failures.at(hicann_on_dnc));
			}
		}
		if (log_to_file) {
			std::stringstream spike_data;
			for( size_t spikenum = 0; spikenum < std::max(received_spikes.size(), sent_spikes.size()); spikenum++) {
				size_t received_time = 0;
				size_t sent_time = 0;
				if (received_spikes.size() > spikenum) {
					received_time = received_spikes[spikenum].getTime();
				}
				if (sent_spikes.size() > spikenum) {
					sent_time = sent_spikes[spikenum].getTime();
				}
				// format for simple plotting
				spike_data << received_time << " 1 " << sent_time << " 1.1\n";
			}
			std::ofstream myfile;
			std::string filename = "spiketimes_" + std::to_string(fpga_it->value()) + ".txt";
			myfile.open(filename, std::ofstream::out | std::ofstream::trunc);
			myfile << spike_data.rdbuf();
			myfile.close();
		}
	}
	if (test_failed)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
