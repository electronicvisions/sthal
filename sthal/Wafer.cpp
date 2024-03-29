#include <chrono>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "boost/serialization/array.h"

#include "boost/serialization/serialization_helper.tcc"

extern "C" {
#include <omp.h>
}

#include "sthal/AnalogRecorder.h"
#include "sthal/ConfigurationStages.h"
#include "sthal/Defects.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/FPGA.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/HICANNv4Configurator.h"
#include "sthal/HardwareDatabase.h"
#include "sthal/ParallelHICANNv4SmartConfigurator.h"
#include "sthal/Settings.h"
#include "sthal/Timer.h"
#include "sthal/Wafer.h"

#include "pythonic/enumerate.h"
#include "pythonic/zip.h"

#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"
#include "halco/common/iter_all.h"
#include "halco/hicann/v2/format_helper.h"
#include "hal/backend/HMFBackend.h"

#include "slurm/vision_defines.h"

#include "sthal_git_version.h"

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace {

void check_fpga_handle(sthal::Wafer::fpga_handle_t const& fpga_handle,
                       sthal::Wafer::fpga_t const& fpga) {
	if (!fpga_handle) {
		std::stringstream message;
		message << "FPGA handle of " << fpga->coordinate()
		        << " missing. You have to connect first to the hardware.";
		throw std::runtime_error(message.str().c_str());
	}
}

std::vector<FPGAGlobal> get_fpgas_from_licenses()
{
	std::string const licenses = (std::getenv(vision_slurm_hardware_licenses_env_name) != nullptr)
	                                 ? std::getenv(vision_slurm_hardware_licenses_env_name)
	                                 : "";

	std::vector<std::string> tokens;
	boost::split(tokens, licenses, boost::is_any_of(","));

	std::vector<std::string> fpga_licenses;
	std::copy_if(
	    std::begin(tokens), std::end(tokens), std::back_inserter(fpga_licenses),
	    [](std::string fl) {
		    try {
			    auto const fs = from_string(fl);
			    return boost::get<FPGAGlobal>(&fs) != nullptr;
		    } catch (std::runtime_error const& e) {
			    return false;
		    }
	    });

	return from_string<FPGAGlobal>(fpga_licenses);
}
}

namespace sthal {

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("sthal.wafer");
static log4cxx::LoggerPtr plogger = log4cxx::Logger::getLogger("progress.sthal");

log4cxx::LoggerPtr Wafer::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.wafer.Time");
	return _logger;
}

Wafer::Wafer(const wafer_coord & w) :
	mWafer(w),
	mFPGA(),
	mFPGAHandle(),
	mHICANN(),
	mADCChannels(),
	mNumHICANNs(0),
	mForceListenLocal(false),
	mSharedSettings(new FPGAShared()),
	mDefects(load_resources_wafer(index()))
{
}

Wafer::wafer_coord const& Wafer::index() const
{
	return mWafer;
}

std::vector<Wafer::fpga_coord> Wafer::getAllocatedFpgaCoordinates() const
{
	std::vector<Wafer::fpga_coord> ret;
	std::copy_if(iter_all<FPGAOnWafer>().begin(), iter_all<FPGAOnWafer>().end(),
	             std::back_inserter(ret), [this](FPGAOnWafer const coord) { return mFPGA[coord]; });
	return ret;
}

std::vector<Wafer::hicann_coord> Wafer::getAllocatedHicannCoordinates() const
{
	std::vector<Wafer::hicann_coord> ret;
	std::copy_if(iter_all<HICANNOnWafer>().begin(), iter_all<HICANNOnWafer>().end(),
	             std::back_inserter(ret),
	             [this](HICANNOnWafer const coord) { return mHICANN[coord]; });
	return ret;
}

Wafer::fpga_handle_t
Wafer::get_fpga_handle(fpga_coord const& fpga)
{
	fpga_handle_t fpga_handle = mFPGAHandle[fpga];
	if (!fpga_handle)
		throw std::runtime_error("FPGA not connected?!");
	return fpga_handle;
}

Wafer::hicann_handle_t
Wafer::get_hicann_handle(hicann_coord const& hicann)
{
	::halco::hicann::v2::HICANNGlobal hg{hicann, index()};
	fpga_handle_t fpga_handle = get_fpga_handle(hg.toFPGAOnWafer());
	hicann_handle_t hh = fpga_handle->get(hicann);
	return hh;
}

