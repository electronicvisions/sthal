#pragma once

#include <sthal/Spike.h>
#include <pywrap/from_numpy.hpp>
#include <pywrap/return_numpy_policy.hpp>

namespace pywrap {

template <>
struct convert_to_numpy< std::vector<sthal::Spike> >
{
	typedef pyublas::numpy_matrix<double> numpy_type;

	inline PyObject * operator()(const std::vector<sthal::Spike> & v) const
	{
		pyublas::numpy_matrix<double> result(v.size(), 2);
		for (size_t ii = 0; ii < v.size(); ++ii)
		{
			result(ii, 0) = v[ii].time;
			result(ii, 1) = v[ii].addr;
		}
		boost::python::handle<> result_handle = result.to_python();
		return result_handle.release();
	}

	PyTypeObject const *get_pytype() const {
		return boost::python::converter::expected_pytype_for_arg<numpy_type>::get_pytype();
	}
};

} // end namespace pywrap
