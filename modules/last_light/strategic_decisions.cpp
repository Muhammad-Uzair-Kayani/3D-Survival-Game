#include "last_light_game.h"
#include <algorithm>
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = 0;
	b.y = 0;
	return a.distance_to(b);
}
Vector3 point(lastlight::Point p) {
	return Vector3(p.x, 0, p.z);
}
const char *intents[] = { "Explore", "Acquire", "Cooperate", "Avoid", "Defend", "Progress" };
} //namespace
void LastLightGame::build_plan(Actor &a) {
	using namespace lastlight;
	StrategicInput in;
	in.personality = controller == "nomemory" ? baseline_personality(a.stats.id) : a.personality;
	in.nodes = strategic_nodes;
	in.edges = strategic_edges;
	in.start = nearest_region(a.body->get_position());
	in.health = a.stats.health;
	in.ammo = a.stats.inventory.rounds;
	in.time_left = MAX(20., double(config.get("deadline_seconds", 1500)) - elapsed);
	in.shortcut_open = gate_open;
	auto &inventory = a.stats.inventory;
	in.initial = (key_found ? HaveKey : 0) | (power_on ? Power : 0) | (radio_done ? HaveSignal : 0) | (inventory.pistol ? Armed : 0) | (inventory.fuel ? Fuel : 0) | (inventory.medkits || inventory.bandages ? Medical : 0) | (inventory.armour ? Armoured : 0) | (inventory.cold < 30 ? Warm : 0) | (a.stats.hunger > 65 ? Fed : 0) | (inventory.hydration > 65 ? Hydrated : 0);
	if (a.teammate_id && find_actor(a.teammate_id) && find_actor(a.teammate_id)->stats.active()) {
		in.initial |= Allied;
	}
	if (inventory.rounds + inventory.reserve > 0) {
		in.initial |= Loaded;
	}
	int relay = 0;
	for (const auto &object : world_objects) {
		if (object.kind == "relay") {
			if (object.used) {
				in.initial |= uint32_t(RelayOne) << relay;
			}
			++relay;
		}
	}
	auto learned = policy.find(a.stats.id);
	if (learned != policy.end()) {
		for (int k = 0; k < 6; ++k) {
			if (learned->second == intents[k]) {
				in.neural_intent = k;
			}
		}
	}
	for (auto &other : actors) {
		if (a.visible_actors.count(other.stats.id) && a.memory[other.stats.id].threat > .25) {
			in.current_risk[nearest_region(other.body->get_position())] += .8;
		}
	}
	if (a.heard_until > elapsed) {
		in.current_risk[nearest_region(a.heard_position)] += .4;
	}
	auto action = [&](const char *kind, const String &label, Vector3 destination, int target, uint32_t required, uint32_t add, uint32_t remove, float duration, float benefit, int intent, float confidence = 1.f) {StrategicAction task;task.kind=kind;task.label=label.utf8().get_data();task.node=nearest_region(destination);task.target=target;task.required=required;task.add=add;task.remove=remove;task.seconds=duration;task.benefit=benefit;task.intent=intent;task.confidence=confidence;in.actions.push_back(task); };
	action("key", "Recover access key", key_position, 0, 0, HaveKey, 0, 1, 0, 5);
	action("crank", "Hand-crank emergency power", generator_position, 0, 0, Power, 0, 12, 0, 5);
	action("power", "Refuel generator", generator_position, 0, Fuel, Power, Fuel, 3, 3, 5);
	const uint32_t pairs[] = { RelayOne | RelayTwo, RelayOne | RelayThree, RelayTwo | RelayThree };
	relay = 0;
	for (const auto &object : world_objects) {
		if (object.kind == "relay") {
			action("relay", "Calibrate " + object.name, object.position, object.id, Power, uint32_t(RelayOne) << relay, 0, 2, 0, 5);
			++relay;
		}
		if (object.kind == "crate" && !a.known_empty_crates.count(object.id) && in.actions.size() < 35) {
			uint32_t fact = Medical;
			float benefit = 2;
			if (object.name.contains("rifle") || object.name.contains("smg") || object.name.contains("shotgun")) {
				fact = Armed | Loaded;
				benefit = inventory.pistol ? 0 : 10 * (.4f + a.personality.caution);
			} else if (object.name.contains("fuel")) {
				fact = Fuel;
				benefit = 1;
			} else if (object.name.contains("food")) {
				fact = Fed;
				benefit = (100 - a.stats.hunger) * .18f;
			} else {
				benefit = (100 - a.stats.health) * .15f;
			}
			action("search", "Search " + object.name, object.position, object.id, 0, fact, 0, 2, benefit, 1, .85);
		}
		if (object.kind == "shelter" && inventory.cold > 25) {
			action("warm", "Warm up at " + object.name, object.position, object.id, 0, Warm, 0, 6, inventory.cold * .3f, 3);
		}
	}
	for (uint32_t pair : pairs) {
		action("radio", "Restore evacuation signal", radio_position, 0, HaveKey | Power | pair, HaveSignal, 0, 3, 0, 5);
	}
	action("extract", "Extract at north landing", exit_position, 0, HaveSignal, Escaped, 0, 1, 0, 5);
	action("extract", "Extract at emergency landing", alternative_exit, 0, HaveSignal, Escaped, 0, 5, 0, 5);
	int known = 0;
	for (auto &[id, position] : a.known_items) {
		if (known >= 10 || in.actions.size() >= 56) {
			break;
		}
		// A known item may have been taken outside sight. The plan discovers that
		// on arrival, rather than receiving a hidden live inventory broadcast.
		String kind = a.known_item_kinds[id];
		uint32_t fact = 0;
		float benefit = 0;
		if (kind == "pistol" || kind == "smg" || kind == "rifle" || kind == "shotgun") {
			fact = Armed | Loaded;
			benefit = 15 * (.4f + a.personality.caution);
		}
		if (kind == "ammo" || kind == "smg_ammo" || kind == "rifle_ammo" || kind == "shells") {
			int type = kind == "ammo" ? 0 : kind == "smg_ammo" ? 1
					: kind == "rifle_ammo"					   ? 2
															   : 3;
			if (type == inventory.weapon_kind) {
				fact = Loaded;
				benefit = inventory.reserve < 12 ? 18 : 2;
			}
		}
		if (kind == "fuel") {
			fact = Fuel;
		}
		if (kind == "medkit" || kind == "bandage") {
			fact = Medical;
			benefit = (100 - a.stats.health) * .12f;
		}
		if (kind == "food") {
			fact = Fed;
			benefit = (100 - a.stats.hunger) * .25f;
		}
		if (kind == "water") {
			fact = Hydrated;
			benefit = (100 - inventory.hydration) * .25f;
		}
		if (kind == "armour") {
			fact = Armoured;
			benefit = 12 * a.personality.caution;
		}
		if (!fact) {
			continue;
		}
		action("loot", "Collect known " + kind, position, id, 0, fact, 0, 1, benefit, 1, 1.f);
		++known;
	}
	if ((inventory.medkits || inventory.bandages) && a.stats.health < 80) {
		action("heal", "Treat injuries", a.body->get_position(), 0, Medical, Treated, Medical, 2, (100 - a.stats.health) * .45f, 3);
	}
	int partners = 0;
	for (auto &other : actors) {
		if (other.stats.id != a.stats.id && other.stats.survivor && a.visible_actors.count(other.stats.id) && other.stats.active() && a.memory[other.stats.id].threat < .2 && partners < 2) {
			float trust = MAX(a.memory[other.stats.id].trust, a.persistent_trust[other.stats.id]);
			action("team", "Rendezvous with survivor " + itos(other.stats.id), other.body->get_position(), other.stats.id, 0, Allied, 0, 2, (a.personality.loyalty + trust) * 8, 2);
			++partners;
		}
	}
	if (!gate_open && partners) {
		action("gate", "Coordinate the shortcut gate", control_a, 0, Allied, Safe, 0, 3, 12 * a.personality.loyalty, 2);
	}
	auto result = make_strategy(in);
	a.alternatives = result.alternatives;
	if (!result.alternatives.empty()) {
		a.plan = controller == "random" ? result.alternatives[rng() % result.alternatives.size()] : result.alternatives.front();
		a.plan_step = 0;
		a.next_plan = elapsed + 6;
		a.reason = String(a.plan.reason.c_str());
	} else {
		a.plan = StrategicPlan();
		a.plan.label = "Scout for a feasible route";
		auto p = strategic_nodes[(a.stats.id + int(elapsed / 15)) % strategic_nodes.size()].position;
		a.plan.steps.push_back({ "travel", "Scout a new area", p, 0, nearest_region(point(p)), 0 });
		a.plan_step = 0;
		a.next_plan = elapsed + 10;
	}
}
void LastLightGame::decide_strategy(Actor &a) {
	using namespace lastlight;
	a.move = Vector3();
	a.interacting = false;
	a.aiming = false;
	a.sprint = false;
	a.crouching = false;
	if (!a.stats.active() || a.stats.busy()) {
		return;
	}
	if (!a.stats.survivor) {
		decide_distractor(a);
		return;
	}
	Vector3 position = a.body->get_position();
	auto &inventory = a.stats.inventory;
	bool immediate_danger = false;
	for (auto &other : actors) {
		if (a.visible_actors.count(other.stats.id) && a.memory[other.stats.id].threat > .25f && flat(position, other.body->get_position()) < 24) {
			immediate_danger = true;
		}
	}
	if (!immediate_danger && a.stats.health < 55 && inventory.medkits) {
		begin(a.stats, State::Heal);
		a.reason = "Stop bleeding before continuing the plan";
		return;
	}
	if (!immediate_danger && a.stats.health < 70 && inventory.bandages) {
		use_equipment(a, "bandage");
		return;
	}
	if (!immediate_danger && a.stats.hunger < 60 && inventory.food) {
		use_equipment(a, "food");
		return;
	}
	if (!immediate_danger && inventory.hydration < 60 && inventory.water) {
		use_equipment(a, "water");
		return;
	}
	if (inventory.pistol && inventory.rounds + inventory.reserve == 0) {
		for (int weapon = 0; weapon < 4; ++weapon) {
			if ((inventory.owned_weapons & (1 << weapon)) && weapon != inventory.weapon_kind && inventory.magazines[weapon] + inventory.ammunition[weapon] > 0) {
				switch_weapon(inventory, weapon);
				break;
			}
		}
	}
	if (inventory.pistol && inventory.rounds == 0 && inventory.reserve > 0) {
		begin(a.stats, State::Reload);
		a.reason = "Reload from compatible ammunition";
		return;
	}
	Actor *enemy = nullptr;
	float enemy_distance = 100;
	for (auto &other : actors) {
		if (a.visible_actors.count(other.stats.id) && a.memory[other.stats.id].threat > .25 && other.stats.active()) {
			float d = flat(position, other.body->get_position());
			if (d < enemy_distance) {
				enemy = &other;
				enemy_distance = d;
			}
		}
	}
	for (auto &other : actors) {
		if (other.stats.survivor && other.stats.id != a.stats.id && other.stats.state == State::Downed && a.visible_actors.count(other.stats.id) && inventory.medkits) {
			float trust = MAX(a.memory[other.stats.id].trust, a.persistent_trust[other.stats.id]);
			if ((!enemy || a.personality.loyalty + trust > a.personality.caution + .3f) && flat(position, other.body->get_position()) < 16) {
				a.target = other.body->get_position();
				if (a.nav->get_target_position().distance_to(a.target) > .2) {
					a.nav->set_target_position(a.target);
				}
				a.intent = "Cooperate";
				a.reason = "Risk accepted to rescue a known survivor";
				if (flat(position, a.target) < 1.8) {
					begin(a.stats, State::Revive, other.stats.id);
				}
				return;
			}
		}
	}
	if (a.reposition_until > elapsed) {
		a.target = a.tactical_position;
		a.aiming = false;
		a.nav->set_target_position(a.target);
		a.reason = "Reposition to clear a neutral survivor from the firing lane";
		a.intent = "Defend";
		return;
	}
	if (enemy && enemy_distance < 24) {
		bool fight = inventory.pistol && inventory.rounds > 0 && a.stats.health > 30 && (a.personality.confidence + a.personality.aggression + inventory.armour / 100.f > a.personality.caution || enemy_distance < 7);
		if (fight) {
			a.target = enemy->body->get_position();
			a.aiming = true;
			a.crouching = a.stats.health < 70;
			a.yaw = Math::atan2(-(a.target.x - position.x), -(a.target.z - position.z));
			a.intent = "Defend";
			a.reason = "Defend from cover; observed threat blocks the route";
			shoot(a);
			return;
		}
		a.intent = "Avoid";
		a.reason = "Disengage: risk exceeds ammunition/health confidence";
		if (inventory.smoke && enemy_distance < 13) {
			use_equipment(a, "smoke");
			return;
		}
		Vector3 away = position - enemy->body->get_position();
		away.y = 0;
		int best = -1;
		float score = -1e9;
		for (size_t i = 0; i < strategic_nodes.size(); ++i) {
			Vector3 p = point(strategic_nodes[i].position);
			float d = flat(position, p);
			if (d > 75 || d < 4) {
				continue;
			}
			float cover = (p - position).normalized().dot(away.normalized()) * 15 - d * .1f + (strategic_nodes[i].shelter ? 8 : 0);
			if (cover > score) {
				score = cover;
				best = i;
			}
		}
		a.target = best >= 0 ? point(strategic_nodes[best].position) : position + away.normalized() * 8;
		a.sprint = true;
		if (a.nav->get_target_position().distance_to(a.target) > .2) {
			a.nav->set_target_position(a.target);
		}
		a.next_plan = 0;
		return;
	}
	if (elapsed >= a.next_environment_use && a.personality.generosity > .55f) {
		for (auto &other : actors) {
			if (other.stats.id == a.stats.id || !other.stats.survivor || !other.stats.active() || !a.visible_actors.count(other.stats.id) || flat(position, other.body->get_position()) > 2.8f || a.memory[other.stats.id].threat > .2f) {
				continue;
			}
			String supply;
			auto &recipient = other.stats.inventory;
			if (other.stats.health < 65 && inventory.medkits > 1 && recipient.medkits == 0) {
				supply = "share_medkit";
			} else if (other.stats.hunger < 50 && inventory.food > 1 && recipient.food == 0) {
				supply = "share_food";
			} else if (recipient.hydration < 50 && inventory.water > 1 && recipient.water == 0) {
				supply = "share_water";
			} else if (recipient.pistol && inventory.weapon_kind == recipient.weapon_kind && inventory.reserve >= 24 && recipient.reserve < 6) {
				supply = "share_ammo";
			}
			if (!supply.is_empty() && begin(a.stats, State::Interact)) {
				a.environment_action = supply;
				a.environment_target = other.stats.id;
				a.stats.timer = 1.2f;
				a.next_environment_use = elapsed + 20;
				a.intent = "Cooperate";
				a.reason = "Share surplus with an observed survivor in need";
				return;
			}
		}
	}
	if (a.plan_step >= a.plan.steps.size() || elapsed >= a.next_plan) {
		build_plan(a);
	}
	while (a.plan_step < a.plan.steps.size()) {
		const auto step = a.plan.steps[a.plan_step];
		Vector3 target = point(step.point);
		bool complete = false;
		if (step.kind == "travel") {
			complete = flat(position, target) < 3;
		}
		if (step.kind == "key") {
			target = key_position;
			complete = key_found;
		}
		if (step.kind == "crank" || step.kind == "power") {
			target = generator_position;
			complete = power_on;
		}
		if (step.kind == "radio") {
			target = radio_position;
			complete = radio_done;
		}
		if (step.kind == "relay" || step.kind == "search" || step.kind == "warm") {
			for (auto &object : world_objects) {
				if (object.id == step.target) {
					target = object.position;
					complete = step.kind == "warm" ? inventory.cold < 20 : object.used && (step.kind != "search" || flat(position, target) < 5 || a.known_empty_crates.count(object.id));
				}
			}
		}
		if (step.kind == "loot") {
			auto found = a.known_items.find(step.target);
			if (found != a.known_items.end()) {
				target = found->second;
			}
			bool available = false;
			for (auto &item : items) {
				if (item.id == step.target) {
					available = item.available;
				}
			}
			if (!available && flat(position, target) < 5) {
				a.known_items.erase(step.target);
				a.known_item_kinds.erase(step.target);
				a.next_plan = 0;
				complete = true;
			}
		}
		if (step.kind == "team") {
			auto peer = find_actor(step.target);
			if (!peer || !peer->stats.active()) {
				a.next_plan = 0;
				complete = true;
			} else {
				target = peer->body->get_position();
				complete = flat(position, target) < 3;
				if (complete) {
					if (peer->personality.loyalty + MAX(peer->memory[a.stats.id].trust, peer->persistent_trust[a.stats.id]) > .65f && peer->memory[a.stats.id].threat < .2f) {
						a.teammate_id = peer->stats.id;
						if (!peer->teammate_id) {
							peer->teammate_id = a.stats.id;
							peer->next_plan = 0;
						}
					}
				}
			}
		}
		if (step.kind == "gate") {
			target = a.stats.id % 2 ? control_a : control_b;
			complete = gate_open;
		}
		if (step.kind == "heal") {
			complete = a.stats.health >= 80 || (!inventory.medkits && !inventory.bandages);
			if (!complete) {
				if (inventory.medkits) {
					begin(a.stats, State::Heal);
				} else {
					use_equipment(a, "bandage");
				}
				return;
			}
		}
		if (complete) {
			++a.plan_step;
			continue;
		}
		if (a.teammate_id && step.kind == "travel") {
			auto mate = find_actor(a.teammate_id);
			if (!mate || !mate->stats.active()) {
				a.teammate_id = 0;
				a.next_plan = 0;
			} else if (a.stats.id > mate->stats.id && flat(position, mate->body->get_position()) > 9) {
				target = mate->body->get_position();
				a.reason = "Follow the agreed teammate through this route";
			} else if (a.stats.id < mate->stats.id && flat(position, mate->body->get_position()) > 20) {
				if (a.team_wait_until < elapsed) {
					a.team_wait_until = elapsed + 5;
				}
				if (a.team_wait_until > elapsed + 1) {
					target = position;
				}
			}
		}
		a.target = target;
		a.intent = intents[step.intent];
		a.reason = String(step.label.c_str());

		if (step.kind == "crank" || step.kind == "power") {
			for (auto &object : world_objects) {
				if (object.kind == "generator") {
					a.environment_target = object.id;
				}
			}
		}
		if (step.kind == "radio") {
			for (auto &object : world_objects) {
				if (object.kind == "radio") {
					a.environment_target = object.id;
				}
			}
		}
		if (step.kind == "search" || step.kind == "relay") {
			a.environment_target = step.target;
		}
		a.interacting = step.kind != "travel" && step.kind != "extract" && flat(position, target) < 2.8;
		a.sprint = !inventory.cold && a.personality.confidence > .6f && flat(position, target) > 20 && a.heard_until < elapsed;
		a.crouching = a.heard_until > elapsed && a.personality.caution > .6f;
		if (a.nav->get_target_position().distance_to(a.target) > .2) {
			a.nav->set_target_position(a.target);
		}
		// Doors are physical and opened locally as a skill, not ignored by A*.
		for (auto &door : world_objects) {
			if (door.kind == "door" && !door.open && flat(position, door.position) < 2.6) {
				a.interacting = true;
				break;
			}
		}
		return;
	}
	a.next_plan = 0;
}
void LastLightGame::decide_distractor(Actor &a) {
	Vector3 position = a.body->get_position();
	Actor *target = nullptr;
	float closest = 42;
	for (auto &other : actors) {
		if (other.stats.survivor && a.visible_actors.count(other.stats.id) && other.stats.active()) {
			float d = flat(position, other.body->get_position());
			if (d < closest) {
				closest = d;
				target = &other;
			}
		}
	}
	auto &i = a.stats.inventory;
	if (a.stats.hunger < 50 && i.food) {
		use_equipment(a, "food");
		return;
	}
	if (i.hydration < 50 && i.water) {
		use_equipment(a, "water");
		return;
	}
	if (target && a.bot_role != 0 && a.bot_role != 4 && a.memory[target->stats.id].threat > .25f) {
		if (i.pistol && i.rounds > 0 && a.stats.health > 30) {
			a.target = target->body->get_position();
			a.aiming = true;
			a.yaw = Math::atan2(-(a.target.x - position.x), -(a.target.z - position.z));
			shoot(a);
			a.reason = "Defend against the survivor who attacked me";
			return;
		}
		Vector3 away = position - target->body->get_position();
		a.target = position + away.normalized() * 12;
		a.sprint = true;
		a.nav->set_target_position(a.target);
		a.reason = "Flee retaliation rather than continue scavenging";
		return;
	}
	if (a.stats.health < 45 && i.medkits) {
		lastlight::begin(a.stats, lastlight::State::Heal);
		return;
	}
	if (i.pistol && i.rounds == 0 && i.reserve > 0) {
		lastlight::begin(a.stats, lastlight::State::Reload);
		return;
	}
	if (a.bot_role == 0 || a.bot_role == 4) {
		if (target && i.rounds > 0) {
			a.target = target->body->get_position();
			a.aiming = true;
			a.crouching = a.bot_role == 4;
			a.yaw = Math::atan2(-(a.target.x - position.x), -(a.target.z - position.z));
			a.reason = a.bot_role == 4 ? "Ambush the crossing from a defended position" : "Pursue an exposed survivor";
			shoot(a);
			return;
		}
		if (target) {
			a.target = target->body->get_position() + Vector3(5, 0, -5);
			a.reason = "Reposition around the survivor's approach";
		} else {
			int zone = (a.stats.id + int(elapsed / 22)) % strategic_nodes.size();
			a.target = point(strategic_nodes[zone].position);
			a.reason = "Patrol between supplies and chokepoints";
			if (a.bot_role == 4) {
				a.target = point(strategic_nodes[(a.stats.id * 3) % strategic_nodes.size()].position);
				a.crouching = true;
				a.reason = "Guard a valuable approach and wait for a visible target";
			}
		}
	} else if (a.bot_role == 1) {
		Item *item = nullptr;
		float nearest = 100;
		for (auto &loot : items) {
			if (loot.available && a.visible_items.count(loot.id)) {
				const String &kind = loot.kind;
				if (kind == "key" || kind == "coat" && i.coat || kind == "backpack" && i.backpack || kind == "medkit" && i.medkits >= 3 || kind == "food" && i.food >= 4 || kind == "water" && i.water >= 4 || kind == "fuel" && i.fuel >= 2 || kind == "armour" && i.armour >= 100) {
					continue;
				}
				int weapon = kind == "pistol" ? 0 : kind == "smg" ? 1
						: kind == "rifle"						  ? 2
						: kind == "shotgun"						  ? 3
																  : -1;
				if (weapon >= 0 && (i.owned_weapons & (1 << weapon))) {
					continue;
				}
				if (i.weight() + .5f > i.capacity()) {
					continue;
				}
				float d = flat(position, loot.position);
				if (d < nearest) {
					nearest = d;
					item = &loot;
				}
			}
		}
		if (item) {
			a.target = item->position;
			a.interacting = nearest < 2;
			a.reason = "Compete for visible supplies before survivors arrive";
		} else {
			int zone = 3 + (a.stats.id + int(elapsed / 35)) % 11;
			a.target = point(strategic_nodes[zone].position);
			a.interacting = flat(position, a.target) < 4;
			a.reason = "Search public storage and supply areas";
		}
	} else if (a.bot_role == 3) {
		a.target = generator_position;
		a.reason = "Disrupt power and lure agents away from their route";
		bool protected_panel = false;
		if (compound_world) {
			for (auto &o : world_objects) {
				if (o.kind == "security" && o.used) {
					protected_panel = true;
				}
			}
		}
		if (flat(position, a.target) < 3 && power_on && !protected_panel && elapsed > a.next_environment_use) {
			power_on = false;
			a.next_environment_use = elapsed + 50;
			emit_event(a.stats.id, 0, "sabotage", position);
			notify("Power was interrupted. Generator assistance is available.");
		}
		if (elapsed > a.next_environment_use && i.decoys) {
			use_equipment(a, "decoy");
			a.next_environment_use = elapsed + 25;
		}
	} else {
		int zone = (int(elapsed / 18) + a.stats.id) % strategic_nodes.size();
		a.target = point(strategic_nodes[zone].position);
		a.sprint = true;
		a.reason = "Panicked flight draws attention through occupied areas";
		if (int(elapsed) % 8 == 0 && elapsed > a.next_environment_use) {
			emit_event(a.stats.id, 0, "noise", position);
			a.next_environment_use = elapsed + 7;
		}
	}
	if (a.nav->get_target_position().distance_to(a.target) > .2) {
		a.nav->set_target_position(a.target);
	}
	for (auto &door : world_objects) {
		if (door.kind == "door" && !door.open && flat(position, door.position) < 2.7) {
			a.interacting = true;
		}
	}
}
