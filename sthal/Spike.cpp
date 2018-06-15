#include "sthal/Spike.h"

namespace sthal {

std::ostream& operator<<(std::ostream& out, Spike const& obj)
{
	out << "Spike(" << obj.addr << ", " << obj.time << "s)";
	return out;
}

std::ostream& operator<<(std::ostream& out, std::vector<Spike> const& obj)
{
	out << "[";
	for (auto spike : obj) {
		out << spike << ", ";
	}
	out << "]";
	return out;
}

}
