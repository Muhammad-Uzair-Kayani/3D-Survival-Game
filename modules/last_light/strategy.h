#pragma once
#include "world_contract.h"
#include <array>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <vector>
namespace lastlight {
struct Personality {
	float caution = .6f, aggression = .25f, generosity = .5f, loyalty = .5f, curiosity = .5f, confidence = .5f;
	std::array<float, 6> preferences{};
	std::array<float, 32> region_risk{};
	int revision = 0, experiences = 0, helped = 0, harmed = 0, rescued = 0, extractions = 0, deaths = 0;
};
inline Personality baseline_personality(int id) {
	Personality p;
	if (id == 2) {
		p.generosity = .8f;
		p.loyalty = .7f;
		p.caution = .65f;
	}
	if (id == 3) {
		p.curiosity = .85f;
		p.caution = .4f;
		p.confidence = .65f;
	}
	if (id == 4) {
		p.loyalty = .8f;
		p.aggression = .45f;
		p.confidence = .7f;
	}
	if (id == 5) {
		p.caution = .8f;
		p.generosity = .3f;
		p.curiosity = .65f;
	}
	return p;
}
inline const char *personality_type(const Personality &p) {
	if (p.caution > .8f && p.confidence < .4f) {
		return "Wary independent survivor";
	}
	if (p.generosity > .7f && p.loyalty > .55f) {
		return "Supportive protector";
	}
	if (p.caution > .75f) {
		return "Cautious survivalist";
	}
	if (p.aggression > .65f) {
		return "Confrontational defender";
	}
	if (p.curiosity > .7f) {
		return "Independent explorer";
	}
	if (p.loyalty > .7f) {
		return "Loyal team player";
	}
	return "Pragmatic survivor";
}
inline void experience(Personality &p, const std::string &kind, int region, float reward = 0, int intent = 5) {
	auto shift = [](float &v, float delta) { v = std::clamp(v + delta, 0.f, 1.f); };
	++p.experiences;
	if (kind == "help_received") {
		++p.helped;
		shift(p.loyalty, .025f);
		shift(p.generosity, .015f);
		shift(p.confidence, .01f);
	}
	if (kind == "help_given") {
		++p.rescued;
		shift(p.generosity, .015f);
		shift(p.loyalty, .01f);
	}
	if (kind == "attacked") {
		++p.harmed;
		shift(p.caution, .025f);
		shift(p.confidence, -.02f);
		if (region >= 0 && region < 32) {
			shift(p.region_risk[region], .12f);
		}
	}
	if (kind == "help_witnessed") {
		shift(p.loyalty, .005f);
	}
	if (kind == "aggression_witnessed") {
		shift(p.caution, .006f);
	}
	if (kind == "scavenged") {
		shift(p.curiosity, .003f);
		shift(p.confidence, .002f);
	}
	if (kind == "defended") {
		shift(p.aggression, .004f);
		shift(p.confidence, .005f);
	}
	if (kind == "harmful") {
		shift(p.aggression, .02f);
		shift(p.loyalty, -.015f);
	}
	if (kind == "extracted") {
		++p.extractions;
		shift(p.confidence, .03f);
	}
	if (kind == "died") {
		++p.deaths;
		shift(p.caution, .04f);
		shift(p.confidence, -.03f);
	}
	if (intent >= 0 && intent < 6 && std::isfinite(reward)) {
		p.preferences[intent] = std::clamp(p.preferences[intent] + .06f * (std::clamp(reward, -1.f, 1.f) - .1f * p.preferences[intent]), -2.f, 2.f);
	}
}
enum Fact : uint32_t { HaveKey = 1,
	Fuel = 2,
	Power = 4,
	HaveSignal = 8,
	Armed = 16,
	Medical = 32,
	Allied = 64,
	Warm = 128,
	Fed = 256,
	Hydrated = 512,
	Armoured = 1024,
	Safe = 2048,
	Escaped = 4096,
	Treated = 8192,
	RelayOne = 16384,
	RelayTwo = 32768,
	RelayThree = 65536,
	Loaded = 131072,
	Scouted = 262144,
	HaveFood = 524288,
	HaveWater = 1048576,
	HasPistol = 2097152,
	HasSMG = 4194304,
	HasRifle = 8388608,
	HasShotgun = 16777216,
	HasPack = 33554432,
	ReadyAmmo = 67108864,
	SecuredPower = 134217728,
	HasTools = 268435456 };
struct StrategyNode {
	Point position;
	std::string name;
	bool shelter = false;
	float elevation = 0;
};
struct StrategyEdge {
	int from = 0, to = 0;
	float exposure = 0, noise = 0;
	bool shortcut = false;
	float travel_distance = 0;
	bool enabled = true;
};
struct StrategicAction {
	std::string kind, label;
	int node = 0, target = 0, intent = 5;
	uint32_t required = 0, add = 0, remove = 0;
	float seconds = 1, benefit = 0, confidence = 1;
	float ammo_cost = 0, ammo_gain = 0, medical_cost = 0, medical_gain = 0, heal = 0, weight_delta = 0, capacity_gain = 0;
	Point destination;
	float elevation = 0;
	bool exact = false;
};
struct StrategicStep {
	std::string kind, label;
	Point point;
	int target = 0, node = 0, intent = 5;
	float height = 0;
};
struct StrategicPlan {
	std::vector<StrategicStep> steps;
	std::string label, reason;
	float score = -1e9f, cost = 0, risk = 0;
	int route_variant = 0, expanded = 0;
	bool complete = true;
	float expected_health = 100, expected_ammo = 0, expected_weight = 0;
};
struct StrategicInput {
	Personality personality;
	std::vector<StrategyNode> nodes;
	std::vector<StrategyEdge> edges;
	std::vector<StrategicAction> actions;
	std::array<float, 32> current_risk{};
	int start = 0, neural_intent = -1;
	uint32_t initial = 0, goal = Escaped;
	float health = 100, ammo = 0, time_left = 1500;
	bool shortcut_open = false;
	bool enforce_budget = false;
	float medical = 0, weight = 0, capacity = 14, min_health = 10;
	int depth_limit = 8, beam_width = 32, expansion_limit = 40000;
};
struct StrategicResult {
	std::vector<StrategicPlan> alternatives;
	int expanded = 0;
};
// Bounded forward search over explicit preconditions/effects, with graph-route
// variants and survival costs. The engine supplies only perceived opportunities.
inline StrategicResult make_strategy(const StrategicInput &in) {
	StrategicResult result;
	const int n = int(in.nodes.size());
	if (n == 0 || in.start < 0 || in.start >= n || n > 32 || in.actions.size() > 60) {
		return result;
	}
	struct Route {
		float cost = 1e9f, risk = 0, travel = 0, noise = 0, known_risk = 0;
		std::vector<int> path;
	};
	Route routes[32][32][3];
	for (int source = 0; source < n; ++source) {
		for (int variant = 0; variant < 3; ++variant) {
			std::array<float, 32> d;
			d.fill(1e9f);
			std::array<int, 32> parent;
			parent.fill(-1);
			std::array<float, 32> risks{};
			using Pair = std::pair<float, int>;
			std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> open;
			d[source] = 0;
			open.push({ 0, source });
			while (!open.empty()) {
				auto [cost, node] = open.top();
				open.pop();
				if (cost > d[node] + .001f) {
					continue;
				}
				for (const auto &edge : in.edges) {
					int next = edge.from == node ? edge.to : edge.to == node ? edge.from
																			 : -1;
					if (next < 0 || next >= n || !edge.enabled || (edge.shortcut && !in.shortcut_open)) {
						continue;
					}
					float danger = edge.exposure + in.current_risk[next] + in.personality.region_risk[next] * .6f;
					float risk_weight = (variant == 0 ? 8.f : variant == 1 ? 38.f
																		   : 20.f) *
							(.5f + in.personality.caution) * (1 + (100 - in.health) / 65);
					float travel = (edge.travel_distance > 0 ? edge.travel_distance : distance(in.nodes[node].position, in.nodes[next].position)) / 3;
					float step = travel + danger * risk_weight + (variant == 2 ? edge.noise * 15 : 0);
					if (cost + step < d[next] - .001f) {
						d[next] = cost + step;
						parent[next] = node;
						risks[next] = risks[node] + danger;
						open.push({ d[next], next });
					}
				}
			}
			for (int target = 0; target < n; ++target) {
				if (d[target] < 1e8f) {
					auto &route = routes[source][target][variant];
					route.cost = d[target];
					route.risk = risks[target];
					for (int at = target; at != source && at >= 0; at = parent[at]) {
						route.path.insert(route.path.begin(), at);
					}
					int before = source;
					for (int at : route.path) {
						for (const auto &edge : in.edges) {
							if ((edge.from == before && edge.to == at) || (edge.to == before && edge.from == at)) {
								route.travel += (edge.travel_distance > 0 ? edge.travel_distance : distance(in.nodes[before].position, in.nodes[at].position)) / 3;
								route.noise += edge.noise;
								break;
							}
						}
						route.known_risk += in.current_risk[at];
						before = at;
					}
				}
			}
		}
	}
	struct State {
		uint32_t facts = 0;
		uint64_t used = 0;
		int node = 0;
		float cost = 0, risk = 0, benefit = 0;
		float health = 100, ammo = 0, medical = 0, weight = 0, capacity = 14;
		std::vector<StrategicStep> steps;
		int variant = 0;
	};
	auto rank = [&](const State &state) {
		int missing = 0;
		uint32_t need = in.goal & ~state.facts;
		for (; need; need >>= 1) {
			missing += need & 1;
		}
		// Intermediate mission facts make multi-step solutions survive the beam.
		int progress = 0;
		for (uint32_t f : { uint32_t(HaveKey), uint32_t(Power), uint32_t(HaveSignal), uint32_t(Escaped), uint32_t(RelayOne), uint32_t(RelayTwo), uint32_t(RelayThree) }) {
			if (state.facts & f) {
				++progress;
			}
		}
		return state.cost - state.benefit - progress * 40 + missing * 20;
	};
	std::vector<State> beam(1);
	beam[0].facts = in.initial;
	beam[0].node = in.start;
	beam[0].health = in.health;
	beam[0].ammo = in.ammo;
	beam[0].medical = in.medical;
	beam[0].weight = in.weight;
	beam[0].capacity = in.capacity;
	State best_partial;
	float partial_rank = 1e9f;
	bool has_partial = false;
	for (int depth = 0; depth < std::clamp(in.depth_limit, 1, 16) && !beam.empty(); ++depth) {
		std::vector<State> next;
		for (const auto &state : beam) {
			for (size_t index = 0; index < in.actions.size(); ++index) {
				const auto &action = in.actions[index];
				if (action.node < 0 || action.node >= n || (state.used & (uint64_t(1) << index)) || (state.facts & action.required) != action.required || ((state.facts & action.add) == action.add && action.add && action.ammo_gain <= 0 && action.medical_gain <= 0 && action.heal <= 0 && action.capacity_gain <= 0)) {
					continue;
				}
				for (int variant = 0; variant < 3; ++variant) {
					const auto &route = routes[state.node][action.node][variant];
					if (route.cost > 1e8f) {
						continue;
					}
					State child = state;
					child.facts = (state.facts & ~action.remove) | action.add;
					child.used |= uint64_t(1) << index;
					child.node = action.node;
					child.variant = variant;
					float protection = ((state.facts & Armed) && (state.facts & Loaded) ? .72f : 1.f) * (state.facts & Armoured ? .85f : 1.f) * (state.facts & Allied ? .8f : 1.f) * (state.facts & Safe ? .85f : 1.f);
					if (in.enforce_budget) {
						child.ammo += action.ammo_gain - action.ammo_cost - route.known_risk * .6f;
						child.medical += action.medical_gain - action.medical_cost;
						child.weight += action.weight_delta;
						child.capacity += action.capacity_gain;
						child.health = std::min(100.f, child.health - route.known_risk * 6.f * protection + action.heal);
						if (child.ammo < -.01f || child.medical < -.01f || child.weight > child.capacity + .01f || child.health < in.min_health) {
							continue;
						}
					}
					float common_risk = (8 + 55 * in.personality.caution) * (1 + (100 - in.health) / 100);
					child.cost += route.travel + route.risk * common_risk * protection + route.noise * in.personality.caution * 3 + action.seconds + (1 - action.confidence) * 12;
					child.risk += route.risk * protection;
					child.benefit += action.benefit + in.personality.preferences[action.intent] * 2 + (in.neural_intent == action.intent ? 1.5f : 0);
					if (child.cost > in.time_left) {
						continue;
					}
					for (int node : route.path) {
						child.steps.push_back({ "travel", "Travel via " + in.nodes[node].name, in.nodes[node].position, 0, node, 5, in.nodes[node].elevation });
					}
					child.steps.push_back({ action.kind, action.label, action.exact ? action.destination : in.nodes[action.node].position, action.target, action.node, action.intent, action.exact ? action.elevation : in.nodes[action.node].elevation });
					++result.expanded;
					if ((child.facts & in.goal) == in.goal) {
						StrategicPlan plan;
						plan.steps = child.steps;
						plan.label = action.label;
						plan.cost = child.cost;
						plan.risk = child.risk;
						plan.score = 200 - child.cost + child.benefit;
						plan.expected_health = child.health;
						plan.expected_ammo = child.ammo;
						plan.expected_weight = child.weight;
						plan.route_variant = variant;
						plan.reason = variant == 0 ? "Shorter route with accepted exposure" : variant == 1 ? "Lower exposure weighted by health and learned caution"
																										   : "Quieter route, avoiding noisy approaches";
						bool duplicate = false;
						for (const auto &other : result.alternatives) {
							if (other.steps.size() == plan.steps.size() && std::abs(other.cost - plan.cost) < .01f) {
								duplicate = true;
								break;
							}
						}
						if (!duplicate) {
							result.alternatives.push_back(plan);
							if (in.enforce_budget && result.alternatives.size() > 24) {
								std::stable_sort(result.alternatives.begin(), result.alternatives.end(), [](const StrategicPlan &a, const StrategicPlan &b) { return a.score > b.score; });
								result.alternatives.resize(24);
							}
						}
					} else {
						if (in.enforce_budget && !child.steps.empty() && rank(child) < partial_rank) {
							best_partial = child;
							partial_rank = rank(child);
							has_partial = true;
						}
						next.push_back(std::move(child));
					}
				}
			}
		}
		std::stable_sort(next.begin(), next.end(), [&](const State &a, const State &b) { return rank(a) < rank(b); });
		// Deduplicate abstract states so route aliases do not consume the beam.
		beam.clear();
		for (auto &state : next) {
			bool duplicate = false;
			for (const auto &kept : beam) {
				if (state.facts == kept.facts && state.node == kept.node && state.used == kept.used) {
					duplicate = true;
					break;
				}
			}
			if (!duplicate) {
				beam.push_back(std::move(state));
			}
			if (beam.size() >= size_t(std::clamp(in.beam_width, 1, 96))) {
				break;
			}
		}
		if (result.expanded > std::clamp(in.expansion_limit, 1000, 100000)) {
			break;
		}
	}
	if (result.alternatives.empty() && in.enforce_budget && has_partial) {
		StrategicPlan p;
		p.steps = best_partial.steps;
		p.label = "Executable preparation; goal still unresolved";
		p.reason = "Partial plan: no feasible complete solution within resource/time/search limits";
		p.complete = false;
		p.cost = best_partial.cost;
		p.risk = best_partial.risk;
		p.score = 100 - best_partial.cost + best_partial.benefit;
		p.expected_health = best_partial.health;
		p.expected_ammo = best_partial.ammo;
		p.expected_weight = best_partial.weight;
		result.alternatives.push_back(p);
	}
	std::stable_sort(result.alternatives.begin(), result.alternatives.end(), [](const StrategicPlan &a, const StrategicPlan &b) { return a.score > b.score; });
	if (result.alternatives.size() > 8) {
		result.alternatives.resize(8);
	}
	for (auto &plan : result.alternatives) {
		plan.expanded = result.expanded;
	}
	return result;
}
} //namespace lastlight
