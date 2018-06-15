#include "sthal/SynapseSwitches.h"

namespace sthal {

bool SynapseSwitches::check_exclusiveness(size_t max_switches_per_row,
                                          size_t max_switches_per_column_per_side,


                                          std::ostream& errors) const {
	auto const check = ::HMF::HICANN::SynapseSwitch::check_exclusiveness(
	    max_switches_per_row, max_switches_per_column_per_side);
	errors << check;
	return check.empty();
}

bool SynapseSwitches::check(size_t max_switches_per_row,
                            size_t max_switches_per_column_per_side,
                            std::ostream& errors) const {
	bool ok = true;
	ok &= check_exclusiveness(max_switches_per_row, max_switches_per_column_per_side,
	                          errors);
	return ok;
}
}
