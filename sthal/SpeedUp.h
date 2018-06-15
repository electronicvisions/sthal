#pragma once

namespace sthal {

	/// Describes the speed up factor of the hardware, which is
	/// used to scale some currents in the floating gates
	enum SpeedUp
	{
		NORMAL = 1 << 0, //! Speed up \f$10^4\f$
		SLOW   = 1 << 1, //! Speed up \f$10^3\f$
		FAST   = 1 << 2, //! Speed up \f$10^5\f$
	};

} // end namespace sthal
