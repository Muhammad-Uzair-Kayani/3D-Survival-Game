#include "last_light_game.h"
#include <cmath>
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
} //namespace
void LastLightGame::build_compound_plan(Actor &a) {
	using namespace lastlight;
	StrategicInput input;
	input.nodes = strategic_nodes;
	input.edges = strategic_edges;
	input.start = nearest_region(a.body->get_position());
	input.personality = controller == "nomemory" ? baseline_personality(a.stats.id) : a.personality;
	auto &i = a.stats.inventory;
	input.health = a.stats.health;
	input.ammo = i.rounds + i.reserve;
	input.medical = i.medkits + i.bandages;
	input.weight = i.weight();
	input.capacity = i.capacity();
	input.min_health = MAX(5.f, MIN(35.f, a.stats.health * .5f));
	input.enforce_budget = true;
	input.shortcut_open = gate_open;
	input.time_left = MAX(1., double(config.get("deadline_seconds", 1800)) - elapsed);
	input.depth_limit = 8;
	input.expansion_limit = 8000;
	input.beam_width = 24;
	input.initial = (i.tools ? HasTools : 0) | (key_found ? HaveKey : 0) | (power_on ? Power : 0) | (radio_done ? HaveSignal : 0) | (i.pistol ? Armed : 0) | (i.fuel ? Fuel : 0) | (i.rounds + i.reserve > 0 ? Loaded : 0) | (i.medkits + i.bandages > 0 ? Medical : 0) | (i.armour > 60 ? Armoured : 0) | (i.food ? HaveFood : 0) | (i.water ? HaveWater : 0) | (a.stats.hunger > 65 ? Fed : 0) | (i.hydration > 65 ? Hydrated : 0) | (i.cold < 30 ? Warm : 0) | (i.backpack ? HasPack : 0);
	if (i.rounds + i.reserve >= 12) {
		input.initial |= ReadyAmmo;
	}
	for (int gun = 0; gun < 4; ++gun) {
		if (i.owned_weapons & (1 << gun)) {
			input.initial |= uint32_t(HasPistol) << gun;
		}
	}
	for (auto &o : world_objects) {
		if (o.kind == "security" && o.used) {
			input.initial |= SecuredPower;
		}
	}
	int relay = 0;
	for (auto &o : world_objects) {
		if (o.kind == "relay") {
			if (o.used) {
				input.initial |= uint32_t(RelayOne) << relay;
			}
			++relay;
		}
	}
	for (auto &[id, c] : a.contacts) {
		if (c.hostile && c.confidence > .25) {
			input.current_risk[nearest_region(c.position)] += c.confidence * (c.visible ? .9f : .45f);
		}
	}
	if (elapsed - a.last_noise_at < 4 && a.noise_danger) {
		input.current_risk[nearest_region(a.last_noise)] += .3;
	}
	const char *intents[] = { "Explore", "Acquire", "Cooperate", "Avoid", "Defend", "Progress" };
	auto inference = policy.find(a.stats.id);
	if (inference != policy.end()) {
		for (int k = 0; k < 6; ++k) {
			if (inference->second == intents[k]) {
				input.neural_intent = k;
			}
		}
	}
	auto add = [&](const char *kind, const String &label, Vector3 p, int target, uint32_t required, uint32_t effect, uint32_t remove, float seconds, float benefit, int intent, float confidence = 1.f) -> StrategicAction & {StrategicAction x;x.kind=kind;x.label=label.utf8().get_data();x.node=nearest_region(p);x.target=target;x.required=required;x.add=effect;x.remove=remove;x.seconds=seconds;x.benefit=benefit;x.intent=intent;x.confidence=confidence;x.destination={p.x,p.z};x.elevation=p.y;x.exact=true;input.actions.push_back(x);return input.actions.back(); };
	add("key", "Recover the access key", key_position, 0, 0, HaveKey, 0, 1, 0, 5);
	add("crank", i.tools ? "Service the turbine backup with tools" : "Hand-crank backup power", generator_position, 0, 0, Power, 0, i.tools ? 6 : 12, 0, 5);
	add("power", "Refuel the emergency generator", generator_position, 0, Fuel, Power, Fuel, 3, 2, 5).weight_delta = -1.5;
	relay = 0;
	for (auto &o : world_objects) {
		if (o.kind == "security") {
			add("secure", "Lock the turbine service panel against sabotage", o.position, o.id, Power | HasTools, SecuredPower, 0, 4, 12, 5);
		}
		if (o.kind == "relay") {
			add("relay", "Calibrate " + o.name, o.position, o.id, Power, uint32_t(RelayOne) << relay, 0, 2, 0, 5);
			++relay;
		}
	}
	for (uint32_t pair : { uint32_t(RelayOne | RelayTwo), uint32_t(RelayOne | RelayThree), uint32_t(RelayTwo | RelayThree) }) {
		add("radio", "Restore the rescue signal", radio_position, 0, HaveKey | Power | pair, HaveSignal, 0, 3, 0, 5);
	}
	add("extract", "Secure north airlift", exit_position, 0, HaveSignal, Escaped, 0, beacon_active[0] ? MAX(1., beacon_times[0] - elapsed) : 22, 0, 5);
	add("extract", "Secure west airlift", alternative_exit, 0, HaveSignal, Escaped, 0, beacon_active[1] ? MAX(1., beacon_times[1] - elapsed) : 32, 0, 5);
	if (a.stats.health < 80 && i.medkits + i.bandages) {
		auto &heal = add("heal", "Treat before travelling", a.body->get_position(), 0, Medical, Treated, Medical, 2, (100 - a.stats.health) * .35f, 3);
		heal.heal = i.medkits ? 40 : 22;
		heal.medical_cost = 1;
		heal.weight_delta = i.medkits ? -.5f : -.1f;
	}
	for (auto &[id, p] : a.known_items) {
		if (input.actions.size() >= 52) {
			break;
		}
		String k = a.known_item_kinds[id];
		uint32_t fact = 0;
		float benefit = 0, weight = 0, ammo = 0, medical = 0;
		int gun = k == "pistol" ? 0 : k == "smg" ? 1
				: k == "rifle"					 ? 2
				: k == "shotgun"				 ? 3
												 : -1;
		if (gun >= 0 && !(i.owned_weapons & (1 << gun)) && (!i.pistol || i.rounds + i.reserve == 0 || a.order == 3)) {
			fact = Armed | Loaded | (uint32_t(HasPistol) << gun);
			benefit = i.pistol ? 3 : 20;
			weight = gun == 0 ? 1.5f : gun == 1 ? 2.5f
					: gun == 2					? 3.5f
												: 3.2f;
			ammo = weapon_spec(gun).magazine;
		}
		int type = k == "ammo" ? 0 : k == "smg_ammo" ? 1
				: k == "rifle_ammo"					 ? 2
				: k == "shells"						 ? 3
													 : -1;
		if (type == i.weapon_kind && i.reserve < 36) {
			fact = Loaded | ReadyAmmo;
			benefit = 20 - i.reserve * .4f;
			weight = .36;
			ammo = type == 3 ? 10 : 24;
		}
		if (k == "medkit" && i.medkits < 2) {
			fact = Medical;
			benefit = 10;
			weight = .5;
			medical = 1;
		}
		if (k == "bandage" && i.bandages < 2) {
			fact = Medical;
			benefit = 6;
			weight = .1;
			medical = 1;
		}
		if (k == "fuel" && !power_on && !i.fuel) {
			fact = Fuel;
			benefit = 3;
			weight = 1.5;
		}
		if (k == "armour" && i.armour < 60) {
			fact = Armoured;
			benefit = 10 * input.personality.caution;
			weight = 0;
		}
		if (k == "food" && i.food < 2) {
			fact = HaveFood;
			benefit = MAX(0.f, 65 - a.stats.hunger) * .3f;
			weight = .35;
		}
		if (k == "water" && i.water < 2) {
			fact = HaveWater;
			benefit = MAX(0.f, 65 - i.hydration) * .3f;
			weight = .5;
		}
		if (k == "tools" && !i.tools) {
			fact = HasTools;
			benefit = 8;
		}
		if (k == "backpack" && !i.backpack) {
			fact = HasPack;
			benefit = MAX(0.f, i.weight() - 7) * 3;
		}
		if (!fact) {
			continue;
		}
		auto &task = add("loot", "Collect observed " + k, p, id, 0, fact, 0, 1, benefit, 1);
		task.weight_delta = weight;
		task.ammo_gain = ammo;
		task.medical_gain = medical;
		if (k == "backpack") {
			task.capacity_gain = 10;
		}
	}
	for (auto &o : world_objects) {
		if (input.actions.size() >= 57) {
			break;
		}
		if (o.kind == "crate" && a.known_caches.count(o.id) && !a.known_empty_crates.count(o.id)) {
			float d = flat(a.body->get_position(), o.position);
			if (d < 45) {
				add("search", "Search observed cache", o.position, o.id, 0, Scouted, 0, 2, (i.rounds + i.reserve < 12 ? 15 : 2) + input.personality.curiosity * 4, 0, .5f);
			}
		}
	}
	struct Job {
		int id;
		uint32_t goal;
		Vector3 position;
		String label;
		float bid;
		int support = 0;
	};
	std::vector<Job> jobs;
	auto known_claim = [&](const MissionClaim &c) { return c.owner == a.stats.id || power_on || (a.contacts.count(c.owner) && a.visible_actors.count(c.owner)); };
	auto offer = [&](int id, uint32_t goal, Vector3 p, const String &name, float affinity) {if(a.failed_jobs.count(id)&&a.failed_jobs[id]>elapsed){return;
}float cost=flat(a.body->get_position(),p)/3+(input.current_risk[nearest_region(p)]+input.personality.region_risk[nearest_region(p)]*.5f)*(15+25*input.personality.caution)-(a.order==4?0.f:affinity) - ((a.order==3&&id>=14000)?22.f:0.f);
  auto found=mission_claims.find(id);if(id!=10003&&id!=10004&&found!=mission_claims.end()&&found->second.owner!=a.stats.id&&found->second.until>elapsed&&known_claim(found->second)){if(cost>=found->second.bid-12){cost+=65;
}}
  if(a.task_claim==id){cost-=7;
}jobs.push_back({id,goal,p,name,cost}); };
	if (!gate_open && flat(a.body->get_position(), control_a) < 30 && a.personality.loyalty > .6f) {
		int helpers = 1;
		for (auto &[peer, c] : a.contacts) {
			auto other = find_actor(peer);
			if (c.visible && peer >= 2 && peer <= 5 && other && other->personality.loyalty + MAX(a.memory[peer].trust, a.persistent_trust[peer]) > .6f && a.memory[peer].threat < .25f) {
				++helpers;
			}
		}
		if (helpers >= 2) {
			input.initial |= Allied;
			for (int pad = 0; pad < 2; ++pad) {
				Vector3 target = pad ? control_b : control_a;
				add("gate", "Coordinate the checkpoint shortcut", target, pad, Allied, Safe, 0, 4, 4, 2);
				offer(10006 + pad, Safe, target, "Checkpoint control " + itos(pad + 1), 12 * a.personality.loyalty);
			}
		}
	}
	for (auto &o : world_objects) {
		if (o.kind == "security" && !o.used && power_on && i.tools) {
			offer(o.id, SecuredPower, o.position, "Secure turbine controls", a.role == 4 ? 32 : 10);
		}
	}
	if (!key_found) {
		offer(10000, HaveKey, key_position, "Access key", a.role == 2 ? 24 : input.personality.curiosity * 6);
	}
	if (!power_on) {
		offer(10001, Power, generator_position, "Restore power", a.role == 4 ? 30 : i.fuel ? 14
																						   : 0);
	}
	relay = 0;
	int calibrated = 0;
	for (auto &o : world_objects) {
		if (o.kind == "relay") {
			if (o.used) {
				++calibrated;
			} else if (power_on) {
				offer(o.id, uint32_t(RelayOne) << relay, o.position, o.name, a.role == 4 ? 10 : 4);
			}
			++relay;
		}
	}
	if (key_found && power_on && calibrated >= 2 && !radio_done) {
		offer(10002, HaveSignal, radio_position, "Rescue signal", a.role == 2 ? 12 : 0);
	}
	if (radio_done) {
		offer(10003, Escaped, exit_position, "North extraction", -input.personality.caution * 4);
		offer(10004, Escaped, alternative_exit, "West extraction", input.personality.caution * 6);
	}
	// Guard/medic support is an explicit shared task, not a hidden read of a peer's
	// current position. Last-confirmed contacts and public claims supply evidence.
	if (!radio_done && a.order != 4 && (a.role == 1 || a.role == 3)) {
		for (auto &[id, claim] : mission_claims) {
			if (claim.owner == a.stats.id || claim.until <= elapsed || !known_claim(claim) || id >= 12000) {
				continue;
			}
			float d = flat(a.body->get_position(), claim.position);
			float cost = d / 3 - (a.role == 3 ? 21.f : 15.f);
			Job job;
			job.id = 12000 + claim.owner * 10 + a.stats.id;
			job.goal = Safe;
			Vector3 anchor = power_on ? claim.peer_position : (a.contacts.count(claim.owner) ? a.contacts[claim.owner].position : claim.position);
			job.position = anchor + (a.role == 1 ? Vector3(0, 0, 6) : Vector3(a.stats.id % 2 ? 5.f : -5.f, 0, -2));
			job.label = "Support " + claim.label;
			job.bid = cost;
			job.support = claim.owner;
			jobs.push_back(job);
		}
	}
	if (a.order == 3 || i.rounds + i.reserve < 8 || (a.stats.health < 65 && i.medkits + i.bandages == 0) || (a.stats.hunger < 45 && i.food == 0) || (i.hydration < 45 && i.water == 0)) {
		for (auto &[id, pos] : a.known_items) {
			String k = a.known_item_kinds[id];
			bool ammo = (k == "ammo" && i.weapon_kind == 0) || (k == "smg_ammo" && i.weapon_kind == 1) || (k == "rifle_ammo" && i.weapon_kind == 2) || (k == "shells" && i.weapon_kind == 3);
			if (ammo && i.rounds + i.reserve < 8) {
				offer(14000 + id, ReadyAmmo, pos, "Replenish compatible ammunition", 45);
			}
			if ((k == "medkit" || k == "bandage") && i.medkits + i.bandages == 0 && a.stats.health < 65) {
				offer(14000 + id, Medical, pos, "Acquire medical supplies", 45);
			}
			if (k == "food" && a.stats.hunger < 45 && i.food == 0) {
				offer(14000 + id, HaveFood, pos, "Find food before exhaustion", 45);
			}
			if (k == "water" && i.hydration < 45 && i.water == 0) {
				offer(14000 + id, HaveWater, pos, "Find drinking water", 45);
			}
		}
		for (auto &o : world_objects) {
			if (o.kind == "crate" && a.known_caches.count(o.id) && !a.known_empty_crates.count(o.id)) {
				offer(o.id, Scouted, o.position, "Inspect known cache for missing supplies", 30);
			}
		}
	}
	if (a.order == 3) {
		for (auto &action : input.actions) {
			if (action.kind == "loot" && action.benefit > 0) {
				offer(14000 + action.target, uint32_t(1) << 29, Vector3(action.destination.x, action.elevation, action.destination.z), "Scavenge observed supplies", 12);
			}
		}
	}
	if (jobs.empty()) {
		for (auto &o : world_objects) {
			if (o.kind == "crate" && a.known_caches.count(o.id) && !a.known_empty_crates.count(o.id)) {
				jobs.push_back({ o.id, Scouted, o.position, "Explore observed supplies", flat(a.body->get_position(), o.position) / 3 });
			}
		}
	}
	std::stable_sort(jobs.begin(), jobs.end(), [](const Job &a, const Job &b) { return a.bid < b.bid; });
	if (!jobs.empty()) {
		auto chosen = jobs.front();
		int previous_task = a.task_claim;
		bool changed_task = a.task_claim != chosen.id;
		a.task_claim = chosen.id;
		a.support_id = chosen.support;
		a.order_reason = chosen.label;
		input.goal = chosen.goal;
		if (a.order == 3 && chosen.id >= 14000) {
			for (auto &action : input.actions) {
				if (action.kind == "loot" && action.target == chosen.id - 14000) {
					action.add |= uint32_t(1) << 29;
				}
			}
		}
		if (chosen.id >= 12000 && chosen.id < 13000) {
			add("guard", chosen.label, chosen.position, chosen.support, 0, Safe, 0, 5, 8, 2);
		}
		auto old = mission_claims.find(chosen.id);
		if (old != mission_claims.end() && old->second.owner != a.stats.id && old->second.until > elapsed && chosen.bid < old->second.bid - 12) {
			auto peer = find_actor(old->second.owner);
			if (peer) {
				peer->next_plan = 0;
				peer->task_claim = 0;
			}
		}
		if (changed_task) {
			auto previous = mission_claims.find(previous_task);
			if (previous != mission_claims.end() && previous->second.owner == a.stats.id) {
				mission_claims.erase(previous);
			}
		}
		mission_claims[chosen.id] = { a.stats.id, elapsed + 12, chosen.bid, chosen.position, chosen.label, a.body->get_position() };
		if (changed_task) {
			a.task_progress_at = elapsed;
			a.task_distance = flat(a.body->get_position(), chosen.position);
		}
	} else {
		input.goal = Escaped;
	}
	if (a.task_claim == 10006 || a.task_claim == 10007) {
		for (auto &task : input.actions) {
			if (task.kind == "gate" && task.target != a.task_claim - 10006) {
				task.required |= uint32_t(1) << 31;
			}
		}
	}
	if (input.goal == Escaped) { // Constrain the selected landing while still comparing its approaches.
		int selected = a.task_claim;
		for (auto &action : input.actions) {
			if (action.kind == "extract" && ((selected == 10003 && action.node == nearest_region(alternative_exit)) || (selected == 10004 && action.node == nearest_region(exit_position)))) {
				action.required |= uint32_t(1) << 31;
			}
		}
	}
	auto result = make_strategy(input);
	a.alternatives = result.alternatives;
	if (!result.alternatives.empty()) {
		a.plan = controller == "random" ? result.alternatives[rng() % result.alternatives.size()] : result.alternatives.front();
		a.plan_step = 0;
		a.next_plan = elapsed + 8;
	} else {
		a.plan = StrategicPlan();
		a.plan.complete = false;
		a.plan.label = "No affordable plan; reassess nearby safety";
		a.plan.reason = "No executable solution satisfied the actual resource and time budget";
		Vector3 target = a.body->get_position();
		a.plan.steps.push_back({ "wait", "Hold safely while the task is unaffordable", { target.x, target.z }, 0, nearest_region(target), 3, target.y });
		a.plan_step = 0;
		a.next_plan = elapsed + 3;
	}
}
