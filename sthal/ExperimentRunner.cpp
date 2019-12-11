#include <algorithm>
#include <sstream>
#include <chrono>
#include <sstream>
#include <thread>

extern "C" {
#include <omp.h>
}

#include <log4cxx/logger.h>

#include "sthal/ExperimentRunner.h"
#include "sthal/ExperimentRunnerHelper.h"

#include "hal/backend/HICANNBackend.h"
#include "hal/backend/DNCBackend.h"
#include "hal/backend/FPGABackend.h"
#include "hal/Coordinate/iter_all.h"

#include "sthal/HICANN.h"
#include "sthal/Timer.h"

using namespace ::HMF::Coordinate;

namespace sthal {

log4cxx::LoggerPtr ExperimentRunner::getLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.ExperimentRunner");
	return _logger;
}

log4cxx::LoggerPtr ExperimentRunner::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.ExperimentRunner.Time");
	return _logger;
}

ExperimentRunner::ExperimentRunner(double run_time_in_s, bool drop_background_events)
	: m_run_time_in_us(run_time_in_s * 1e6), m_drop_background_events(drop_background_events)
{
}

ExperimentRunner::~ExperimentRunner()
{
}

void ExperimentRunner::run(const fpga_list & fpgas, const fpga_handle_list & handles)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(
		getLogger(), "starting experiment run using "
		"at most " << omp_get_max_threads() << " OpenMP threads");
	sort_spikes(fpgas);
	send_spikes(fpgas, handles);
	start_experiment(fpgas, handles);
	receive_spikes(fpgas, handles);
	LOG4CXX_INFO(
	    getLogger(), "execution took " << t.get_ms() << "ms"
	                                   << " for an experiment run of" << run_time_in_s() * 1e3
	                                   << "ms");
}

void ExperimentRunner::sort_spikes(fpga_list const& fpgas)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "Sorting spike trains");
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};

		if (fpgas.at(fpga_c)) {
			fpgas.at(fpga_c)->sortSpikes();
		}
	}
	LOG4CXX_INFO(getTimeLogger(), "Sorting Spike trains took " << t.get_ms() << "ms");

}

void ExperimentRunner::send_spikes(const fpga_list & fpgas, const fpga_handle_list & handles)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	auto logger = getLogger();
	LOG4CXX_DEBUG(logger, "sending spikes to FPGAs");

	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};

		if (fpgas.at(fpga_c) && handles.at(fpga_c)) {
			FPGA& fpga = *fpgas.at(fpga_c);
			::HMF::Handle::FPGA& handle = *handles.at(fpga_c);

			// reset hostal FPGA timestamp counter
			::HMF::FPGA::reset_pbmem(handle);

			// upload spike data and insert end-of-experiment marker
			auto const& spikes = fpga.getSendSpikes();
			LOG4CXX_INFO(logger, "sending " << spikes.size()
			                                << " to FPGA: " << handle.coordinate());
			FPGA::PulseEvent::spiketime_t const endtime =
				FPGA::dnc_freq_in_MHz * this->m_run_time_in_us;
			::HMF::FPGA::write_playback_program(
			    handle, spikes, endtime, fpga.commonFPGASettings()->getFPGAHICANNDelay(),
			    fpga.hasOutboundMergers(), m_drop_background_events);
		}
	}

	// FIXME: move expecting-answer-timeout to communication layer
	std::chrono::milliseconds const timeout{1000};

	// Wait for response from all FPGAs that experiment was written to Playback memory
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		if (!(fpgas.at(fpga_c) && handles.at(fpga_c)))
			continue;
		HMF::Handle::FPGA& handle = *handles.at(fpga_c);

		auto const start = std::chrono::steady_clock::now();
		auto now = start;

		// wait for new data in 10us slices (for up to 1s, cf. FIXME above)
		while (!HMF::FPGA::get_pbmem_buffering_completed(handle)) {
			std::chrono::microseconds const sleep_interval{10};
			std::this_thread::sleep_for(sleep_interval);
			now = std::chrono::steady_clock::now();
			if ((now - start) > timeout) {
				std::stringstream debug_msg;
				debug_msg << "write_playback_program: no end of experiment response from FPGA "
				          << handle.coordinate();
				throw std::runtime_error(debug_msg.str());
			}
		}
	}
	LOG4CXX_INFO(getTimeLogger(), "sending spikes to FPGAs took " << t.get_ms() << "ms");
}

