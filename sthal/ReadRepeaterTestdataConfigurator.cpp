#include <algorithm>
#include <set>
#include <vector>

#include "ReadRepeaterTestdataConfigurator.h"
#include "hal/Coordinate/iter_all.h"
#include "hal/HICANNContainer.h"
#include "hal/Handle/HICANN.h"
#include "hal/backend/HICANNBackend.h"
#include "sthal/HICANN.h"

using namespace ::HMF::Coordinate;
using namespace ::HMF::HICANN;


geometry::SideHorizontal get_forwarding_direction(const HorizontalRepeater& hr) {
	if (hr.getMode() != Repeater::FORWARDING) {
		throw std::runtime_error("repeater not in forwarding mode");
	}

	const bool to_left = hr.getLeft();
	const bool to_right = hr.getRight();

	if ((!to_left && !to_right) || (to_left && to_right)) {
		throw std::runtime_error(
		    "repeater not repeating left or right or repeating in both directions");
	}

	return to_left ? left : right;
}

geometry::SideVertical get_forwarding_direction(const VerticalRepeater& vr) {
	if (vr.getMode() != Repeater::FORWARDING) {
		throw std::runtime_error("repeater not in forwarding mode");
	}

	const bool to_top = vr.getTop();
	const bool to_bottom = vr.getBottom();

	if ((!to_top && !to_bottom) || (to_top && to_bottom)) {
		throw std::runtime_error(
		    "repeater not repeating top or bottom or repeating in both directions");
	}

	return to_top ? top : bottom;
}

