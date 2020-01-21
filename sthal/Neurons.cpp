#include "sthal/Neurons.h"
#include "hal/macro_HALbe.h"

#include "halco/hicann/v2/fwd.h"
#include "halco/common/iter_all.h"

namespace sthal {

Neurons::Neurons():config(),mQuads(){}

bool Neurons::check_analog_io() {
	throw std::runtime_error("Not implemented");
}

bool Neurons::check_current_io() {
	throw std::runtime_error("Not implemented");
}

bool Neurons::check_enable_spl1_output(std::ostream & errors) const
{
	bool ok = true;
	using namespace halco::common;
	using namespace halco::hicann::v2;
	for (NeuronOnHICANN neuron : iter_all<NeuronOnHICANN>())
	{
		if ((*this)[neuron].enable_spl1_output())
			if ((*this)[neuron].activate_firing()==false) {
				ok=false;
				errors << "Warning: Meaningless configuration: enable_spl1_output is set to true, but activate_firing is set to false for " << neuron << "\n";
			}
	}
	return ok;
}

bool operator==(const Neurons & a, const Neurons & b)
{
	return
	       a.config == b.config
		&& a.mQuads == b.mQuads
	;
}

bool operator!=(const Neurons & a, const Neurons & b)
{
	return !(a == b);
}

bool Neurons::check(std::ostream & errors) const
{
	bool ok = true;
	ok &= check_denmem_connectivity(errors);
	ok &= check_enable_spl1_output(errors);
	return ok;
}


namespace
{
	using namespace halco::hicann::v2;
	using namespace halco::common;
	class CheckDenmemConnectivtyHelper
	{
	public:
		CheckDenmemConnectivtyHelper(const Neurons & _neurons,
				std::ostream & _errors) :
			neurons(_neurons),
			errors(_errors),
			ok(true)
		{
			std::fill(visited.begin(), visited.end(), false);
			std::fill(components.begin(), components.end(), invalid);
		}

		bool check()
		{
			// Depth-First-Search to find connected components
			size_t current_component = 0;
			for (NeuronOnHICANN neuron : iter_all<NeuronOnHICANN>())
			{
				if (!visited[neuron.toEnum()])
				{
					find_cc(neuron, current_component);
					++current_component;
				}
			}

			if (std::find(components.begin(), components.end(), invalid)
					!= components.end())
				internalError();

			bool ok = true;
			ok &= check_enable_fire_input();
			ok &= check_activate_firing();
			return ok;
		}

	private:
		bool check_enable_fire_input()
		{
			bool ok = true;
			// Check that the connect of the neighbouring Quads is set
			// correctly
			auto it_gen = iter_all<QuadOnHICANN>();
			const auto iend = --(it_gen.end());
			for (auto it = it_gen.begin(); it != iend; ++it)
			{
				auto neighbour = it;
				++neighbour;
				QuadOnHICANN left = *it;
				QuadOnHICANN right = *neighbour;
				NeuronOnHICANN lt = NeuronOnHICANN(left, NeuronOnQuad(X(1), Y(0)));
				NeuronOnHICANN lb = NeuronOnHICANN(left, NeuronOnQuad(X(1), Y(1)));
				NeuronOnHICANN rt = NeuronOnHICANN(right, NeuronOnQuad(X(0), Y(0)));
				NeuronOnHICANN rb = NeuronOnHICANN(right, NeuronOnQuad(X(0), Y(1)));

				size_t cnt = 0;
				if (neurons[lt].enable_fire_input())
					++cnt;
				if (neurons[lb].enable_fire_input())
					++cnt;
				if (neurons[rt].enable_fire_input())
					++cnt;
				if (neurons[rb].enable_fire_input())
					++cnt;

				if (cnt > 1)
				{
					ok = false;
					errors << "Only one neuron on the boundary between two quads "
					           "should have enable_fire_input set:\n"
					       << lt << ", " << lb << ", " << rt << ", " << rb << "\n";
			    }
			}
			return ok;
		}

