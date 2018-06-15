#define STHAL_ARRAY_OPERATOR(type, coord, ...) \
	type & operator[](const coord & ii) { __VA_ARGS__ } \
	const type & operator[](const coord & ii) const { __VA_ARGS__ }
