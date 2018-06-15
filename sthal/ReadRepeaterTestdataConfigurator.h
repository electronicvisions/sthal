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

	bool get_full_flag(::HMF::Coordinate::HICANNGlobal,
	                   ::HMF::Coordinate::HRepeaterOnHICANN) const;

	bool get_full_flag(::HMF::Coordinate::HICANNGlobal,
	                   ::HMF::Coordinate::VRepeaterOnHICANN) const;

	typedef std::array< ::HMF::HICANN::RepeaterBlock::TestEvent, 3> test_data_t;

	test_data_t get_test_events(::HMF::Coordinate::HICANNGlobal,
	                            ::HMF::Coordinate::HRepeaterOnHICANN) const;

	test_data_t get_test_events(::HMF::Coordinate::HICANNGlobal,
	                            ::HMF::Coordinate::VRepeaterOnHICANN) const;

	std::vector< ::HMF::Coordinate::HRepeaterOnHICANN> get_active_hrepeater(
	    ::HMF::Coordinate::HICANNGlobal) const;
	std::vector< ::HMF::Coordinate::VRepeaterOnHICANN> get_active_vrepeater(
	    ::HMF::Coordinate::HICANNGlobal) const;

	p_s_s_t analyze_all(std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	                    std::vector<size_t> expected_periods) const;

	bool analyze(bool full_flag, test_data_t received_test_data,
	             std::vector< ::HMF::HICANN::L1Address> expected_addrs,
	             std::vector<size_t> expected_periods) const;

	void add_passive_hrepeater(::HMF::Coordinate::HICANNGlobal hicann,
	                           ::HMF::Coordinate::HRepeaterOnHICANN hr);

	void add_passive_vrepeater(::HMF::Coordinate::HICANNGlobal hicann,
	                           ::HMF::Coordinate::VRepeaterOnHICANN vr);

	friend std::ostream& operator<<(std::ostream& out,
	                                const ReadRepeaterTestdataConfigurator& cfg);

private:

#ifndef PYPLUSPLUS
	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNGlobal,
		std::set< ::HMF::Coordinate::HRepeaterOnHICANN>,
		std::hash< ::HMF::Coordinate::HICANNGlobal> >
		passive_hrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNGlobal,
		std::set< ::HMF::Coordinate::VRepeaterOnHICANN>,
		std::hash< ::HMF::Coordinate::HICANNGlobal> >
		passive_vrepeater_map;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNGlobal,
		std::map< ::HMF::Coordinate::HRepeaterOnHICANN,
		std::pair<bool, test_data_t> >,
		std::hash< ::HMF::Coordinate::HICANNGlobal> >
		result_hr;

	tbb::concurrent_unordered_map<
		::HMF::Coordinate::HICANNGlobal,
		std::map< ::HMF::Coordinate::VRepeaterOnHICANN, std::pair<bool, test_data_t> >,
		std::hash< ::HMF::Coordinate::HICANNGlobal> >
		result_vr;
#endif

};

} // end namespace sthal
