#include <chrono>
#include <thread>

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

#include "sthal/Wafer.h"
#include "sthal/ConfigurationStages.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/HardwareDatabase.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/HICANNv4Configurator.h"
#include "sthal/ParallelHICANNv4Configurator.h"
#include "sthal/ParallelHICANNNoFGConfigurator.h"
#include "sthal/Settings.h"
#include "sthal/FPGA.h"
#include "sthal/AnalogRecorder.h"
#include "sthal/Timer.h"
#include "sthal/Defects.h"

#include "pythonic/enumerate.h"
#include "pythonic/zip.h"

#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"
#include "hal/Coordinate/iter_all.h"
#include "hal/Coordinate/FormatHelper.h"
#include "hal/backend/HMFBackend.h"

#include "sthal_git_version.h"

using namespace ::HMF::Coordinate;

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

std::vector<Wafer::fpga_coord> Wafer::getAllocatedFpgaCoordinates()
{
	std::vector<Wafer::fpga_coord> ret;
	std::copy_if(iter_all<FPGAOnWafer>().begin(), iter_all<FPGAOnWafer>().end(),
	             std::back_inserter(ret), [this](FPGAOnWafer const coord) { return mFPGA[coord]; });
	return ret;
}

std::vector<Wafer::hicann_coord> Wafer::getAllocatedHicannCoordinates()
{
	std::vector<Wafer::hicann_coord> ret;
	std::copy_if(iter_all<HICANNOnWafer>().begin(), iter_all<HICANNOnWafer>().end(),
	             std::back_inserter(ret),
	             [this](HICANNOnWafer const coord) { return mHICANN[coord.id()]; });
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
	::HMF::Coordinate::HICANNGlobal hg{hicann, index()};
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

void Wafer::drop_defects() {
	mDefects.reset();
}

void Wafer::allocate(const hicann_coord& c)
{
	if (!has(c)) {
		throw std::runtime_error(short_format(c) + " is not available");
	}

	if (!mHICANN[c.id()])
	{
		::HMF::Coordinate::HICANNGlobal hicann_global(c, mWafer);
	    ::HMF::Coordinate::FPGAGlobal f = hicann_global.toFPGAGlobal();
		auto & fpga = mFPGA[f.value()];
		auto & hicann = mHICANN[c.id()];
		if (!fpga)
		{
			fpga.reset(new FPGA(f, mSharedSettings));
		}
		hicann.reset(new HICANN(hicann_global, fpga, *this));
		fpga->add_hicann(hicann_global, hicann);
		LOG4CXX_INFO(logger, "allocate HICANN: " << hicann_global);

		const size_t num_fpgas = std::count_if(
		    mFPGA.begin(), mFPGA.end(),
		    [](const boost::shared_ptr<FPGA>& f) { return f != nullptr; });
		// allocate master fpga if more than one fpga is used
		if (!mForceListenLocal && (num_fpgas > 1)) {
			::HMF::Coordinate::FPGAGlobal f{::HMF::Coordinate::FPGAOnWafer::Master,
			                                mWafer};
			auto& fpga = mFPGA[f.value()];
			if (fpga == nullptr) {
				fpga.reset(new FPGA(f, mSharedSettings));
				LOG4CXX_INFO(logger, "allocate master FPGA: " << f);
			}
		}
		++mNumHICANNs;
	}
	else
	{
		// TODO: what else?
	}
}

namespace {
	std::string printHICANNS(std::vector< ::HMF::Coordinate::HICANNOnWafer> const& hicanns)
	{
		std::stringstream out;
		out << "{";
		if (!hicanns.empty()) {
			out << " ";
			std::copy(hicanns.begin(), hicanns.end() - 1,
			          std::ostream_iterator< ::HMF::Coordinate::HICANNOnWafer>(out, ", "));
			out << hicanns.back() << " ";
		}
		out << "}";
		return out.str();
	}
}

void Wafer::populate_adc_config(hicann_coord const& hicann, analog_coord const& analog)
{
	auto h = mHICANN[hicann.id()];

	if(!mHardwareDatabase) {
		throw std::runtime_error("Wafer::populate_adc_config(): connect to HardwareDatabase first");
	}
	LOG4CXX_DEBUG(logger, "Retrieving ADC config for " << hicann << " " << analog);
	auto conf = mHardwareDatabase->get_adc_of_hicann(HICANNGlobal(hicann, index()), analog);
	h->setADCConfig(analog, conf);
	auto& adc_channel = mADCChannels[hicann.toDNCOnWafer().id()][analog.value()];
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
				hicanns.push_back(mHICANN.at(hicann.toEnum()));
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

	for (auto ii : getAllocatedHicannCoordinates() )
	{
		HICANNGlobal hicann(ii, mWafer);
		auto & h = mHICANN[hicann.toHICANNOnWafer().id()];
		h->set_version(db.get_hicann_version(hicann));
	}
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

	/// First reset / configure FPGAs
	const size_t num_fpgas = mFPGA.size();
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		// Skip if FPGA is not allocated or FPGA has no allocated HICANNs,
		// i.e. master FPGA that is not used in experiment besides global systart
		if (!fpga || fpga->getAllocatedHICANNs().empty()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.config_fpga(fpga_handle, fpga);
	}

	std::map<ConfigurationStage, size_t> const serial_stage_sleep = {
	    {ConfigurationStage::TIMING_UNCRITICAL, 0}};
	const std::vector<ConfigurationStage> serial_stages = {
	    ConfigurationStage::TIMING_UNCRITICAL};

	auto const& settings = Settings::get();
	auto const& parallel_stage_sleep = settings.configuration_stages.sleeps;
	auto const& parallel_stages = settings.configuration_stages.order;

	auto const call_stages =
	    (is_hicann_parallel && !is_v4_cfg) ? parallel_stages : serial_stages;
	auto const sleep_stages =
	    (is_hicann_parallel && !is_v4_cfg) ? parallel_stage_sleep : serial_stage_sleep;

	/// Check for invalid/dangerous HICANN configuration
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		if (!fpga || fpga->getAllocatedHICANNs().empty()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		for (HICANNOnWafer hicann_c : fpga->getAllocatedHICANNs()) {
			if (auto hicann = mHICANN.at(hicann_c.id())) {
				std::stringstream errors;
				errors << "Dangerous/invalid HICANN configuration of "
				       << short_format(hicann->index()) << "!\n";

				bool const ok = hicann->check(errors);

				if (!ok && settings.ignore_hicann_checks) {
					LOG4CXX_WARN(logger, errors.str());
				} else if (!ok && !settings.ignore_hicann_checks) {
					LOG4CXX_ERROR(logger, errors.str());
					throw std::runtime_error(errors.str());
				} else {
					LOG4CXX_DEBUG(logger, short_format(hicann->index()) << ": checks passed.");
				}
			}
		}
	}

	/// Then configure HICANNs
	for (auto stage : call_stages) {
		#pragma omp parallel for schedule(dynamic)
		for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
			fpga_t fpga = mFPGA.at(fpga_counter);
			fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
			if (!fpga || fpga->getAllocatedHICANNs().empty()) {
				continue;
			}
			check_fpga_handle(fpga_handle, fpga);
			std::vector<HICANNOnWafer> hicann_coords;
			ParallelHICANNv4Configurator::hicann_handles_t hicann_handles;
			ParallelHICANNv4Configurator::hicann_datas_t hicann_datas;
			for (HICANNOnWafer hicann_c : fpga->getAllocatedHICANNs()) {
				if (auto hicann = mHICANN.at(hicann_c.id())) {
					hicann_coords.push_back(hicann_c);
					hicann_handles.push_back(fpga_handle->get(hicann_c));
					hicann_datas.push_back(hicann);
				}
			}

			if (is_hicann_parallel && !is_v4_cfg) {
				parallel_configurator->config(fpga_handle, hicann_handles, hicann_datas,
				                              stage);
			} else {
				assert(hicann_handles.size() == hicann_datas.size());
				for (auto item : pythonic::zip(hicann_handles, hicann_datas)) {
					configurator.config(fpga_handle, std::get<0>(item),
					                    std::get<1>(item));
				}
			}
		} // fpgas
		auto const sleep_interval = std::chrono::milliseconds(sleep_stages.at(stage));
		LOG4CXX_DEBUG(logger, "sleeping for " << sleep_interval.count() << " ms after stage "
		                                      << static_cast<size_t>(stage));
		std::this_thread::sleep_for(sleep_interval);
	} // stages

    #pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

	/// prime systime counters on all fpgas
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		// Skip if FPGA is not allocated, do as well for master FPGA
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.prime_systime_counter(fpga_handle);
	}

	/// start systime counters on all fpgas
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		if (!fpga) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.start_systime_counter(fpga_handle);
	}

	// for all but master
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		if (!fpga || fpga_handle->isMaster()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

	// for master
	#pragma omp parallel for schedule(dynamic)
	for (size_t fpga_counter = 0; fpga_counter < num_fpgas; ++fpga_counter) {
		fpga_t fpga = mFPGA.at(fpga_counter);
		fpga_handle_t fpga_handle = mFPGAHandle.at(fpga_counter);
		if (!fpga || !fpga_handle->isMaster()) {
			continue;
		}
		check_fpga_handle(fpga_handle, fpga);
		configurator.disable_global(fpga_handle);
	}

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
		auto & h = mHICANN[coord.id()];
		if (h)
		{
            h->resetADCConfig();
		}
	}
	LOG4CXX_INFO(plogger, "Disconnected from hardware");
}


