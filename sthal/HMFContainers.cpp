#if __cplusplus >= 201103L
#include "sthal/HMFContainers.h"
#include "hal/macro_HALbe.h"

namespace HMF {
	bool DNCConfig::operator==(const DNCConfig & other) const {
		return COMPARE_EQUAL(other, gbit_reticle, loopback);
	}

	bool SpinnIFConfig::operator==(const SpinnIFConfig & other) const {
		return COMPARE_EQUAL(other, receive_port, upsample_count, downsample_count, sender_cfg, addr_cfg, routing_table);
	}
}
#endif //__cplusplus >= 201103L