namespace sthal {

ReadRepeaterTestdataConfigurator::ReadRepeaterTestdataConfigurator() {}

log4cxx::LoggerPtr ReadRepeaterTestdataConfigurator::getLogger() {
	static log4cxx::LoggerPtr _logger =
	    log4cxx::Logger::getLogger("sthal.ReadRepeaterTestdataConfigurator");
	return _logger;
}

bool ReadRepeaterTestdataConfigurator::get_full_flag(
    ::HMF::Coordinate::HRepeaterOnWafer c_hr) const {
	const bool full_flag = this->result_hr.at(c_hr.toHICANNOnWafer()).at(c_hr).first;
	LOG4CXX_TRACE(getLogger(), "get_full_flag: " << c_hr << " full flag: " << full_flag);

	return full_flag;
}

bool ReadRepeaterTestdataConfigurator::get_full_flag(
    ::HMF::Coordinate::VRepeaterOnWafer c_vr) const {
	const bool full_flag = this->result_vr.at(c_vr.toHICANNOnWafer()).at(c_vr).first;
	LOG4CXX_TRACE(getLogger(), "get_full_flag: " << c_vr << " full flag: " << full_flag);

	return full_flag;
}

std::array< ::HMF::HICANN::RepeaterBlock::TestEvent, 3>
ReadRepeaterTestdataConfigurator::get_test_events(
    ::HMF::Coordinate::HRepeaterOnWafer c_hr) const {
	return this->result_hr.at(c_hr.toHICANNOnWafer()).at(c_hr).second;
}

std::array< ::HMF::HICANN::RepeaterBlock::TestEvent, 3>
ReadRepeaterTestdataConfigurator::get_test_events(
    ::HMF::Coordinate::VRepeaterOnWafer c_vr) const {
	return this->result_vr.at(c_vr.toHICANNOnWafer()).at(c_vr).second;
}

std::vector< ::HMF::Coordinate::HRepeaterOnHICANN>
ReadRepeaterTestdataConfigurator::get_active_hrepeater(
    ::HMF::Coordinate::HICANNOnWafer hicann) const {
	std::vector< ::HMF::Coordinate::HRepeaterOnHICANN> active_hrepeater;

	for (auto kv : this->result_hr.at(hicann)) {
		active_hrepeater.push_back(kv.first);
	}

	return active_hrepeater;
}

std::vector< ::HMF::Coordinate::VRepeaterOnHICANN>
ReadRepeaterTestdataConfigurator::get_active_vrepeater(
    ::HMF::Coordinate::HICANNOnWafer hicann) const {
	std::vector< ::HMF::Coordinate::VRepeaterOnHICANN> active_vrepeater;

	for (auto kv : this->result_vr.at(hicann)) {
		active_vrepeater.push_back(kv.first);
	}

	return active_vrepeater;
}

p_s_s_t ReadRepeaterTestdataConfigurator::analyze_all(
    std::vector< ::HMF::HICANN::L1Address> expected_addrs,
    std::vector<size_t> expected_periods) const {
	size_t n_good = 0;
	size_t n_total = 0;

	for (auto h_kv : this->result_hr) {
		for (auto r_kv : h_kv.second) {
			auto result = r_kv.second;
			const bool good = this->analyze(result.first, result.second, expected_addrs,
			                                expected_periods);
			if (good) {
				n_good += 1;
				LOG4CXX_TRACE(getLogger(), "analyze_all: " << h_kv.first << " "
				                                           << r_kv.first << " is good");
			} else {
				LOG4CXX_WARN(getLogger(), "analyze_all: " << h_kv.first << " "
				                                          << r_kv.first << " is bad");
			}
		}
		n_total += h_kv.second.size();
	}

	for (auto h_kv : this->result_vr) {
		for (auto r_kv : h_kv.second) {
			auto result = r_kv.second;
			const bool good = this->analyze(result.first, result.second, expected_addrs,
			                                expected_periods);
			if (good) {
				n_good += 1;
				LOG4CXX_TRACE(getLogger(), "analyze_all: " << h_kv.first << " "
				                                           << r_kv.first << " is good");
			} else {
				LOG4CXX_WARN(getLogger(), "analyze_all: " << h_kv.first << " "
				                                          << r_kv.first << " is bad");
			}
		}
		n_total += h_kv.second.size();
	}

	LOG4CXX_INFO(getLogger(), "analyze_all: " << n_good << " of " << n_total
	                                          << " repeaters are good");

	return std::make_pair(n_good, n_total);
}

bool ReadRepeaterTestdataConfigurator::analyze(
    bool full_flag, ReadRepeaterTestdataConfigurator::test_data_t received_test_data,
    std::vector<L1Address> expected_addrs, std::vector<size_t> expected_periods) const {
	// pair of address and time for valid events
	std::vector<std::pair<size_t, L1Address> > valid_entries;

	for (auto entry : received_test_data) {
		// TODO: should time 0 be allowed?
		if (entry.time != 0 and full_flag) {
			valid_entries.push_back(std::make_pair(entry.time, entry.address));
		}
	}

	std::vector<size_t> tdiffs;
	std::vector<L1Address> addrs;

	for (auto e : valid_entries) {
		addrs.push_back(e.second);
	}

	if (!valid_entries.empty()) {
		for (size_t i = 0; i != valid_entries.size() - 1; ++i) {
			auto tdiff = valid_entries[i + 1].first - valid_entries[i].first;
			tdiffs.push_back(tdiff);
			LOG4CXX_TRACE(getLogger(), "diff: " << tdiff);
		}
	}

	std::set<size_t> tdiffs_set(tdiffs.begin(), tdiffs.end());
	std::set<L1Address> addrs_set(addrs.begin(), addrs.end());

	std::set<size_t> expected_tdiffs_set;
	for (auto ep : expected_periods) {
		for (short margin : {-2, -1, 0, 1, 2}) {
			expected_tdiffs_set.insert(ep + margin);
			LOG4CXX_TRACE(getLogger(), "expected diff: " << ep + margin);
		}
	}

	std::set<size_t> diff;

	std::set_difference(tdiffs_set.begin(), tdiffs_set.end(), expected_tdiffs_set.begin(),
	                    expected_tdiffs_set.end(), std::inserter(diff, diff.begin()));

	for (auto diff_result : diff) {
		LOG4CXX_TRACE(getLogger(), "diff result: " << diff_result);
	}

	std::set<L1Address> expected_addrs_set(expected_addrs.begin(), expected_addrs.end());

	const bool good_addrs = (addrs_set == expected_addrs_set);
	const bool good_diffs = diff.empty();

	LOG4CXX_TRACE(getLogger(), "addrs good: " << good_addrs
	                                          << ", diffs good: " << good_diffs);

	const bool good = good_addrs && good_diffs;

	return good;
}

void ReadRepeaterTestdataConfigurator::add_passive_hrepeater(
    ::HMF::Coordinate::HRepeaterOnWafer hr) {
	passive_hrepeater_map[hr.toHICANNOnWafer()].insert(hr);
}

void ReadRepeaterTestdataConfigurator::add_passive_vrepeater(
    ::HMF::Coordinate::VRepeaterOnWafer vr) {
	passive_vrepeater_map[vr.toHICANNOnWafer()].insert(vr);
}

void ReadRepeaterTestdataConfigurator::add_ignore_hrepeater(::HMF::Coordinate::HRepeaterOnWafer hr) {
	ignore_hrepeater_map[hr.toHICANNOnWafer()].insert(hr);
}

void ReadRepeaterTestdataConfigurator::add_ignore_vrepeater(::HMF::Coordinate::VRepeaterOnWafer vr) {
	ignore_vrepeater_map[vr.toHICANNOnWafer()].insert(vr);
}

void ReadRepeaterTestdataConfigurator::clear_ignore_hrepeater() {
	ignore_hrepeater_map.clear();
}

void ReadRepeaterTestdataConfigurator::clear_ignore_vrepeater() {
	ignore_vrepeater_map.clear();
}

void ReadRepeaterTestdataConfigurator::config(const fpga_handle_t&,
                                              const hicann_handle_t& h,
                                              const hicann_data_t& hicann) {
	LOG4CXX_TRACE(getLogger(), "ReadRepeaterTestdataConfigurator::config "
	                               << h->coordinate());

	auto const hicann_on_wafer = h->coordinate().toHICANNOnWafer();
	L1Repeaters repeaters = hicann->repeater;

	std::vector<HRepeaterOnHICANN> active_hrepeater;
	std::vector<VRepeaterOnHICANN> active_vrepeater;

	for (auto addr : iter_all<HRepeaterOnHICANN>()) {
		if (!addr.isSending() && repeaters[addr].getMode() == Repeater::FORWARDING) {
			active_hrepeater.push_back(addr);
		}
	}

	for (auto addr : iter_all<VRepeaterOnHICANN>()) {
		if (repeaters[addr].getMode() == Repeater::FORWARDING) {
			active_vrepeater.push_back(addr);
		}
	}

	auto passive_hrepeater = passive_hrepeater_map[hicann_on_wafer];
	auto passive_vrepeater = passive_vrepeater_map[hicann_on_wafer];

	std::vector<HRepeaterOnHICANN> recorded_hrepeater;
	std::vector<VRepeaterOnHICANN> recorded_vrepeater;

	size_t n = 0;

	std::map<RepeaterBlockOnHICANN, std::vector<bool> > testport_active;

	auto testable_hrepeater(active_hrepeater);
	testable_hrepeater.insert(testable_hrepeater.begin(), passive_hrepeater.begin(),
	                          passive_hrepeater.end());

	auto testable_vrepeater(active_vrepeater);
	testable_vrepeater.insert(testable_vrepeater.begin(), passive_vrepeater.begin(),
	                          passive_vrepeater.end());

	for (auto t_c_hr : testable_hrepeater) {
		LOG4CXX_TRACE(getLogger(), "testable: " << t_c_hr);
	}

	for (auto t_c_vr : testable_vrepeater) {
		LOG4CXX_TRACE(getLogger(), "testable: " << t_c_vr);
	}

	while (true) {
		LOG4CXX_TRACE(getLogger(), "reading for the " << n << " time");

		std::vector<HRepeaterOnHICANN> to_be_read_hrepeater;
		std::vector<VRepeaterOnHICANN> to_be_read_vrepeater;

		for (auto c_rb : iter_all<RepeaterBlockOnHICANN>()) {
			testport_active[c_rb] = {false, false};
		}

		auto add_testable = [&hicann_on_wafer, &testport_active](
		                        auto& testable, auto& to_be_read_repeater, auto& recorded_repeater,
		                        auto& ignore_repeater_map) {
			for (auto c_r : testable) {
				if (ignore_repeater_map.count(hicann_on_wafer) &&
				    std::find(
				        ignore_repeater_map[hicann_on_wafer].begin(),
				        ignore_repeater_map[hicann_on_wafer].end(),
				        c_r) != ignore_repeater_map[hicann_on_wafer].end()) {
					continue;
				}

				auto const c_rb = c_r.toRepeaterBlockOnHICANN();
				auto const tp = c_r.toTestPortOnRepeaterBlock().value();
				if (std::find(recorded_repeater.begin(), recorded_repeater.end(), c_r) ==
				        recorded_repeater.end() &&
				    !testport_active[c_rb][tp]) {
					to_be_read_repeater.push_back(c_r);
					testport_active[c_rb][tp] = true;
				}
			}
		};

		add_testable(testable_hrepeater, to_be_read_hrepeater, recorded_hrepeater, ignore_hrepeater_map);
		add_testable(testable_vrepeater, to_be_read_vrepeater, recorded_vrepeater, ignore_vrepeater_map);

		std::map<std::pair<RepeaterBlockOnHICANN, size_t>, HRepeaterOnHICANN>
		    rb_tp_to_c_hr;
		std::map<std::pair<RepeaterBlockOnHICANN, size_t>, VRepeaterOnHICANN>
		    rb_tp_to_c_vr;

		auto set_to_be_read = [&h, &repeaters](
		                          auto& rb_tp_to_c_r, auto& to_be_read_repeater,
		                          auto& original_directions, auto& passive_recorded_repeater,
		                          auto& recorded_repeater) {
			for (auto c_r : to_be_read_repeater) {
				LOG4CXX_TRACE(getLogger(), "reading " << c_r);

				auto c_rb = c_r.toRepeaterBlockOnHICANN();
				auto tp = c_r.toTestPortOnRepeaterBlock().value();
				rb_tp_to_c_r[std::make_pair(c_rb, tp)] = c_r;

				auto& r = repeaters[c_r];
				if (r.getMode() == Repeater::FORWARDING) {
					auto direction = get_forwarding_direction(r);
					original_directions[c_r] = direction;
					// set to input mode and program
					r.setInput(direction);
				}

				if (r.getMode() == Repeater::IDLE) {
					r.setInput();
					passive_recorded_repeater.push_back(c_r);
				}

				set_repeater(*h, c_r, r);
				recorded_repeater.push_back(c_r);
			}
		};

		if (!to_be_read_hrepeater.empty() || !to_be_read_vrepeater.empty()) {
			// original forwarding directions
			std::map<HRepeaterOnHICANN, geometry::SideHorizontal> directions_hr;
			std::map<VRepeaterOnHICANN, geometry::SideVertical> directions_vr;

			std::vector<HRepeaterOnHICANN> passive_recorded_hr;
			std::vector<VRepeaterOnHICANN> passive_recorded_vr;

			set_to_be_read(
			    rb_tp_to_c_hr, to_be_read_hrepeater, directions_hr, passive_recorded_hr,
			    recorded_hrepeater);

			set_to_be_read(
			    rb_tp_to_c_vr, to_be_read_vrepeater, directions_vr, passive_recorded_vr,
			    recorded_vrepeater);

			std::set<RepeaterBlockOnHICANN> c_rbs;
			for (auto c_hr : to_be_read_hrepeater) {
				c_rbs.insert(c_hr.toRepeaterBlockOnHICANN());
			}
			for (auto c_vr : to_be_read_vrepeater) {
				c_rbs.insert(c_vr.toRepeaterBlockOnHICANN());
			}

			for (auto c_rb : c_rbs) {
				if (std::none_of(testport_active[c_rb].begin(),
				                 testport_active[c_rb].end(), [](bool i) { return i; })) {
					continue;
				}

				LOG4CXX_TRACE(getLogger(), "reading " << c_rb);

				auto rb = repeaters.getRepeaterBlock(c_rb);

				// reset the full flag
				for (auto testport : {0, 1}) {
					rb.full_flag[testport] = false;
				}
				set_repeater_block(*h, c_rb, rb);
				flush(*h);

				// start recording test data
				for (auto testport : {0, 1}) {
					rb.start_tdi[testport] = testport_active[c_rb][testport];
				}
				set_repeater_block(*h, c_rb, rb);
				flush(*h);

				// readout received events
				rb = get_repeater_block(*h, c_rb);

				for (auto testport : {0, 1}) {
					if (!testport_active[c_rb][testport]) {
						continue;
					}

					LOG4CXX_TRACE(getLogger(), "filling result for " << c_rb << " "
					                                                 << testport);


					auto received_test_data = rb.tdi_data[testport];
					auto full_flag = rb.full_flag[testport];

					if (rb_tp_to_c_hr.find(std::make_pair(c_rb, testport)) !=
					    rb_tp_to_c_hr.end()) {
						auto c_hr = rb_tp_to_c_hr[std::make_pair(c_rb, testport)];
						this->result_hr[hicann_on_wafer][HRepeaterOnWafer(c_hr, hicann_on_wafer)] =
						    std::make_pair(full_flag, received_test_data);
						LOG4CXX_TRACE(
						    getLogger(),
						    hicann_on_wafer
						        << " " << c_hr << " "
						        << this->result_hr[hicann_on_wafer][HRepeaterOnWafer(c_hr, hicann_on_wafer)].first);
					}
					if (rb_tp_to_c_vr.find(std::make_pair(c_rb, testport)) !=
					    rb_tp_to_c_vr.end()) {
						auto c_vr = rb_tp_to_c_vr[std::make_pair(c_rb, testport)];
						this->result_vr[hicann_on_wafer][VRepeaterOnWafer(c_vr, hicann_on_wafer)] =
						    std::make_pair(full_flag, received_test_data);
						LOG4CXX_TRACE(
						    getLogger(),
						    hicann_on_wafer
						        << " " << c_vr << " "
						        << this->result_vr[hicann_on_wafer][VRepeaterOnWafer(c_vr, hicann_on_wafer)].first);
					}
				}

				// reset the full flag again
				for (auto testport : {0, 1}) {
					rb.full_flag[testport] = false;
				}
				set_repeater_block(*h, c_rb, rb);

				flush(*h);
			}

			for (auto c_hr : passive_recorded_hr) {
				auto& r = repeaters[c_hr];
				r.setIdle();
				set_repeater(*h, c_hr, r);
			}

			for (auto c_vr : passive_recorded_vr) {
				auto& r = repeaters[c_vr];
				r.setIdle();
				set_repeater(*h, c_vr, r);
			}

			for (auto kv : directions_hr) {
				auto& r = repeaters[kv.first];
				r.setForwarding(kv.second);
				set_repeater(*h, kv.first, r);
			}

			for (auto kv : directions_vr) {
				auto& r = repeaters[kv.first];
				r.setForwarding(kv.second);
				set_repeater(*h, kv.first, r);
			}

			flush(*h);

			++n;

		} else {
			break;
		}
	}
}

std::ostream& operator<<(std::ostream& out, const ReadRepeaterTestdataConfigurator& cfg) {
	out << "ReadRepeaterTestdataConfigurator:\n";

	for (auto h_kv : cfg.result_hr) {
		for (auto r_kv : h_kv.second) {
			auto result = r_kv.second;
			out << h_kv.first << " " << r_kv.first << '\n';
			out << "\t full flag: " << result.first << '\n';
			for (auto event : result.second) {
				out << "\t\t" << event << '\n';
			}
		}
	}

	for (auto h_kv : cfg.result_vr) {
		for (auto r_kv : h_kv.second) {
			auto result = r_kv.second;
			out << h_kv.first << " " << r_kv.first << " / "
			    << r_kv.first.toVLineOnHICANN() << '\n';
			out << "\t full flag: " << result.first << '\n';
			for (auto event : result.second) {
				out << "\t\t" << event << '\n';
			}
		}
	}

	return out;
}

} // end namespace sthal
