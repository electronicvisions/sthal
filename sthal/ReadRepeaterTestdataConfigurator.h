#pragma once

#ifndef PYPLUSPLUS
#include <tbb/concurrent_unordered_map.h>
#endif

#include "pywrap/compat/macros.hpp"

#include "sthal/HICANNConfigurator.h"
#include "hal/Coordinate/HMFGeometry.h"
#include "hal/HICANNContainer.h"

PYPP_INSTANTIATE(std::vector<size_t>)
typedef std::pair<size_t, size_t> p_s_s_t;
PYPP_INSTANTIATE(p_s_s_t)
PYPP_INSTANTIATE(std::vector< ::HMF::HICANN::L1Address>)

namespace sthal {

class ReadRepeaterTestdataConfigurator : public HICANNConfigurator {
public:
	ReadRepeaterTestdataConfigurator();

	void config_fpga(fpga_handle_t const&, fpga_t const&) PYPP_OVERRIDE {}
	void prime_systime_counter(fpga_handle_t const&) PYPP_OVERRIDE {}
	void start_systime_counter(fpga_handle_t const&) PYPP_OVERRIDE {}
	void disable_global(fpga_handle_t const&) PYPP_OVERRIDE {}

	void config(const fpga_handle_t& f, const hicann_handle_t& h,
	            const hicann_data_t& fg) PYPP_OVERRIDE;

	static log4cxx::LoggerPtr getLogger();

	bool get_full_flag(::HMF::Coordinate::HRepeaterOnWafer) const;

	bool get_full_flag(::HMF::Coordinate::VRepeaterOnWafer) const;

	typedef std::array< ::HMF::HICANN::RepeaterBlock::TestEvent, 3> test_data_t;

	test_data_t get_test_events(::HMF::Coordinate::HRepeaterOnWafer) const;

	test_data_t get_test_events(::HMF::Coordinate::VRepeaterOnWafer) const;

	std::vector< ::HMF::Coordinate::HRepeaterOnHICANN> get_active_hrepeater(
	    ::HMF::Coordinate::HICANNOnWafer) const;
	std::vector< ::HMF::Coordinate::VRepeaterOnHICANN> get_active_vrepeater(
	    ::HMF::Coordinate::HICANNOnWafer) const;

	p_s_s_t analyze_all(std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	                    std::vector<size_t> expected_periods) const;

	bool analyze(bool full_flag, test_data_t received_test_data,
	             std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	             std::vector<size_t> expected_periods) const;

	void add_passive_hrepeater(::HMF::Coordinate::HRepeaterOnWafer hr);

	void add_passive_vrepeater(::HMF::Coordinate::VRepeaterOnWafer vr);

	// add horizontal repeater that is to be ignored in checks
	void add_ignore_hrepeater(::HMF::Coordinate::HRepeaterOnWafer hr);
	// add vertical repeater that is to be ignored in checks
	void add_ignore_vrepeater(::HMF::Coordinate::VRepeaterOnWafer vr);

	// clear ignoring of horizontal repeaters
	void clear_ignore_hrepeater();
	// clear ignoring of vertical repeaters
	void clear_ignore_vrepeater();

	friend std::ostream& operator<<(std::ostream& out,
	                                const ReadRepeaterTestdataConfigurator& cfg);

private:

#ifndef PYPLUSPLUS
	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::set< ::HMF::Coordinate::HRepeaterOnWafer>,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		passive_hrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::set< ::HMF::Coordinate::VRepeaterOnWafer>,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		passive_vrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::set< ::HMF::Coordinate::HRepeaterOnWafer>,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		ignore_hrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::set< ::HMF::Coordinate::VRepeaterOnWafer>,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		ignore_vrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::map< ::HMF::Coordinate::HRepeaterOnWafer,
		std::pair<bool, test_data_t> >,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		result_hr;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNOnWafer,
		std::map< ::HMF::Coordinate::VRepeaterOnWafer,
		std::pair<bool, test_data_t> >,
		std::hash< ::HMF::Coordinate::HICANNOnWafer> >
		result_vr;
#endif

};

} // end namespace sthal