bool Wafer::has(hicann_coord const& hicann) const
{
	if (mDefects) {
		return mDefects->has(hicann);
	}
	return true;
}

void Wafer::set_defects(defects_t wafer)
{
	mDefects = wafer;
}

Wafer::defects_t Wafer::get_defects()
{
	return mDefects;
}

void Wafer::drop_defects() {
	mDefects.reset();
}

void Wafer::allocate(const hicann_coord& c)
{
	if (!has(c)) {
		throw std::runtime_error(short_format(c) + " is not available");
	}

	if (!mHICANN[c])
	{
		::halco::hicann::v2::HICANNGlobal hicann_global(c, mWafer);
	    ::halco::hicann::v2::FPGAGlobal f = hicann_global.toFPGAGlobal();
		(*this)[f.toFPGAOnWafer()]; // allocates FPGA
		auto & fpga = mFPGA[f];
		auto & hicann = mHICANN[c];
		hicann.reset(new HICANN(hicann_global, fpga, this));
		fpga->add_hicann(hicann_global, hicann);
		LOG4CXX_INFO(logger, "allocate HICANN: " << hicann_global);
		++mNumHICANNs;
	}
	else
	{
		// TODO: what else?
	}
}

namespace {
	std::string printHICANNS(std::vector< ::halco::hicann::v2::HICANNOnWafer> const& hicanns)
	{
		std::stringstream out;
		out << "{";
		if (!hicanns.empty()) {
			out << " ";
			std::copy(hicanns.begin(), hicanns.end() - 1,
			          std::ostream_iterator< ::halco::hicann::v2::HICANNOnWafer>(out, ", "));
			out << hicanns.back() << " ";
		}
		out << "}";
		return out.str();
	}
}

void Wafer::populate_adc_config(hicann_coord const& hicann, analog_coord const& analog)
{
	auto h = mHICANN[hicann];

	if(!mHardwareDatabase) {
		throw std::runtime_error("Wafer::populate_adc_config(): connect to HardwareDatabase first");
	}
	LOG4CXX_DEBUG(logger, "Retrieving ADC config for " << hicann << " " << analog);
	if (!mHardwareDatabase->has_adc_of_hicann(HICANNGlobal(hicann, index()), analog)) {
		throw std::runtime_error(
		    "Wafer::populate_adc_config(): " + short_format(hicann) + " (" +
		    short_format(hicann.toFPGAOnWafer()) + ") analog " + std::to_string(analog) +
		    " has no ADC entry in HWDB");
	}
	auto conf = mHardwareDatabase->get_adc_of_hicann(HICANNGlobal(hicann, index()), analog);
	h->setADCConfig(analog, conf);
	auto& adc_channel = mADCChannels[hicann.toDNCOnWafer()][analog];
	conf.loadCalibration = ADCConfig::CalibrationMode::DEFAULT_CALIBRATION;
	adc_channel.board_id = conf.coord;
	adc_channel.channel = conf.channel;
	AnalogRecorder const r(conf);
	adc_channel.bitfile_version = r.version();
}