void ExperimentRunner::start_experiment(const fpga_list & fpgas, const fpga_handle_list & handles)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	auto logger = getLogger();

	LOG4CXX_DEBUG(logger, "sending experiment start to FPGAs");
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		if (fpgas.at(fpga_c) && handles.at(fpga_c)) {
			::HMF::Handle::FPGA& handle = *handles.at(fpga_c);
			LOG4CXX_INFO(logger, "prime experiment of FPGA: " << handle.coordinate());
			::HMF::FPGA::prime_experiment(handle);
		}
	}

	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		if (fpgas.at(fpga_c) && handles.at(fpga_c)) {
			::HMF::Handle::FPGA& handle = *handles.at(fpga_c);
			LOG4CXX_DEBUG(logger, "sending start signal to FPGA: " << handle.coordinate());
			::HMF::FPGA::start_experiment(handle);
		}
	}
	LOG4CXX_INFO(getTimeLogger(), "sending experiment start to FPGAs took " << t.get_ms() << "ms");
}

void ExperimentRunner::receive_spikes(const fpga_list & fpgas, const fpga_handle_list & handles)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	auto logger = getLogger();
	LOG4CXX_DEBUG(logger, "receiving spikes from FPGAs");
	// L1Address(0) is reserved for background events
	const ::HMF::HICANN::L1Address bkg_address(0);

	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		if (fpgas.at(fpga_c) && handles.at(fpga_c)) {
			::HMF::Handle::FPGA& handle = *handles.at(fpga_c);
			FPGA& fpga = *fpgas.at(fpga_c);
			FPGA::PulseEvent::spiketime_t const duration =
				FPGA::dnc_freq_in_MHz * this->m_run_time_in_us;
			auto const& result = ::HMF::FPGA::read_trace_pulses(handle, duration);

			size_t const total_events = result.size();
			if (!m_drop_background_events) {
				size_t background_events = 0;
				for (auto const& pulse_event : result) {
					if (pulse_event.getNeuronAddress() == bkg_address) {
						++background_events;
					}
				}
				LOG4CXX_INFO(
					logger, "received " << total_events
					<< " (" << (total_events - background_events)
					<< ") spikes (not background) from FPGA: " << handle.coordinate());
			} else {
				LOG4CXX_INFO(logger, "received " << total_events << " spike(s) from FPGA: " << handle.coordinate());
			}

			fpga.setReceivedPulseEvents(std::move(result));

			size_t const max_total_events = this->m_run_time_in_us*1e-6 * FPGA::gbitlink_max_throughput;
			for (auto hicann : fpga.getAllocatedHICANNs()) {
				size_t n_spikes = 0;
				for (auto link : iter_all<GbitLinkOnHICANN>()) {
					n_spikes += fpga.getReceivedSpikes(hicann, link).size();
				}

				double const warn_threshold = 0.9;
				if (n_spikes >= max_total_events * warn_threshold) {
					LOG4CXX_WARN(
					    logger, short_format(hicann)
					                << ": number of received spikes " << n_spikes << " close (>= "
					                << warn_threshold * 100 << " %) to saturation of "
					                << max_total_events << " events (given a bandwidth of "
					                << FPGA::gbitlink_max_throughput / 1e6 << " MEvent/s)");
				}
			}

		}
	}
	LOG4CXX_INFO(getTimeLogger(), "receiving spikes from FPGAs took " << t.get_ms() << "ms");
}

std::ostream& operator<<(std::ostream& out, ExperimentRunner const& obj)
{
	out << "ExperimentRunner(" << obj.time() << "us)";
	return out;
}

} // end namespace sthal
