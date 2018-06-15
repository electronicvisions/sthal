#include "sthal/CrossbarSwitches.h"

namespace sthal {

bool CrossbarSwitches::check_exclusiveness(size_t const max_switches_per_row,
                                           size_t const max_switches_per_column,
                                           std::ostream& errors) const {
	auto const check = ::HMF::HICANN::Crossbar::check_exclusiveness(
	    max_switches_per_row, max_switches_per_column);
	errors << check;
	return check.empty();
}

bool CrossbarSwitches::check(size_t const max_switches_per_row,
                             size_t const max_switches_per_column,
                             std::ostream& errors) const {
	bool ok = true;
	ok &= check_exclusiveness(max_switches_per_row, max_switches_per_column, errors);
	return ok;
}
}