void Wafer::connect(const HardwareDatabase & db)
{
	mHardwareDatabase = db.clone();

	const size_t num_fpgas = std::count_if(
	    mFPGA.begin(), mFPGA.end(), [](const boost::shared_ptr<FPGA>& f) { return f != nullptr; });

	if (mDefects) {
		for (auto fpga : mFPGA) {
			if (!fpga) {
				continue;
			}
			auto const defects_fpga = mDefects->get(fpga->coordinate());
			for (auto hicann : iter_all<HICANNOnDNC>()) {
				auto const hicann_on_wafer = hicann.toHICANNOnWafer(fpga->coordinate());

				bool const hs_user = fpga->getHighspeed(hicann);
				bool const hs_defects = defects_fpga->hslinks()->has(hicann.toHighspeedLinkOnDNC());
				if (hs_user != hs_defects) {
					LOG4CXX_WARN(
					    logger, "Overriding highspeed setting for " << short_format(hicann_on_wafer)
					                                                << " to " << hs_defects);
					fpga->setHighspeed(hicann, hs_defects);
				}

				bool const bl_user = fpga->getBlacklisted(hicann);
				bool const bl_defects = !mDefects->hicanns()->has(hicann_on_wafer);
				if (bl_user != bl_defects) {
					LOG4CXX_WARN(
					    logger, "Overriding blacklisting for " << short_format(hicann_on_wafer)
					                                           << " to " << bl_defects);
					fpga->setBlacklisted(hicann, bl_defects);
				}
			}
		}
	}

	for (auto coord : getAllocatedFpgaCoordinates() )
	{
		if (!mFPGAHandle[coord])
		{
			FPGAGlobal const fpga_global{coord, mWafer};
			auto hicann_coords = mFPGA[coord]->getAllocatedHICANNs();
			std::vector<hicann_t> hicanns;
			for (auto hicann : hicann_coords) {
				hicanns.push_back(mHICANN.at(hicann));
			}

			mFPGAHandle[coord] = db.get_fpga_handle(fpga_global, mFPGA[coord], hicanns);
			// multi fpga experiment
			if (num_fpgas > 1 && !mForceListenLocal) {
				mFPGAHandle.at(coord)->setListenGlobalMode(true);
			}
			LOG4CXX_DEBUG(
			    logger, "connected to FPGA: " << fpga_global << " using HICANNS "
			                                  << printHICANNS(hicann_coords));
		}
	}

	std::vector<FPGAOnWafer> const allocated_fpga_coordinates = getAllocatedFpgaCoordinates();
	std::vector<FPGAGlobal> const allocated_fpgas_by_licenses = get_fpgas_from_licenses();

	std::vector<FPGAGlobal> allocated_fpgas_by_experiment(allocated_fpga_coordinates.size());
	std::transform(
	    std::begin(allocated_fpga_coordinates), std::end(allocated_fpga_coordinates),
	    std::back_inserter(allocated_fpgas_by_experiment),
	    [this](auto f) { return FPGAGlobal(f, index()); });

	std::vector<FPGAGlobal> diff;
	std::set_difference(
	    std::begin(allocated_fpgas_by_licenses), std::end(allocated_fpgas_by_licenses),
	    std::begin(allocated_fpgas_by_experiment), std::end(allocated_fpgas_by_experiment),
	    std::inserter(diff, begin(diff)));

	for (auto const& f : diff) {
		LOG4CXX_WARN(logger, "License of " << to_string(f) << " not required by experiment");
	}

	for (auto ii : getAllocatedHicannCoordinates() )
	{
		HICANNGlobal hicann(ii, mWafer);
		auto & h = mHICANN[hicann.toHICANNOnWafer()];
		h->set_version(db.get_hicann_version(hicann));
	}
	mConnected = true;
	LOG4CXX_INFO(plogger, "Connected to hardware");
}

void Wafer::configure() {
	ParallelHICANNv4Configurator default_configurator;
	configure(default_configurator);
}