HICANN & Wafer::operator[](const hicann_coord & hicann)
{
	auto& ret = mHICANN.at(hicann.id());
	if (!ret)
	{
		allocate(hicann);
	}
	return *ret;
}

const HICANN & Wafer::operator[](const hicann_coord & hicann) const
{
	auto const& ret = mHICANN.at(hicann.id());
	if (!ret)
	{
		throw std::runtime_error("HICANN not allocated");
	}
	return *ret;
}

FPGA &
Wafer::operator[](const fpga_coord & fc)
{
	auto & fpga = mFPGA[fc.value()];
	if (!fpga)
	{
		fpga.reset(new FPGA(::HMF::Coordinate::FPGAGlobal(fc, index()) , mSharedSettings));
	}
	return *fpga;
}

const FPGA &
Wafer::operator[](const fpga_coord & fc) const
{
	auto & fpga = mFPGA[fc.value()];
	if (!fpga)
	{
		throw std::runtime_error("FPGA not allocated");
	}
	return *fpga;
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

	for (size_t ii=0; ii<fpga_coord::size; ++ii)
	{
		auto handle = mFPGAHandle[ii];
		if (handle)
		{
			auto fpga_st = ::HMF::FPGA::get_fpga_status(*handle);
			st.fpga_id[ii]  = fpga_st.getHardwareId();
			st.fpga_rev[ii] = fpga_st.get_git_hash();
		}
		else
		{
			st.fpga_id[ii] = st.fpga_rev[ii] = 0;
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
}

} // sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::Wafer)

EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::Wafer)
