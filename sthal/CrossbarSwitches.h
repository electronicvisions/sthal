#pragma once

#include "hal/HICANN/Crossbar.h"

namespace sthal {

class CrossbarSwitches : public ::HMF::HICANN::Crossbar {

public:
	bool check_exclusiveness(size_t max_switches_per_row, size_t max_switches_per_column,
	                         std::ostream& errors) const;
	bool check(size_t max_switches_per_row, size_t max_switches_per_column,
	           std::ostream& errors) const;
};
}
