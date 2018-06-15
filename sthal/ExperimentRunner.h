#pragma once

#include <boost/shared_ptr.hpp>
#include <pywrap/compat/array.hpp>
#include "log4cxx/provisionnode.h"

#include "hal/Coordinate/External.h"

namespace HMF {
namespace Handle {
	class FPGA;
} // Handle
} // HMF

namespace sthal {

class FPGA;

typedef ::HMF::Coordinate::FPGAOnWafer fpga_coord;
typedef boost::shared_ptr< ::HMF::Handle::FPGA > fpga_handle_t;
typedef std::array< boost::shared_ptr<FPGA>, fpga_coord::size> fpga_list;
typedef std::array<fpga_handle_t, fpga_coord::size> fpga_handle_list;

class ExperimentRunner
{
public:
	ExperimentRunner(double run_time_in_s, bool drop_background_events = false);
	virtual ~ExperimentRunner();

	virtual void run(const fpga_list & fpgas, const fpga_handle_list & handles);

	virtual void sort_spikes(fpga_list const& fpgas);
	virtual void send_spikes(const fpga_list & fpgas, const fpga_handle_list & handles);
	virtual void receive_spikes(const fpga_list & fpgas, const fpga_handle_list & handles);
	virtual void start_experiment(const fpga_list & fpgas, const fpga_handle_list & handles);

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();

	double run_time_in_s() const { return m_run_time_in_us * 1e-6; }

	/// Experiment run time in microseconds
	double time() const { return m_run_time_in_us; }

	bool drop_background_events() const { return m_drop_background_events; }
	void drop_background_events(bool const value) { m_drop_background_events = value; }
private:
	double m_run_time_in_us;
	bool m_drop_background_events;
};

std::ostream& operator<<(std::ostream& out, ExperimentRunner const& obj);

} // end namespace sthal