void Wafer::configure(HICANNConfigurator & configurator)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(plogger, "Configure hardware");

	/* Deactivating OpenMP-based parallelization for Python-based instances of
	 * `HICANNConfigurator`s. This is a workaround for non-thread-safe access to
	 * the Python interpreter (via `get_override()` in boost::python). A more
	 * scalable solution would check if the class does really "override" the
	 * C++-based code. However, this would require changes to Py++-generated
	 * code (or boost python code). The DeParallelizeForPython object will reduce
	 * the thread count to 1 on construction and resets it to previous count on
	 * destruction. */

	auto* const parallel_configurator =
	    dynamic_cast<ParallelHICANNv4Configurator*>(&configurator);
	bool const is_hicann_parallel = (parallel_configurator != nullptr);

	auto* const v4_configurator = dynamic_cast<HICANNv4Configurator*>(&configurator);
	bool const is_v4_cfg = (v4_configurator != nullptr);

	// check for global changes configuration changes
	configure_l1_bus_locking(configurator);

	/// First reset / configure FPGAs
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		// Skip if FPGA is not allocated or FPGA has no allocated HICANNs,
		// i.e. master FPGA that is not used in experiment besides global systart
		if (!fpga || fpga->getAllocatedHICANNs().empty()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.config_fpga(fpga_handle, fpga);
	}

	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

	/// prime systime counters on all fpgas
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		// Skip if FPGA is not allocated, do as well for master FPGA
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.prime_systime_counter(fpga_handle);
	}

	/// start systime counters on all fpgas
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.start_systime_counter(fpga_handle);
	}

	// for all but master
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		if (!fpga || fpga_handle->isMaster()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

	// for master
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		if (!fpga || !fpga_handle->isMaster()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

	// systime counters are started on all allocated FPGAs, which is noted for the
	// smart configurator
	note_systime_start(configurator);

	std::map<ConfigurationStage, size_t> const serial_stage_sleep = {
	    {ConfigurationStage::TIMING_UNCRITICAL, 0}};
	const std::vector<ConfigurationStage> serial_stages = {
	    ConfigurationStage::TIMING_UNCRITICAL};

	auto const& settings = Settings::get();
	auto const& parallel_stage_sleep = settings.configuration_stages.sleeps;
	auto const& parallel_stages = settings.configuration_stages.order;

	auto const call_stages =
	    (is_hicann_parallel) ? parallel_stages : serial_stages;
	auto const sleep_stages =
	    (is_hicann_parallel) ? parallel_stage_sleep : serial_stage_sleep;

	/// Check for invalid/dangerous HICANN configuration
	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		fpga_t fpga = mFPGA.at(fpga_c);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
		if (!fpga || fpga->getAllocatedHICANNs().empty()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		for (HICANNOnWafer hicann_c : fpga->getAllocatedHICANNs()) {
			if (auto hicann = mHICANN.at(hicann_c)) {
				std::stringstream errors;
				errors << "Dangerous/invalid HICANN configuration of "
				       << short_format(hicann->index()) << "!\n";

				switch (settings.hicann_checks_mode) {
					case Settings::HICANNChecksMode::Skip:
						LOG4CXX_DEBUG(logger, short_format(hicann->index()) << ": checks skipped.");
						break;
					case Settings::HICANNChecksMode::CheckButIgnore: {
						if (!hicann->check(errors)) {
							LOG4CXX_WARN(logger, errors.str());
						} else {
							LOG4CXX_DEBUG(
							    logger, short_format(hicann->index()) << ": checks passed.");
						}
						break;
					}
					case Settings::HICANNChecksMode::Check: {
						if (!hicann->check(errors)) {
							LOG4CXX_ERROR(logger, errors.str());
							throw std::runtime_error("HICANN software checks failed");
						} else {
							LOG4CXX_DEBUG(
							    logger, short_format(hicann->index()) << ": checks passed.");
						}
						break;
					}
					default:
						throw std::runtime_error("Unknown HICANN checks mode");
				}
			}
		}
	}

	/// Then configure HICANNs
	for (auto stage : call_stages) {
		/// Comment the following #pragma line while debugging, in order to get
		/// meaningful error messages.
		#pragma omp parallel for schedule(dynamic)
		for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
			FPGAOnWafer const fpga_c{Enum(fpga_enum)};
			fpga_t fpga = mFPGA.at(fpga_c);
			fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_c);
			if (!fpga || fpga->getAllocatedHICANNs().empty()) {
				continue;
			}
			check_fpga_handle(fpga_handle, fpga);
			std::vector<HICANNOnWafer> hicann_coords;
			ParallelHICANNv4Configurator::hicann_handles_t hicann_handles;
			ParallelHICANNv4Configurator::hicann_datas_t hicann_datas;
			for (HICANNOnWafer hicann_c : fpga->getAllocatedHICANNs()) {
				if (auto hicann = mHICANN.at(hicann_c)) {
					hicann_coords.push_back(hicann_c);
					hicann_handles.push_back(fpga_handle->get(hicann_c));
					hicann_datas.push_back(hicann);
				}
			}

			if (is_hicann_parallel && !is_v4_cfg) {
				parallel_configurator->config(fpga_handle, hicann_handles, hicann_datas,
				                              stage);
			} else if (is_v4_cfg) {
				// The HICANNv4Configurator is a wrapper of the ParallelHICANNv4Configurator
				// which offers the same functionality as its base class but performs
				// HICANN configuration sequentially, i.e. configure one HICANN at a time
				// (c.f. header of HICANNv4Configurator): That different behavior is achieved
				// here, since the configuration calls will be either parallel or sequential
				// depending on the configurator provided to this Wafer::configure function.
				// We don't use ParallelHICANNConfigurator::config(fpga_handle_t f,
				// hicann_handle_t h, hicann_data_t hd) since that function doesn't sleep
				// after every configuration stage.
				assert(hicann_handles.size() == hicann_datas.size());
				for (auto item : pythonic::zip(hicann_handles, hicann_datas)) {
					v4_configurator->config(
						fpga_handle,
						ParallelHICANNv4Configurator::hicann_handles_t{std::get<0>(item)},
						ParallelHICANNv4Configurator::hicann_datas_t{std::get<1>(item)},
						stage);
				}
			} else {
				// Any other configurator that does not inherit from ParallelHICANNv4Configurator
				assert(hicann_handles.size() == hicann_datas.size());
				for (auto item : pythonic::zip(hicann_handles, hicann_datas)) {
					configurator.config(fpga_handle, std::get<0>(item),
					                    std::get<1>(item));
				}
			}

			// sync command buffers
			configurator.sync_command_buffers(fpga_handle, hicann_handles);
		} // fpgas
		if (sleep_stages.at(stage)) {
			auto const sleep_interval = std::chrono::milliseconds(sleep_stages.at(stage));
			LOG4CXX_DEBUG(
			    logger, "sleeping for " << sleep_interval.count() << " ms after stage "
			                            << static_cast<size_t>(stage));
			std::this_thread::sleep_for(sleep_interval);
		}
	} // stages

	LOG4CXX_DEBUG(getTimeLogger(), short_format(index())
		      << ": configure took " << t.get_ms()
		      << "ms");
}

