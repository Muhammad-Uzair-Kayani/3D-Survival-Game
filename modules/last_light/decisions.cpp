#include "core/os/os.h"
#include "last_light_game.h"

namespace {
float distance_flat(Vector3 a, Vector3 b) {
	a.y = 0;
	b.y = 0;
	return a.distance_to(b);
}
bool useful(const lastlight::Inventory &i, const String &kind) {
	return (kind == "pistol" && !i.pistol) || (kind == "sword" && !i.sword) || (kind == "ammo" && i.reserve < 12) || (kind == "medkit" && i.medkits < 1) || (kind == "food" && i.food < 1);
}
} //namespace

Dictionary LastLightGame::build_observation(const Actor &a) const {
	// The adapter exposes only local perception and public mission data to the core.
	struct Adapter : lastlight::WorldPort {
		lastlight::ObservationInput input;
		lastlight::ObservationInput observe(int) const override { return input; }
	} adapter;
	auto &in = adapter.input;
	in.self = a.stats;
	auto position = a.body->get_position();
	auto destination = mission_target_for(a);
	in.position = { position.x, position.z };
	in.destination = { destination.x, destination.z };
	in.control = { control_a.x, control_a.z };
	in.elapsed = elapsed;
	in.gate_open = gate_open;
	in.key_found = key_found;
	in.heard_noise = a.heard_until > elapsed;
	for (auto &[id, belief] : a.memory) {
		in.contacts.push_back({ id, id >= 1 && id <= 5, bool(a.visible_actors.count(id)), belief });
	}
	for (const auto &item : items) {
		if (item.available && a.visible_items.count(item.id)) {
			in.visible_loot = true;
			break;
		}
	}
	auto observation = lastlight::observe(adapter, a.stats.id);
	if (arctic) {
		observation.features[1] = a.stats.inventory.rounds / float(lastlight::weapon_spec(a.stats.inventory.weapon_kind).magazine);
		observation.features[6] = MAX(0.f, 1 - position.distance_to(destination) / 600);
		observation.features[7] = CLAMP((destination.x - position.x + 300) / 600, 0.f, 1.f);
		observation.features[8] = CLAMP((destination.z - position.z + 300) / 600, 0.f, 1.f);
	}
	Array features, mask;
	for (float value : observation.features) {
		features.push_back(value);
	}
	for (int value : observation.mask) {
		mask.push_back(value);
	}
	Dictionary result;
	result["id"] = a.stats.id;
	result["features"] = features;
	result["valid_actions"] = mask;
	Array biases;
	for (float value : a.personality.preferences) {
		biases.push_back(value);
	}
	result["biases"] = biases;
	return result;
}

