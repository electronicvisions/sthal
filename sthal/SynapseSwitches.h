#pragma once

#include "hal/HICANN/SynapseSwitch.h"

namespace sthal {

class SynapseSwitches : public ::HMF::HICANN::SynapseSwitch {

public:
	bool check_exclusiveness(size_t max_switches_per_row,
	                         size_t max_switches_per_column_per_side,
	                         std::ostream& errors) const;

	bool check(size_t max_switches_per_row, size_t max_switches_per_column_per_side,
	           std::ostream& errors) const;
};
}
