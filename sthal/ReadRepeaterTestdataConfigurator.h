#pragma once

#ifndef PYPLUSPLUS
#include <tbb/concurrent_unordered_map.h>
#endif

#include "pywrap/compat/macros.hpp"

#include "sthal/HICANNConfigurator.h"
#include "halco/hicann/v2/fwd.h"
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

	bool get_full_flag(::halco::hicann::v2::HRepeaterOnWafer) const;

	bool get_full_flag(::halco::hicann::v2::VRepeaterOnWafer) const;

	typedef std::array< ::HMF::HICANN::RepeaterBlock::TestEvent, 3> test_data_t;

	test_data_t get_test_events(::halco::hicann::v2::HRepeaterOnWafer) const;

	test_data_t get_test_events(::halco::hicann::v2::VRepeaterOnWafer) const;

	std::vector< ::halco::hicann::v2::HRepeaterOnHICANN> get_active_hrepeater(
	    ::halco::hicann::v2::HICANNOnWafer) const;
	std::vector< ::halco::hicann::v2::VRepeaterOnHICANN> get_active_vrepeater(
	    ::halco::hicann::v2::HICANNOnWafer) const;

	// first: bad repeaters, second: good repeaters
	std::pair<
	    std::vector< ::halco::hicann::v2::VRepeaterOnWafer>,
	    std::vector< ::halco::hicann::v2::VRepeaterOnWafer> >
	analyze_vrepeater(
	    std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	    std::vector<size_t> expected_periods) const;

	// first: bad repeaters, second: good repeaters
	std::pair<
	    std::vector< ::halco::hicann::v2::HRepeaterOnWafer>,
	    std::vector< ::halco::hicann::v2::HRepeaterOnWafer> >
	analyze_hrepeater(
	    std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	    std::vector<size_t> expected_periods) const;

	p_s_s_t analyze_all(std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	                    std::vector<size_t> expected_periods) const;

	static bool analyze(bool full_flag, test_data_t received_test_data,
	                    std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	                    std::vector<size_t> expected_periods);

	void add_passive_hrepeater(::halco::hicann::v2::HRepeaterOnWafer hr);

	void add_passive_vrepeater(::halco::hicann::v2::VRepeaterOnWafer vr);

	// add horizontal repeater that is to be ignored in checks
	void add_ignore_hrepeater(::halco::hicann::v2::HRepeaterOnWafer hr);
	// add vertical repeater that is to be ignored in checks
	void add_ignore_vrepeater(::halco::hicann::v2::VRepeaterOnWafer vr);

	// clear ignoring of horizontal repeaters
	void clear_ignore_hrepeater();
	// clear ignoring of vertical repeaters
	void clear_ignore_vrepeater();

	friend std::ostream& operator<<(std::ostream& out,
	                                const ReadRepeaterTestdataConfigurator& cfg);

private:

#ifndef PYPLUSPLUS
	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::set< ::halco::hicann::v2::HRepeaterOnWafer>,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		passive_hrepeater_map;

	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::set< ::halco::hicann::v2::VRepeaterOnWafer>,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		passive_vrepeater_map;

	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::set< ::halco::hicann::v2::HRepeaterOnWafer>,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		ignore_hrepeater_map;

	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::set< ::halco::hicann::v2::VRepeaterOnWafer>,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		ignore_vrepeater_map;

	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::map< ::halco::hicann::v2::HRepeaterOnWafer,
		std::pair<bool, test_data_t> >,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		result_hr;

	tbb::concurrent_unordered_map<
		::halco::hicann::v2::HICANNOnWafer,
		std::map< ::halco::hicann::v2::VRepeaterOnWafer,
		std::pair<bool, test_data_t> >,
		std::hash< ::halco::hicann::v2::HICANNOnWafer> >
		result_vr;
#endif

};

} // end namespace sthal