void Wafer::start(ExperimentRunner & runner)
{
	LOG4CXX_DEBUG(plogger, "Start experiment");
	runner.run(mFPGA, mFPGAHandle);
}


void Wafer::restart(ExperimentRunner & runner)
{
	LOG4CXX_DEBUG(plogger, "Restart experiment");
	clearSpikes(true, false);
	runner.run(mFPGA, mFPGAHandle);
}


void Wafer::disconnect()
{
	for (auto & item : mFPGAHandle)
	{
		if (item.use_count() > 1)
			throw std::runtime_error("There are stray FPGA Handles, can't disconnect properly");
	}
	for (auto & item : mFPGAHandle)
	{
		item.reset();
	}
	for (auto coord : iter_all<HICANNOnWafer>())
	{
		auto & h = mHICANN[coord];
		if (h)
		{
            h->resetADCConfig();
		}
	}
	mConnected = false;
	LOG4CXX_INFO(plogger, "Disconnected from hardware");
}


HICANN & Wafer::operator[](const hicann_coord & hicann)
{

	auto& ret = mHICANN.at(hicann);
	if (!ret)
	{
		if (mConnected) {
			throw std::runtime_error(
			    "Allocation of new HICANN " + to_string(hicann) +
			    " not allowed when already connected");
		}
		allocate(hicann);
	}
	return *ret;
}

const HICANN & Wafer::operator[](const hicann_coord & hicann) const
{
	auto const& ret = mHICANN.at(hicann);
	if (!ret)
	{
		throw std::runtime_error("HICANN not allocated");
	}
	return *ret;
}

FPGA &
Wafer::operator[](const fpga_coord & fc)
{
	auto & fpga = mFPGA[fc];
	if (!fpga)
	{
		if (mConnected) {
			throw std::runtime_error(
			    "Allocation of new FPGA " + to_string(fc) + " not allowed when already connected");
		}
		fpga.reset(new FPGA(::halco::hicann::v2::FPGAGlobal(fc, index()) , mSharedSettings));

		const size_t num_fpgas = std::count_if(
		    mFPGA.begin(), mFPGA.end(),
		    [](const boost::shared_ptr<FPGA>& f) { return f != nullptr; });
		// allocate master fpga if more than one fpga is used
		if (!mForceListenLocal && (num_fpgas > 1)) {
			::halco::hicann::v2::FPGAGlobal f{::halco::hicann::v2::FPGAOnWafer::Master,
			                                mWafer};
			auto& fpga = mFPGA[f];
			if (fpga == nullptr) {
				fpga.reset(new FPGA(f, mSharedSettings));
				LOG4CXX_INFO(logger, "allocate master FPGA: " << f);
			}
		}
	}
	return *fpga;
}

