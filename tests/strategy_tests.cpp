#include "../modules/last_light/strategy.h"
#include <iostream>
#include <stdexcept>
using namespace lastlight;
void require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}
int main() {
	try {
		StrategicInput in;
		in.nodes = { { { 0, 0 }, "start" }, { { 10, 0 }, "exposed" }, { { 10, 10 }, "forest" }, { { 20, 0 }, "exit" } };
		in.edges = { { 0, 1, .1f, .1f }, { 1, 3, .1f, .1f }, { 0, 2, .02f, 0 }, { 2, 3, .02f, 0 } };
		in.actions = { { "extract", "Extract", 3, 0, 5, 0, Escaped, 0, 1, 0, 1 } };
		in.initial = HaveSignal;
		in.personality.caution = .05f;
		auto fast = make_strategy(in);
		require(!fast.alternatives.empty(), "a reachable goal has a plan");
		require(fast.alternatives[0].cost > 6, "travel time is a real cost");
		require(fast.alternatives[0].steps[0].node == 1, "healthy risk-tolerant survivor takes the shorter approach");
		in.health = 25;
		in.personality.caution = .95f;
		auto cautious = make_strategy(in);
		require(!cautious.alternatives.empty() && cautious.alternatives[0].steps[0].node == 2, "injured cautious survivor chooses the forest route");
		in.current_risk[2] = 5;
		auto blocked = make_strategy(in);
		require(blocked.alternatives[0].steps[0].node == 1, "new threat information changes a previously preferred strategy");
		StrategicInput mission;
		mission.nodes = { { { 0, 0 }, "start" }, { { 6, 0 }, "key" }, { { 12, 0 }, "generator" }, { { 18, 0 }, "radio" }, { { 24, 0 }, "exit" } };
		mission.edges = { { 0, 1, 0, 0 }, { 1, 2, 0, 0 }, { 2, 3, 0, 0 }, { 3, 4, 0, 0 } };
		mission.actions = { { "key", "Key", 1, 0, 5, 0, HaveKey, 0, 1, 0, 1 }, { "crank", "Crank", 2, 0, 5, 0, Power, 0, 12, 0, 1 }, { "radio", "Signal", 3, 0, 5, HaveKey | Power, HaveSignal, 0, 3, 0, 1 }, { "extract", "Leave", 4, 0, 5, HaveSignal, Escaped, 0, 1, 0, 1 }, { "fuel", "Impossible fuel operation", 2, 0, 5, Fuel, Power, Fuel, 1, 0, 1 } };
		auto result = make_strategy(mission);
		require(!result.alternatives.empty(), "multi-stage prerequisite plan found");
		uint32_t facts = 0;
		int meaningful = 0;
		for (auto &step : result.alternatives[0].steps) {
			if (step.kind != "travel") {
				const StrategicAction *action = nullptr;
				for (auto &candidate : mission.actions) {
					if (candidate.kind == step.kind) {
						action = &candidate;
					}
				}
				require(action && (facts & action->required) == action->required, "each action's preconditions are achieved by earlier actions");
				facts = (facts & ~action->remove) | action->add;
				++meaningful;
			}
		}
		require(facts & Escaped, "complete plan reaches extraction");
		require(meaningful == 4, "no imaginary fuel or unnecessary action");
		Personality profile = baseline_personality(2);
		float initial = profile.caution;
		experience(profile, "attacked", 3, -1, 3);
		require(profile.caution > initial && profile.region_risk[3] > .1f, "experience changes personal caution and historical region risk");
		experience(profile, "help_received", 3, 1, 2);
		require(profile.helped == 1 && profile.preferences[2] > 0, "help updates social evidence and individual preferences");
		profile = baseline_personality(2);
		require(profile.experiences == 0 && profile.region_risk[3] == 0 && profile.preferences[2] == 0, "reset restores the baseline without learned state");
		Inventory inventory;
		inventory.owned_weapons = 3;
		inventory.pistol = true;
		inventory.rounds = 3;
		inventory.reserve = 7;
		inventory.magazines[1] = 12;
		inventory.ammunition[1] = 24;
		require(switch_weapon(inventory, 1) && inventory.rounds == 12 && inventory.reserve == 24, "weapon change selects compatible magazine/ammunition");
		require(switch_weapon(inventory, 0) && inventory.rounds == 3 && inventory.reserve == 7, "switching back preserves ammo without duplication");
		std::cout << "Strategic route selection, forward prerequisites, experience reset and arsenal checks passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
	return 0;
}
