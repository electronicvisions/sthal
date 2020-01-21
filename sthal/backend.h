#pragma once
#include <iostream>

//#include "backend_fwd.h"
#include "hal/HMFBackend.h"

// TODO @SJ: delete me?

template<typename T>
struct Backend
{
	template<typename Sys, typename ... Args>
	static void config(Sys const& sys, Args&& ... args)
	{
		std::cout << "configuring default backend for: [ ";
		for (auto ii : sys.index()) {
			std::cout << ii << " ";
		}
		std::cout << "]" << std::endl;
	}
};


template<>
struct Backend<HMF::Chip>
{
public:
	template<typename ... Args>
	static void config(HMF::Chip const& chip, Args&& ... args)
	{
		std::cout << "configuring HMF::Chip backend for: [ ";
		for (auto ii : chip.index()) {
			std::cout << ii << " ";
		}
		std::cout << "]" << std::endl;



		// configure HW
		using namespace halco::common;
		HMF::HICANNBackend hicann(
			Enum(chip.index()[1]),
			HMF::WaferId(chip.index()[0]));


		// Crossbar
		size_t cnt = 0;
		for(auto& row : chip.crossbar.switches()) {
			hicann.set_crossbar_switch_row(halco::hicann::v2::HLineOnHICANN(cnt), LEFT,
				reinterpret_cast<std::array<bool, 4> const&>(row));
			hicann.set_crossbar_switch_row(halco::hicann::v2::HLineOnHICANN(cnt), RIGHT,
				reinterpret_cast<std::array<bool, 4> const&>(row));

			++cnt;
		}

		// synapse driver switches
		cnt = 0;
		for(auto& row : chip.synapse_switch.switches()) {
			hicann.set_syndriver_switch_row(halco::hicann::v2::HLineOnHICANN(cnt), LEFT,
				reinterpret_cast<std::array<bool, 4*4> const&>(row));
			hicann.set_syndriver_switch_row(halco::hicann::v2::HLineOnHICANN(cnt), RIGHT,
				reinterpret_cast<std::array<bool, 4*4> const&>(row[4*4]));

			++cnt;
		}


		// Synapses

		//void set_weights_row(HWire y, Side s, bool line, std::array<uint8_t, 256> const&);
		//void set_decoder_double_row(HWire y, Side s, std::array<std::array<uint8_t, 256>, 2> const&);


		// HICANN.
		//void set_synapsedriver(HWire y, Side s, HMF::HICANN.<256> const& drv_row);


		// DenMems
		//void set_denmem_quad_block(OutputBufferAddress oba, size_t qb [> 0-7 <], std::array<std::array<HMF::Neuron, 32>, 2> const& neurons);
		//void set_phase(uint8_t phase = 0x00);


		// Floating Gates
		//void set_fg_values(SideVertical y, HMF::FGBlockLeft const& fg);
		//void set_fg_values(SideVertical y, HMF::FGBlockRight const& fg);



		// Repeater
		//void set_repeater(HWire y, HMF::Repeater const& rc);
		//void set_repeater(VWire x, HMF::Repeater const& rc);

		//void set_vertical_repeater_block(Side s, bool [> top/bottom <], HMF::RepeaterBlock const& rbc);
		//void set_horizontal_repeater_block(Side s, HMF::RepeaterBlock const& rbc);



		// Merger Tree
		//void set_merger_tree(HMF::MergerTree const& m);
		//void set_dnc_merger(HMF::DNCMerger const& m);



		// Backgroud Generators
		//void set_background_generator(std::array<HMF::HICANN., 8> const& bg);


		// Analog Outputs (Alex loves this)
		//void set_analog(typename HMF::AnalogControl::Analog0 const& a);
		//void set_analog(typename HMF::AnalogControl::Analog1 const& a);
	}
};
