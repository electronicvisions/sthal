#include "sthal/ESSRunner.h"

#include <memory>

#include <log4cxx/logger.h>

#include "sthal/ExperimentRunnerHelper.h"
#include "hal/Handle/FPGAEss.h"
#include "sthal/Timer.h"
#include "hal/backend/FPGABackend.h"

#include "hal/Coordinate/iter_all.h"

using namespace HMF::Handle;

namespace sthal {

ESSRunner::ESSRunner(double run_time_in_s, const ESSConfig& cfg)
    : ExperimentRunner(run_time_in_s), mConfig(cfg)
{
}

ESSRunner::~ESSRunner()
{}

void ESSRunner::run(const fpga_list & fpgas,
					const fpga_handle_list & handles)
{
	Timer t;
	LOG4CXX_DEBUG(getLogger(), "starting ESS run");

	assert ( fpgas.size() == handles.size() );

	for ( size_t i = 0; i < fpgas.size(); ++i)
	{
		auto & handle = handles[i];
		auto & fpga = fpgas[i];

		if (handle) {
			FPGAEss& fpga_handle = dynamic_cast<FPGAEss&>(*handle);
			fpga->sortSpikes();

			FPGA::PulseEvent::spiketime_t const endtime =
				FPGA::dnc_freq_in_MHz * this->time();

			// send_spikes(fpgas, handles);
			::HMF::FPGA::write_playback_pulses(
				fpga_handle,
				fpga->getSendSpikes(),
				endtime,
				 /* fpga_hicann_delay */ 62 // 62 fpga_clk cycles are 496 nano seconds (corresponds to L2_TRANSMIT_DELAY=500ns from systemsim/global_src/systemc/sim_def.h), FIXME: hardcoded delay
				);

			::HMF::FPGA::prime_experiment(fpga_handle);
			::HMF::FPGA::start_experiment(fpga_handle);
		}
	}

	for ( size_t i = 0; i < fpgas.size(); ++i)
	{
		auto & handle = handles[i];

		if (handle) {
			// start_experiment(fpgas, handles);
			FPGAEss& fpga_handle = dynamic_cast<FPGAEss&>(*handle);

			// apply ESS config
			Ess& ess_handle = fpga_handle.ess_handle();
			ess_handle.setSpeedup(mConfig.speedup);
			ess_handle.setTimestep(mConfig.timestep);
			ess_handle.setWeightDistortion(mConfig.enable_weight_distortion, mConfig.weight_distortion);
			ess_handle.enableTimedMerger(mConfig.enable_timed_merger);
			ess_handle.enableSpikeDebugging(mConfig.enable_spike_debugging);
			ess_handle.setPulseStatisticsFile(mConfig.pulse_statistics_file);
			ess_handle.setCalibPath(mConfig.calib_path);

			// FIXME: PM: unit of mTime / ExperimentRunner::time()  not documented
			fpga_handle.runESS(time() * 1e3);

			break;
		}
	}

	bool const drop_bg_events = drop_background_events();
	for ( size_t i = 0; i < fpgas.size(); ++i)
	{
		auto & handle = handles[i];
		auto & fpga = fpgas[i];

		if (handle) {

			FPGAEss& fpga_handle = dynamic_cast<FPGAEss&>(*handle);

			// receive_spikes(fpgas, handles);
			auto result = ::HMF::FPGA::read_trace_pulses(fpga_handle, drop_bg_events);

			size_t background_events = result.dropped_events;
			size_t total_events = result.events.size() + background_events;
			if (!drop_bg_events) {
				// L1Address(0) is reserved for background events
				const ::HMF::HICANN::L1Address bkg_address(0);
				for (auto const& pulse_event : result.events) {
					if (pulse_event.getNeuronAddress() == bkg_address) {
						++background_events;
					}
				}
			}

			fpga->setReceivedPulseEvents(std::move(result.events));

			LOG4CXX_INFO(
				getLogger(), "received " << total_events
				<< " (" << (total_events - background_events)
				<< ") spikes (not background) from FPGA: " << fpga_handle.coordinate());
		}
	}

	LOG4CXX_DEBUG(getLogger(), "ESS run took " << t.get_ms() << "ms");
}

} // end namespace sthal
