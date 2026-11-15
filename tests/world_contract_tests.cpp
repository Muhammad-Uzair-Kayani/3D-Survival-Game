#include "../modules/last_light/world_contract.h"
#include <iostream>
#include <stdexcept>
using namespace lastlight;
void require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}
struct FakeWorld final : WorldPort {
	ObservationInput state;
	Point target;
	ObservationInput observe(int) const override { return state; }
};
int main() {
	try {
		FakeWorld world;
		world.state.self.id = 1;
		world.state.position = { 4, 4 };
		world.state.destination = { 14, 24 };
		world.state.control = { 18, 12 };
		auto alone = observe(world, 1);
		require(alone.mask[2] == 0 && alone.mask[4] == 0, "solo cannot cooperate or defend without equipment/evidence");
		world.state.contacts.push_back({ 2, true, true, {} });
		require(observe(world, 1).mask[2] == 1, "visible partner enables cooperation");
		world.state.contacts[0].visible = false;
		world.state.elapsed = 30;
		require(observe(world, 1).mask[2] == 0, "stale invisible contact does not enable cooperation");
		world.state.heard_noise = true;
		auto noise = observe(world, 1);
		require(noise.mask[3] && !noise.mask[4], "anonymous gunfire enables avoidance, not an invented target");
		world.state.heard_noise = false;
		world.state.contacts[0].visible = true;
		witness(world.state.contacts[0].belief, "attack", 30);
		world.state.self.inventory.pistol = true;
		world.state.self.inventory.rounds = 1;
		require(observe(world, 1).mask[4], "witnessed aggression permits armed defense");
		for (float value : observe(world, 1).features) {
			require(std::isfinite(value) && value >= 0 && value <= 1, "feature bounds");
		}
		GateState gate;
		for (int i = 0; i < 200; ++i) {
			advance_gate(gate, 1, 1, 1.f / 60);
		}
		require(!gate.open && gate.held == 0, "same actor cannot satisfy both gate controls");
		for (int i = 0; i < 40; ++i) {
			advance_gate(gate, 1, 2, 1.f / 60);
		}
		advance_gate(gate, 1, 0, 1.f / 60);
		require(gate.held == 0, "leaving before activation resets hold");
		for (int i = 0; i < 120; ++i) {
			advance_gate(gate, 1, 2, 1.f / 60);
		}
		require(gate.open && gate.lift == 3, "distinct simultaneous actors latch and raise gate");
		require(!advance_gate(gate, 1, 2, 1.f / 60), "open event emitted once");
		require(fitness(5, 5, 60, 0) > fitness(0, 5, 60, 0), "arrival rewarded");
		require(fitness(0, 5, 60, 0) > fitness(0, 5, 120, 0), "idle time penalized");
		require(fitness(5, 5, 60, 0) > fitness(5, 5, 60, 1), "aggression never grants positive reward");
		std::cout << "Fake-world perception, action masks, gate and reward checks passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