void LastLightGame::decide(Actor &a) {
	if (compound_world) {
		decide_compound(a);
		return;
	}
	if (arctic) {
		decide_strategy(a);
		return;
	}
	using namespace lastlight;
	a.move = Vector3();
	a.interacting = false;
	a.aiming = false;
	a.sprint = false;
	a.crouching = false;
	if (!a.stats.active() || a.stats.busy()) {
		return;
	}
	Vector3 position = a.body->get_position();
	a.use_sword = !a.stats.inventory.pistol && a.stats.inventory.sword;
	if (a.stats.survivor) {
		if (a.stats.health < 65 && a.stats.inventory.medkits > 0) {
			begin(a.stats, State::Heal);
			a.reason = "Treating wounds";
			return;
		}
		if (a.stats.hunger < 50 && a.stats.inventory.food > 0) {
			--a.stats.inventory.food;
			a.stats.hunger = MIN(100.f, a.stats.hunger + 45);
		}
		for (auto &other : actors) {
			if (other.stats.id != a.stats.id && other.stats.survivor && other.stats.state == State::Downed && a.stats.inventory.medkits > 0 && distance_flat(position, other.body->get_position()) < 12 && clear_line(position + Vector3(0, 1, 0), other.body->get_position() + Vector3(0, 1, 0))) {
				a.target = other.body->get_position();
				a.reason = "Rescuing a downed survivor";
				if (distance_flat(position, a.target) < 1.8) {
					begin(a.stats, State::Revive, other.stats.id);
				}
				a.nav->set_target_position(a.target);
				return;
			}
		}
		if (a.stats.inventory.pistol && a.stats.inventory.rounds == 0 && a.stats.inventory.reserve > 0) {
			begin(a.stats, State::Reload);
			a.reason = "Reloading";
			return;
		}
		Actor *threat = nullptr;
		float nearest = 100;
		for (auto &other : actors) {
			auto belief = a.memory.find(other.stats.id);
			if (a.visible_actors.count(other.stats.id) && belief != a.memory.end() && belief->second.threat > .25) {
				const float distance = distance_flat(position, other.body->get_position());
				if (distance < nearest) {
					nearest = distance;
					threat = &other;
				}
			}
		}
		Item *supply = nullptr;
		float closest = 100;
		for (auto &item : items) {
			if (item.available && a.visible_items.count(item.id) && useful(a.stats.inventory, item.kind)) {
				float distance = distance_flat(position, item.position);
				if (distance < closest) {
					closest = distance;
					supply = &item;
				}
			}
		}
		String selected = "Progress";
		if (threat) {
			selected = a.stats.inventory.pistol && a.stats.inventory.rounds > 0 ? "Defend" : "Avoid";
		} else if (supply) {
			selected = "Acquire";
		} else if (mission == 1 && !gate_open && elapsed < 25 && position.x < 20) {
			selected = "Cooperate";
		}
		Dictionary observation = build_observation(a);
		Array mask = observation["valid_actions"];
		const char *names[] = { "Explore", "Acquire", "Cooperate", "Avoid", "Defend", "Progress" };
		auto policy_choice = policy.find(a.stats.id);
		if (policy_choice != policy.end()) {
			for (int k = 0; k < 6; ++k) {
				if (policy_choice->second == names[k] && int(mask[k])) {
					selected = policy_choice->second;
					break;
				}
			}
		} else if (controller == "random") {
			Vector<int> available;
			for (int k = 0; k < 6; ++k) {
				if (int(mask[k])) {
					available.push_back(k);
				}
			}
			selected = names[available[rng() % available.size()]];
		}
		// Immediate self-preservation remains a skill constraint, not a learned label.
		if (threat && nearest < 4) {
			selected = (a.stats.inventory.pistol && a.stats.inventory.rounds > 0) || a.use_sword ? "Defend" : "Avoid";
		}
		a.intent = selected;
		if (selected == "Defend" && threat) {
			a.target = threat->body->get_position();
			a.aiming = true;
			a.yaw = Math::atan2(-(a.target.x - position.x), -(a.target.z - position.z));
			if (a.use_sword && nearest < 1.8) {
				melee(a);
			} else if (!a.use_sword) {
				shoot(a);
			} else {
				a.aiming = false;
			}
			a.reason = "Defending against observed aggression";
		} else if (selected == "Avoid") {
			Vector3 danger = threat ? threat->body->get_position() : a.heard_position;
			Vector3 away = position - danger;
			away.y = 0;
			if (away.length() < .1) {
				away = Vector3(-1, 0, 0);
			}
			a.target = position + away.normalized() * 5;
			a.target.x = CLAMP(a.target.x, 2.f, 38.f);
			a.target.z = CLAMP(a.target.z, 2.f, 30.f);
			a.sprint = true;
			a.reason = threat ? "Avoiding a witnessed threat" : "Moving away from heard gunfire";
		} else if (selected == "Acquire") {
			Item *target = supply;
			if (!target) {
				for (auto &item : items) {
					if (item.available && a.visible_items.count(item.id)) {
						target = &item;
						break;
					}
				}
			}
			if (target) {
				a.target = target->position;
				a.interacting = distance_flat(position, a.target) < 1.8;
				a.reason = "Gathering visible supplies";
			} else {
				a.target = mission == 0 ? key_position : mission == 1 ? radio_position
																	  : exit_position;
			}
		} else if (selected == "Cooperate" && !gate_open) {
			Vector3 chosen = a.stats.id % 2 ? control_a : control_b;
			for (auto &other : actors) {
				if (other.stats.id != a.stats.id && other.stats.active() && other.interacting && distance_flat(other.body->get_position(), chosen) < .8) {
					chosen = chosen == control_a ? control_b : control_a;
					break;
				}
			}
			a.target = chosen;
			a.interacting = distance_flat(position, chosen) < .8;
			a.reason = "Operating a complementary gate control";
		} else if (selected == "Explore") {
			int leg = (int(elapsed) / 8 + a.stats.id) % 4;
			const Vector3 points[] = { Vector3(6, 0, 8), Vector3(14, 0, 24), Vector3(28, 0, 8), Vector3(34, 0, 24) };
			a.target = points[leg];
			a.reason = "Exploring the next visible area";
		} else {
			a.target = mission == 0 ? key_position : mission == 1 ? radio_position
																  : exit_position;
			a.interacting = mission < 2 && distance_flat(position, a.target) < 1.8;
			a.reason = mission == 0 ? "Recovering the key" : mission == 1 ? "Restoring the radio signal"
																		  : "Reaching extraction";
		}
	} else if (a.bot_role == 0) {
		Actor *target = nullptr;
		float nearest = 17;
		for (auto &other : actors) {
			if (other.stats.survivor && a.visible_actors.count(other.stats.id)) {
				float distance = distance_flat(position, other.body->get_position());
				if (distance < nearest) {
					nearest = distance;
					target = &other;
				}
			}
		}
		if (target && a.stats.inventory.rounds > 0) {
			a.target = target->body->get_position();
			a.aiming = true;
			a.yaw = Math::atan2(-(a.target.x - position.x), -(a.target.z - position.z));
			shoot(a);
		}
		a.reason = "Scripted aggressor";
		return;
	} else if (a.bot_role == 1) {
		Item *target = nullptr;
		float closest = 100;
		for (auto &item : items) {
			if (item.available && item.kind != "key" && a.visible_items.count(item.id)) {
				float distance = distance_flat(position, item.position);
				if (distance < closest) {
					closest = distance;
					target = &item;
				}
			}
		}
		if (target) {
			a.target = target->position;
			a.interacting = closest < 1.8;
		} else {
			a.target = Vector3(28, 0, 24);
		}
		a.reason = "Scavenging visible supplies";
	} else {
		a.target = Vector3(24 + (int(elapsed) / 5 % 2) * 8, 0, 6 + (int(elapsed) / 8 % 2) * 20);
		a.sprint = true;
		a.reason = "Panicked runner";
	}
	if (a.target.distance_to(a.nav->get_target_position()) > .2) {
		a.nav->set_target_position(a.target);
	}
}
