#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <fstream>

#include "test/hwtest.h"

#include "hal/Coordinate/iter_all.h"

#include "sthal/Wafer.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/MagicHardwareDatabase.h"

#include "SthalHWTest.h"

using namespace HMF::Coordinate;

// Same test as in HALBE but via sthal
// denmem 0 stimulated with a current stimulus and readout with FPGA
TEST_F(SthalHWTest, NeuronCurrentStimHWTest) {
	NeuronOnHICANN neuron_c{X{0},Y{0}};
	AnalogOnHICANN analog_c{0};

	HMF::HICANN::L1Address neuron_addr{42};

	hicann.use_big_capacitors(true);

	hicann.enable_l1_output(neuron_c, neuron_addr);
	hicann.enable_aout(neuron_c, analog_c);

	ASSERT_EQ(hicann.neurons[neuron_c].address(), neuron_addr);

	for (auto merger : iter_all<DNCMergerOnHICANN>() )
	{
		auto & m = hicann.layer1[merger];
		m.config = HMF::HICANN::Merger::RIGHT_ONLY;
		m.slow =  false;
		m.loopback = false;
	}

	// DNC and dnc_if:
	hicann.layer1[GbitLinkOnHICANN(0)] = HMF::HICANN::GbitLink::Direction::TO_DNC;

	// CURRENT STIMULUS
	sthal::FGStimulus stimulus;
	stimulus.setPulselength(15);
	stimulus.setContinuous(true);
	for ( size_t ndv = 0; ndv < 80; ++ndv )
		stimulus[ndv] = 800;
	hicann.setCurrentStimulus(neuron_c, stimulus);

	// Configure HICANN via HICANNConfigurator
	sthal::HICANNConfigurator cfg;
	sthal::ExperimentRunner runner{0.01};
	wafer.connect(sthal::MagicHardwareDatabase());
	wafer.configure(cfg);
	wafer.start(runner);


	{
		auto recorder = hicann.analogRecorder(analog_c);
		recorder.setRecordingTime(10.0e-3);
		auto dt = recorder.getTimestamp();
		size_t ii = 0;
		std::ofstream out("NeuronCurrentStimHWTest_membrane.txt");
		recorder.record();
		for (float point : recorder.trace() )
			out << (ii++ * dt) << '\t' << point << '\n';
	}


	auto spikes = hicann.receivedSpikes(GbitLinkOnHICANN(0));
	std::cout << "Received " << spikes.size() << " pulses" << std::endl;
	EXPECT_TRUE(spikes.size() > 0);
	for (auto spike : spikes)
	{
		EXPECT_EQ( neuron_addr, spike.addr );
	}

	for (auto dnc_merger : iter_all<GbitLinkOnHICANN>())
	{
		if (dnc_merger.value() > 0)
		{
			auto spikes = hicann.receivedSpikes(dnc_merger);
			EXPECT_EQ(spikes.size(), 0);
		}
	}
}