const FPGA &
Wafer::operator[](const fpga_coord & fc) const
{
	auto & fpga = mFPGA[fc];
	if (!fpga)
	{
		throw std::runtime_error("FPGA not allocated");
	}
	return *fpga;
}

SynapseProxy Wafer::operator[](const ::halco::hicann::v2::SynapseOnWafer& s)
{
	return (*this)[s.toHICANNOnWafer()].synapses[s.toSynapseOnHICANN()];
}

SynapseConstProxy Wafer::operator[](const ::halco::hicann::v2::SynapseOnWafer& s) const
{
	return (*this)[s.toHICANNOnWafer()].synapses[s.toSynapseOnHICANN()];
}

size_t Wafer::allocated() const
{
	return mNumHICANNs;
}

size_t Wafer::size() const
{
	return HICANNOnWafer::enum_type::end;
}

size_t Wafer::capacity() const
{
	return size() * HICANN::capacity();
}

Status Wafer::status() const
{
	Status st;
	st.adc_channels = this->mADCChannels;

	st.wafer = mWafer;
	for (auto hicann : mHICANN)
	{
		if(hicann)
		{
			st.hicanns.push_back(hicann->index());
		}
	}

	st.git_rev_halbe = ::HMF::Debug::getHalbeGitVersion();
	st.git_rev_hicann_system = ::HMF::Debug::getHicannSystemGitVersion();
	st.git_rev_sthal = STHAL_GIT_VERSION;

	for (auto ii : iter_all<FPGAOnWafer>())
	{
		auto handle = mFPGAHandle[ii];
		if (handle)
		{
			auto fpga_st = ::HMF::FPGA::get_fpga_status(*handle);
			st.fpga_id[ii]  = fpga_st.getHardwareId();
			st.fpga_rev[ii] = fpga_st.get_git_hash();
			st.fpga_drops[ii] = fpga_st.get_hicann_dropped_pulses_at_fpga_tx_fifo();
		}
		else
		{
			st.fpga_id[ii] = st.fpga_rev[ii] = 0;
			st.fpga_drops[ii] = {};
		}
	}
	return st;
}

bool Wafer::force_listen_local() const
{
	return mForceListenLocal;
}

void Wafer::force_listen_local(bool force)
{
	mForceListenLocal = force;
}

/// Clear spikes
void Wafer::clearSpikes(bool received, bool send)
{
	for (auto & fpga : mFPGA)
	{
		if(fpga && received)
			fpga->clearReceivedSpikes();
		if(fpga && send)
			fpga->clearSendSpikes();
	}
}

void Wafer::dump(const char * const _filename, bool overwrite) const
{
	boost::filesystem::path filename(_filename);
	if (!overwrite && boost::filesystem::exists(filename))
	{
		std::string err("File '");
		err += filename.c_str();
		err += "' allready exists!";
		throw std::runtime_error(err);
	}
	boost::filesystem::ofstream stream(filename);
	if (filename.extension() == ".xml") {
		LOG4CXX_INFO(logger, "Wafer config (xml) dump to: " << filename);
		boost::archive::xml_oarchive{stream} << boost::serialization::make_nvp("wafer", *this);
	} else {
		LOG4CXX_INFO(logger, "Wafer config (binary) dump to: " << filename);
		boost::archive::binary_oarchive{stream} << *this;
	}
}

void Wafer::save(const char* const _filename, bool overwrite) const
{
	dump(_filename, overwrite);
}

void Wafer::load(const char * const _filename)
{
	boost::filesystem::path filename(_filename);
	if (!boost::filesystem::exists(filename))
	{
		std::string err("File '");
		err += filename.c_str();
		err += "' not found!";
		throw std::runtime_error(err);
	}
	boost::filesystem::ifstream stream(filename);
	disconnect();
	if (filename.extension() == ".xml") {
		LOG4CXX_INFO(logger, "Wafer config (xml) load from: " << filename);
		boost::archive::xml_iarchive{stream} >> boost::serialization::make_nvp("wafer", *this);
	} else {
		LOG4CXX_INFO(logger, "Wafer config (binary) load from: " << filename);
		boost::archive::binary_iarchive{stream} >> *this;
	}
}