		bool check_activate_firing()
		{
			bool ok = true;
			std::array<size_t, n> count;
			std::fill(count.begin(), count.end(), 0u);
			for (size_t ii = 0; ii < n; ++ii)
			{
				if (neurons[NeuronOnHICANN{Enum{ii}}].activate_firing())
				{
					++(count[components[ii]]);
				}
			}
			for (size_t ii = 0; ii < n; ++ii)
			{
				if (count[ii] > 1)
				{
					ok = false;
					errors << "activate_firing is activated more than once in:";
					for (size_t jj = 0; jj < n; ++jj)
					{
						if (components[jj] == ii)
						{
							errors << " " << NeuronOnHICANN{Enum{jj}};
						}
					}
					errors << "\n";
				}
			}
			return ok;
		}


		void find_cc(NeuronOnHICANN start, size_t current_component)
		{
			std::vector<size_t> stack;
			stack.push_back(start.toEnum());
			while (!stack.empty())
			{
				NeuronOnHICANN n{Enum{stack.back()}};
				stack.pop_back();

				if (visited[n.toEnum()])
					continue;

				visited[n.toEnum()] = true;
				components[n.toEnum()] = current_component;
				// Visit Neighbours: left, right, top/bottom
				if (n.x() != NeuronOnHICANN::x_type::min)
				{
					X x{n.x() - 1u};
					NeuronOnHICANN next(x, n.y());
					if (are_connected(n, next))
						stack.push_back(next.toEnum());
				}
				if (n.x() != NeuronOnHICANN::x_type::max)
				{
					X x{n.x() + 1u};
					NeuronOnHICANN next(x, n.y());
					if (are_connected(n, next))
						stack.push_back(next.toEnum());
				}
				{
					NeuronOnHICANN next(n.x(), (n.y() == Y(0) ? Y(1) : Y(0)));
					auto quad = neurons[n.toQuadOnHICANN()];
					if (quad.getVerticalInterconnect(n.toNeuronOnQuad().x()))
						stack.push_back(next.toEnum());
				}
			}
		}

		// checks if two neurons left or right of each other are conneted
		bool are_connected(NeuronOnHICANN a, NeuronOnHICANN b)
		{
			if ( a == b || a.x() == b.x() || a.y() != b.y())
				internalError();

			if (a.toQuadOnHICANN() == b.toQuadOnHICANN())
			{
				auto quad = neurons[a.toQuadOnHICANN()];
				return quad.getHorizontalInterconnect(a.y());
			}
			else
			{
				return neurons[a].enable_fire_input() ||
					neurons[b].enable_fire_input();
			}
		}

		void internalError() const
		{
			throw std::runtime_error("Internal Error in CheckDenmemConnectivtyHelper");
		}

		static const size_t n = NeuronOnHICANN::enum_type::size;
		static const size_t invalid = n;

		const Neurons & neurons;
		std::ostream & errors;

		bool ok;

		std::array<bool, n> visited;
		std::array<size_t, n> components;
		std::array<size_t, n> previous;
		std::vector<size_t> stack;
	};

	const size_t CheckDenmemConnectivtyHelper::n;
	const size_t CheckDenmemConnectivtyHelper::invalid;
}


// We do a serach through the denmens:
// * collect all denmens that activate firing
// * search from these denmes though als connected denmens and
// ** check for cyclic connections
// ** check for multiple firing neuron
// Circles in Neuron Quads are allowed, otherwise not
bool Neurons::check_denmem_connectivity(std::ostream & errors) const
{
	CheckDenmemConnectivtyHelper helper(*this, errors);
	return helper.check();
}

std::ostream& operator<<(std::ostream& out, Neurons const& obj)
{
	out << "StHAL::Neurons:" << std::endl;
	out << "- Quads:" << std::endl;

	auto prev = *std::begin(obj.mQuads);
	auto count = 0;
	for (auto it : obj.mQuads) {
		if (it == prev) {
			count++;
		} else {
			out << count << "x ";
			out << prev << std::endl;
			prev = it;
			count = 1;
		}
	}
	out << count << "x ";
	out << prev << std::endl;
	out << "- Config: " << obj.config;
	return out;
}

}