boost::shared_ptr<FPGAShared> Wafer::commonFPGASettings()
{
	return mSharedSettings;
}

void Wafer::configure_l1_bus_locking(HICANNConfigurator& configurator)
{
	// only relevant if configurator is smart
	auto* const smart_configurator =
		dynamic_cast<ParallelHICANNv4SmartConfigurator*>(&configurator);
	bool const is_smart_cfg = (smart_configurator != nullptr);

	if (!is_smart_cfg)
	{
		return;
	}

	bool locking_needed = false;

	for (size_t fpga_enum = 0; fpga_enum < FPGAOnWafer::end; ++fpga_enum) {
		FPGAOnWafer const fpga_c{Enum(fpga_enum)};
		const fpga_t fpga = mFPGA.at(fpga_c);
		if (!fpga || fpga->getAllocatedHICANNs().empty()) {
			continue;
		}
		for (HICANNOnWafer coord : fpga->getAllocatedHICANNs()) {
			auto hicann = mHICANN.at(coord);
			locking_needed |= smart_configurator->check_l1_bus_changes(coord, hicann);
		}
	}

	smart_configurator->m_global_l1_bus_changes = locking_needed;
}

void Wafer::note_systime_start(HICANNConfigurator& configurator)
{
	// only relevant if configurator is smart
	auto* const smart_configurator =
		dynamic_cast<ParallelHICANNv4SmartConfigurator*>(&configurator);
	bool const is_smart_cfg = (smart_configurator != nullptr);

	if (!is_smart_cfg)
	{
		return;
	}
	smart_configurator->note_systime_start();
}

std::ostream& operator<<(std::ostream& out, Wafer const& obj)
{
	out << obj.mWafer << ":" << std::endl;
	out << "- FPGA handles: ";
	for (auto it : obj.mFPGA) {
		out << it << " ";
	}
	out << std::endl;
	out << "- HICANN handles (" << obj.mNumHICANNs;
	out << " total):" << std::endl;
	for (auto it : obj.mHICANN) {
		out << it << " ";
	}
	out << std::endl;
	out << obj.status();

	return out;
}

template<typename Archiver>
void Wafer::serialize(Archiver & ar, unsigned int const version)
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
	if (version == 3) {
		throw std::runtime_error("De-serialization of sthal::Wafer version 3 not supported");
	}
	if (version > 3) {
		ar& make_nvp("defects", mDefects);
	}
	if (version > 3) {
		ar & make_nvp("wafer", mWafer);
	}
}

bool operator==(Wafer const& a, Wafer const& b)
{
	// boost::shared_ptr does not support reasonable comparisons...
	for (auto i : iter_all<FPGAOnWafer>()) {
		if (static_cast<bool>(a.mFPGA[i]) != static_cast<bool>(b.mFPGA[i])) {
			return false;
		} else if (static_cast<bool>(a.mFPGA[i]) && ((*a.mFPGA[i]) != (*b.mFPGA[i]))) {
			return false;
		}
	}

	for (auto i : iter_all<HICANNOnWafer>()) {
		if (static_cast<bool>(a.mHICANN[i]) != static_cast<bool>(b.mHICANN[i])) {
			return false;
		} else if (static_cast<bool>(a.mHICANN[i]) && ((*a.mHICANN[i]) != (*b.mHICANN[i]))) {
			return false;
		}
	}
	return (a.mWafer == b.mWafer)
		&& (a.mADCChannels == b.mADCChannels)
		&& (a.mNumHICANNs == b.mNumHICANNs)
		&& ((static_cast<bool>(a.mSharedSettings) == static_cast<bool>(b.mSharedSettings)) &&
			(static_cast<bool>(a.mSharedSettings) && ((*a.mSharedSettings) == (*b.mSharedSettings))))
		// TODO: missing comparison operators
		//&& (a.mWaferWithBackend == b.mWaferWithBackend)
	;
}

bool operator!=(Wafer const& a, Wafer const& b)
{
	return !(a == b);
}

} // sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::Wafer)

EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::Wafer)
