#include "last_light_game.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/navigation_server_3d.h"
#include <algorithm>
#include <cmath>
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
float loot_value(const lastlight::Stats &s, const String &k) {
	auto &i = s.inventory;
	if (i.weight() > i.capacity() - 1 && k != "backpack" && k != "key") {
		return 0;
	}
	if (k == "medkit") {
		return i.medkits < 2 ? 18 + (100 - s.health) * .2f : 0;
	}
	if (k == "bandage") {
		return i.bandages < 2 ? 10 : 0;
	}
	if (k == "food") {
		return i.food < 2 ? MAX(0.f, 70 - s.hunger) * .35f + (i.food == 0 ? 8 : 0) : 0;
	}
	if (k == "water") {
		return i.water < 2 ? MAX(0.f, 70 - i.hydration) * .4f + (i.water == 0 ? 8 : 0) : 0;
	}
	if (k == "fuel") {
		return i.fuel == 0 ? 9 : 0;
	}
	if (k == "smoke") {
		return i.smoke < 2 ? 14 : 0;
	}
	if (k == "armour") {
		return i.armour < 50 ? 20 : 0;
	}
	if (k == "coat") {
		return !i.coat ? 8 + i.cold * .25f : 0;
	}
	if (k == "backpack") {
		return !i.backpack ? MAX(0.f, i.weight() - 7) * 4 : 0;
	}
	int weapon = k == "pistol" ? 0 : k == "smg" ? 1
			: k == "rifle"						? 2
			: k == "shotgun"					? 3
												: -1;
	if (weapon >= 0) {
		return !(i.owned_weapons & (1 << weapon)) && (!i.pistol || i.rounds + i.reserve == 0) ? 35 : 0;
	}
	int ammo = k == "ammo" ? 0 : k == "smg_ammo" ? 1
			: k == "rifle_ammo"					 ? 2
			: k == "shells"						 ? 3
												 : -1;
	if (ammo >= 0 && (i.owned_weapons & (1 << ammo))) {
		int rounds = ammo == i.weapon_kind ? i.reserve : i.ammunition[ammo];
		return rounds < 36 ? 30 - rounds * .4f : 0;
	}
	return 0;
}
} //namespace
void LastLightGame::perceive_compound(Actor &a) {
	for (auto &[id, c] : a.contacts) {
		c.visible = false;
		c.confidence = MAX(0.f, 1.f - float(elapsed - c.seen) / 18);
	}
	a.visible_actors.clear();
	a.visible_items.clear();
	if (!a.stats.active()) {
		return;
	}
	Vector3 eye = a.body->get_position() + Vector3(0, a.crouching ? .8f : 1.35f, 0);
	Vector3 front = -Basis(Vector3(0, 1, 0), a.yaw).get_column(2);
	for (auto &other : actors) {
		if (other.stats.state == lastlight::State::Dead && a.contacts.count(other.stats.id) && flat(other.body->get_position(), a.body->get_position()) < 36 && can_see(eye, other.body->get_position() + Vector3(0, .3, 0))) {
			a.contacts.erase(other.stats.id);
		}
		if (other.stats.id == a.stats.id || (!other.stats.active() && other.stats.state != lastlight::State::Downed)) {
			continue;
		}
		Vector3 delta = other.body->get_position() - a.body->get_position();
		float d = delta.length();
		float range = other.crouching ? 24 : 36;
		if (d > range || (d > 6 && front.dot(delta.normalized()) < -.2f)) {
			continue;
		}
		if (!can_see(eye, other.body->get_position() + Vector3(0, other.stats.state == lastlight::State::Downed ? .35f : other.crouching ? .65f
																																		 : 1.1f,
															   0))) {
			continue;
		}
		auto &c = a.contacts[other.stats.id];
		bool newly_seen = elapsed - c.seen > 4;
		if (c.seen > -1 && elapsed - c.seen > .01) {
			c.velocity = (other.body->get_position() - c.position) / float(elapsed - c.seen);
		}
		c.position = other.body->get_position();
		c.seen = elapsed;
		c.confidence = 1;
		c.visible = true;
		c.health_band = other.stats.health > 0 ? ((other.stats.health + 24) / 25) * 25 : 0;
		bool uniform_hostile = a.stats.survivor ? (!other.stats.survivor && (other.bot_role == 0 || other.bot_role == 4)) : ((a.bot_role == 0 || a.bot_role == 4) && other.stats.survivor);
		c.hostile = uniform_hostile || a.memory[other.stats.id].threat > .25f;
		if (c.hostile && newly_seen && (controller != "nomemory" || elapsed - a.react_at > 4)) {
			a.react_at = elapsed + .3 + (1 - a.personality.confidence) * .25;
		}
		a.visible_actors.insert(other.stats.id);
		a.memory[other.stats.id].last_seen = elapsed;
		if (uniform_hostile) {
			a.memory[other.stats.id].threat = MAX(.7f, a.memory[other.stats.id].threat);
		}
	}
	for (auto &o : world_objects) {
		if (o.kind == "crate" && flat(a.body->get_position(), o.position) < 22 && can_see(eye, o.position + Vector3(0, .8, 0))) {
			a.known_caches.insert(o.id);
			if (o.used) {
				a.known_empty_crates.insert(o.id);
			}
		}
	}
	for (auto &item : items) {
		float d = flat(a.body->get_position(), item.position);
		if (d > 17) {
			continue;
		}
		if (item.available && can_see(eye, item.position)) {
			a.visible_items.insert(item.id);
			a.known_items[item.id] = item.position;
			a.known_item_kinds[item.id] = item.kind;
		} else if (d < 4 && can_see(eye, item.position)) {
			a.known_items.erase(item.id);
			a.known_item_kinds.erase(item.id);
		}
	}
	if (a.heard_until > elapsed && elapsed - a.last_noise_at > .5) {
		a.last_noise = a.heard_position;
		a.last_noise_at = elapsed;
	}
	if (a.stats.damage_received > a.seen_damage) {
		a.last_damage_at = elapsed;
		a.suppression = MIN(1.f, a.suppression + .5f);
		a.seen_damage = a.stats.damage_received;
	}
}
void LastLightGame::evaluate_positions(Actor &a, const Vector3 &threat) {
	a.next_tactical_scan = elapsed + 1.2;
	a.peek_clear = false;
	a.flank_clear = false;
	Vector3 self = a.body->get_position();
	a.cover_protected = !can_see(threat + Vector3(0, 1.15, 0), self + Vector3(0, .7, 0));
	std::vector<std::pair<float, int>> near;
	for (auto &p : cover_points) {
		if (a.failed_jobs.count(20000 + p.id) && a.failed_jobs[20000 + p.id] > elapsed) {
			continue;
		}
		float d = flat(self, p.position);
		if (d > 28 || Math::abs(p.position.y - self.y) > 5) {
			continue;
		}
		if (p.owner && p.owner != a.stats.id && p.reserved_until > elapsed) {
			continue;
		}
		near.push_back({ d, p.id });
	}
	std::sort(near.begin(), near.end());
	float best = -1e6, flank_best = -1e6, retreat_best = -1e6;
	a.cover_id = -1;
	auto server = NavigationServer3D::get_singleton();
	RID map = get_world_3d()->get_navigation_map();
	for (size_t i = 0; i < near.size() && i < 6; ++i) {
		auto &p = cover_points[near[i].second];
		Vector3 projected = server->map_get_closest_point(map, p.position);
		if (flat(projected, p.position) > .65f || Math::abs(projected.y - p.position.y) > .75f) {
			continue;
		}
		bool protected_point = true;
		for (Vector3 margin : { Vector3(), Vector3(.28, 0, 0), Vector3(-.28, 0, 0), Vector3(0, 0, .28), Vector3(0, 0, -.28) }) {
			if (can_see(threat + Vector3(0, 1.15, 0), p.position + margin + Vector3(0, .7, 0))) {
				protected_point = false;
			}
		}
		if (!protected_point) {
			continue;
		}
		{
			auto route = server->map_get_path(map, self, p.position, true);
			if (route.is_empty() || flat(route[route.size() - 1], p.position) > 1 || Math::abs(route[route.size() - 1].y - p.position.y) > 1) {
				continue;
			}
			float length = 0;
			for (int n = 1; n < route.size(); ++n) {
				length += route[n].distance_to(route[n - 1]);
			}
			if (length > near[i].first * 2.5f + 6) {
				continue;
			}
			near[i].first = length;
		}

		Vector3 peek = p.position;
		bool firing = p.height < 1.35f && can_see(peek + Vector3(0, 1.3, 0), threat + Vector3(0, 1, 0));
		if (!firing) {
			for (Vector3 q : { p.peek_a, p.peek_b }) {
				Vector3 walk = server->map_get_closest_point(map, q);
				if (flat(walk, q) < .65f && Math::abs(walk.y - q.y) < .75f && flat(q, p.position) < 8 && can_see(q + Vector3(0, 1.3, 0), threat + Vector3(0, 1, 0))) {
					peek = q;
					firing = true;
					break;
				}
			}
		}
		float score = 35 - near[i].first * 1.15f + (firing ? 9 : 0) + p.normal.dot((self - threat).normalized()) * 6;
		if (score > best) {
			best = score;
			a.cover_id = p.id;
			a.cover_goal = p.position;
			a.peek_goal = peek;
			a.peek_clear = firing;
		}
		Vector3 old = (self - threat).normalized(), next = (peek - threat).normalized();
		float angle = 1 - old.dot(next);
		float flank = angle * 40 - near[i].first * .8f + (firing ? 12 : 0);
		if (firing && angle > .15f && flat(peek, threat) > 5 && flank > flank_best) {
			flank_best = flank;
			a.flank_goal = peek;
			a.flank_clear = true;
		}
		float retreat = flat(p.position, threat) - flat(self, threat) - near[i].first * .35f + 12;
		if (retreat > retreat_best) {
			retreat_best = retreat;
			a.retreat_goal = p.position;
		}
	}
	if (retreat_best < -1e5) {
		Vector3 away = (self - threat).normalized();
		float best_route = -1e6;
		for (auto &node : strategic_nodes) {
			Vector3 p(node.position.x, node.elevation, node.position.z);
			float d = flat(self, p);
			if (d < 8 || d > 65) {
				continue;
			}
			float score = (p - self).normalized().dot(away) * 25 - d * .2f + (node.shelter ? 8 : 0);
			if (score > best_route) {
				best_route = score;
				a.retreat_goal = p;
			}
		}
	}
}
void LastLightGame::follow_compound_plan(Actor &a) {
	using namespace lastlight;
	if (a.plan_step >= a.plan.steps.size() || elapsed >= a.next_plan) {
		build_compound_plan(a);
	}
	Vector3 pos = a.body->get_position();
	while (a.plan_step < a.plan.steps.size()) {
		auto step = a.plan.steps[a.plan_step];
		Vector3 target(step.point.x, step.height, step.point.z);
		bool complete = false;
		String kind = step.kind.c_str();
		int object_id = step.target;
		if (kind == "travel") {
			complete = flat(pos, target) < 2.2f && Math::abs(pos.y - target.y) < 1.2f;
		}
		if (kind == "key") {
			target = key_position;
			complete = key_found;
		}
		if (kind == "crank" || kind == "power") {
			target = generator_position;
			complete = power_on;
			for (auto &o : world_objects) {
				if (o.kind == "generator") {
					object_id = o.id;
				}
			}
		}
		if (kind == "relay" || kind == "search" || kind == "warm" || kind == "secure") {
			for (auto &o : world_objects) {
				if (o.id == step.target) {
					target = o.position;
					complete = kind == "warm" ? a.stats.inventory.cold < 20 : kind == "search" ? a.known_empty_crates.count(o.id)
																							   : o.used;
				}
			}
		}
		if (kind == "radio") {
			target = radio_position;
			complete = radio_done;
			for (auto &o : world_objects) {
				if (o.kind == "radio") {
					object_id = o.id;
				}
			}
		}
		if (kind == "loot") {
			if (!a.known_items.count(step.target)) {
				complete = true;
			} else {
				target = a.known_items[step.target];
				if (loot_value(a.stats, a.known_item_kinds[step.target]) <= 0) {
					complete = true;
				}
			}
		}
		if (kind == "team") {
			complete = true;
		}
		if (kind == "wait") {
			a.target = pos;
			a.nav->set_target_position(pos);
			a.reason = "No legal preparation or route meets the current budget";
			return;
		}
		if (kind == "gate") {
			target = step.target == 0 ? control_a : control_b;
			complete = gate_open;
			if (!gate_open && elapsed - a.task_progress_at > 14) {
				a.failed_jobs[a.task_claim] = elapsed + 30;
				mission_claims.erase(a.task_claim);
				a.task_claim = 0;
				a.next_plan = 0;
				return;
			}
		}
		if (kind == "guard") {
			bool needed = false;
			for (auto &[id, claim] : mission_claims) {
				if (claim.owner == step.target && claim.until > elapsed && id < 12000) {
					needed = true;
					Vector3 anchor = power_on ? claim.peer_position : (a.contacts.count(step.target) ? a.contacts[step.target].position : claim.position);
					target = anchor + (a.role == 1 ? Vector3(0, 0, 6) : Vector3(a.stats.id % 2 ? 5.f : -5.f, 0, -2));
					break;
				}
			}
			if (!needed) {
				a.next_plan = 0;
				complete = true;
			} else if (flat(pos, target) < 4) {
				a.target = pos;
				a.nav->set_target_position(pos);
				a.yaw = std::fmod(float(elapsed) * .22f, Math::TAU);
				a.reason = "Hold the assigned support angle; watch for a casualty or threat";
				return;
			}
		}
		if (kind == "heal") {
			complete = a.stats.health >= 80 || !a.stats.inventory.medkits;
			if (!complete) {
				begin(a.stats, State::Heal);
				return;
			}
		}
		if (kind == "extract") {
			target = Vector3(step.point.x, 0, step.point.z);
			int which = flat(target, alternative_exit) < flat(target, exit_position) ? 1 : 0;
			target = which ? alternative_exit : exit_position;
			for (auto &o : world_objects) {
				if (o.kind == "extraction" && int(o.value) == which) {
					object_id = o.id;
				}
			}
		}
		if (complete) {
			++a.plan_step;
			continue;
		}
		a.target = target;
		a.intent = step.intent == 1 ? "Acquire" : step.intent == 2 ? "Cooperate"
																   : "Progress";
		a.reason = step.label.c_str();
		a.environment_target = object_id;
		a.interacting = kind != "travel" && flat(pos, target) < 2.7f && Math::abs(pos.y - target.y) < 1.4f;
		if (a.nav->get_target_position().distance_to(target) > .3) {
			a.nav->set_target_position(target);
		}
		a.sprint = flat(pos, target) > 18 && a.stats.stamina > 40 && a.personality.confidence > .6f;
		for (auto &door : world_objects) {
			if (door.kind == "door" && !door.open && flat(pos, door.position) < 2.7 && Math::abs(pos.y - door.position.y) < 1) {
				a.environment_target = door.id;
				a.interacting = true;
				break;
			}
		}
		return;
	}
	a.next_plan = 0;
}
void LastLightGame::decide_compound(Actor &a) {
	using namespace lastlight;
	a.move = Vector3();
	a.aiming = false;
	a.sprint = false;
	a.crouching = false;
	a.interacting = false;
	if (!a.stats.active() || a.stats.busy()) {
		return;
	}
	Vector3 pos = a.body->get_position();
	if (a.order && elapsed > a.order_until) {
		a.order = 0;
		a.order_reason = "Order expired; resumed independent planning";
		a.next_plan = 0;
	}
	if (a.order == 1) {
		if (a.contacts.count(a.order_peer) && a.contacts[a.order_peer].visible) {
			a.order_position = a.contacts[a.order_peer].position;
		} else if (power_on) {
			auto leader = find_actor(a.order_peer);
			if (leader && leader->stats.active()) {
				a.order_position = leader->body->get_position();
			}
		}
	}
	Contact *enemy = nullptr;
	int target_id = 0;
	float nearest = 100;
	for (auto &[id, c] : a.contacts) {
		if (c.hostile && c.health_band > 0 && c.confidence > .25f && elapsed - c.seen < 8) {
			float d = flat(pos, c.position);
			if (d < nearest) {
				nearest = d;
				enemy = &c;
				target_id = id;
			}
		}
	}
	bool incoming = elapsed - a.last_damage_at < 4;
	bool gun_noise = a.noise_danger && elapsed - a.last_noise_at < 2;
	if (!a.stats.survivor && !enemy && !incoming) {
		decide_distractor(a);
		return;
	}
	bool danger = (enemy && (enemy->visible || elapsed - enemy->seen < 3)) || incoming || gun_noise;
	Vector3 threat = enemy ? enemy->position : a.last_noise;
	bool reaching_tactical = (a.tactic == Tactic::Cover || a.tactic == Tactic::Flank || a.tactic == Tactic::Withdraw) && elapsed - a.tactical_since < 3 && flat(pos, a.target) > 1.5;
	if (danger && elapsed >= a.next_tactical_scan && !reaching_tactical) {
		evaluate_positions(a, threat);
	}
	auto &inv = a.stats.inventory;
	if (inv.pistol && inv.rounds + inv.reserve == 0) {
		for (int gun = 0; gun < 4; ++gun) {
			if ((inv.owned_weapons & (1 << gun)) && gun != inv.weapon_kind && inv.magazines[gun] + inv.ammunition[gun] > 0) {
				switch_weapon(inv, gun);
				break;
			}
		}
	}
	int loot = 0;
	float loot_score = 0;
	for (auto &[id, p] : a.known_items) {
		float v = loot_value(a.stats, a.known_item_kinds[id]) - flat(pos, p) * .7f;
		if (v > loot_score) {
			loot_score = v;
			loot = id;
		}
	}
	Actor *casualty = nullptr;
	bool safe_rescue = false;
	for (auto &other : actors) {
		if (a.stats.survivor && other.stats.survivor && (!a.contacts.count(other.stats.id) || !a.contacts[other.stats.id].hostile) && other.stats.state == State::Downed && a.visible_actors.count(other.stats.id) && flat(pos, other.body->get_position()) < 22) {
			casualty = &other;
			safe_rescue = !danger || (enemy && !can_see(threat + Vector3(0, 1.2, 0), other.body->get_position() + Vector3(0, .5, 0)));
			break;
		}
	}
	TacticalInput c;
	c.health = a.stats.health;
	c.suppression = a.suppression;
	c.caution = a.personality.caution;
	c.confidence = a.personality.confidence;
	c.aggression = a.personality.aggression;
	c.loyalty = a.personality.loyalty;
	c.rounds = inv.rounds;
	c.reserve = inv.reserve;
	c.magazine = weapon_spec(inv.weapon_kind).magazine;
	c.range = weapon_spec(inv.weapon_kind).range;
	c.distance = nearest;
	c.danger = danger;
	c.visible = enemy && enemy->visible;
	c.hostile = enemy;
	c.contact_age = enemy ? float(elapsed - enemy->seen) : 100;
	c.ready = elapsed >= a.react_at;
	c.smoke = inv.smoke;
	c.has_medical = inv.medkits || inv.bandages;
	c.cover_available = a.cover_id >= 0;
	c.cover_distance = a.cover_id >= 0 ? flat(pos, a.cover_goal) : 0;
	if (enemy) {
		a.cover_protected = !can_see(threat + Vector3(0, 1.15, 0), pos + Vector3(0, .7, 0));
	}
	c.protected_here = enemy && a.cover_protected;
	c.peek_available = a.peek_clear;
	c.flank_available = a.flank_clear;
	c.needs_loot = loot > 0;
	c.heard = elapsed - a.last_noise_at < 5 && elapsed >= a.next_investigate;
	if (a.tactic == Tactic::Investigate && elapsed - a.tactical_since > 5) {
		a.next_investigate = elapsed + 20;
		c.heard = false;
	}
	c.downed_ally = casualty;
	c.rescue_safe = safe_rescue && inv.medkits > 0;
	c.learned = a.tactical_learning;
	for (auto &[id, contact] : a.contacts) {
		if (!contact.visible) {
			continue;
		}
		if (contact.hostile) {
			++c.enemies;
		} else if (id <= 5 && a.stats.survivor && id != a.stats.id) {
			++c.allies;
		}
	}
	if (enemy) {
		c.firing_lane = can_see(pos + Vector3(0, 1.2, 0), threat + Vector3(0, 1, 0)) || (a.cover_protected && a.peek_clear);
		for (auto &[id, peer] : a.contacts) {
			if (!peer.visible || peer.hostile || id == target_id) {
				continue;
			}
			Vector3 line = threat - pos;
			float t = (peer.position - pos).dot(line) / MAX(.01f, line.length_squared());
			if (t > 0 && t < 1 && flat(pos + line * t, peer.position) < .65f) {
				c.firing_lane = false;
			}
		}
	}
	if (mission == 2) {
		for (int n = 0; n < 2; ++n) {
			if (beacon_active[n] && elapsed < beacon_times[n] && flat(pos, n ? alternative_exit : exit_position) < 10) {
				c.waiting = true;
			}
		}
	}
	if (a.order == 2 && !danger) {
		c.waiting = flat(pos, a.order_position) < 3;
	}
	a.tactical_context = lastlight::tactical_context(c);
	auto learned = a.contextual_values.find(a.tactical_context);
	if (learned != a.contextual_values.end()) {
		for (int k = 0; k < TACTIC_COUNT; ++k) {
			c.learned[k] += learned->second[k];
		}
	}
	a.tactical_decision = choose_tactic(c, a.tactic, elapsed < a.tactical_until);
	Tactic choice = a.tactical_decision.action;
	if (choice != a.tactic) {
		a.tactic = choice;
		a.tactical_since = elapsed;
		a.tactical_until = elapsed + 1.4;
		a.burst_shots = 0;
	}
	a.tactical_reason = a.tactical_decision.reason[int(choice)].c_str();
	a.reason = tactic_name(choice);
	a.engagement_id = target_id;
	a.aim_point = threat;
	auto travel = [&](Vector3 goal, bool run) {a.target=goal;a.sprint=run&&a.stats.stamina>20;if(a.nav->get_target_position().distance_to(goal)>.3){a.nav->set_target_position(goal);
} };
	if (choice == Tactic::Smoke) {
		use_equipment(a, "smoke");
		return;
	}
	if (choice == Tactic::Reload) {
		begin(a.stats, State::Reload);
		return;
	}
	if (choice == Tactic::Treat) {
		if (inv.medkits) {
			begin(a.stats, State::Heal);
		} else {
			use_equipment(a, "bandage");
		}
		return;
	}
	if (choice == Tactic::Rescue && casualty) {
		travel(casualty->body->get_position(), false);
		if (flat(pos, a.target) < 1.8) {
			begin(a.stats, State::Revive, casualty->stats.id);
		}
		return;
	}
	if (choice == Tactic::Acquire && loot) {
		travel(a.known_items[loot], false);
		a.interacting = flat(pos, a.target) < 1.7;
		return;
	}
	if (choice == Tactic::Cover) {
		if (flat(pos, a.cover_goal) < .5f && !a.cover_protected && elapsed - a.tactical_since > 3) {
			a.failed_jobs[20000 + a.cover_id] = elapsed + 15;
			a.next_tactical_scan = 0;
		}
		travel(a.cover_goal, false);
		if (enemy && elapsed - enemy->seen < 4 && c.ready) {
			a.aiming = true;
			a.yaw = Math::atan2(-(threat.x - pos.x), -(threat.z - pos.z));
		}
		a.crouching = flat(pos, a.target) < 1.5;
		if (a.cover_id >= 0) {
			cover_points[a.cover_id].owner = a.stats.id;
			cover_points[a.cover_id].reserved_until = elapsed + 3;
		}
		return;
	}
	if (choice == Tactic::Withdraw) {
		travel(a.retreat_goal, true);
		a.intent = "Avoid";
		return;
	}
	if (choice == Tactic::Flank) {
		travel(a.flank_goal, true);
		a.intent = "Defend";
		return;
	}
	if (choice == Tactic::Peek || choice == Tactic::Suppress) {
		a.intent = "Defend";
		float phase = std::fmod(float(elapsed - a.tactical_since), 2.2f);
		if (a.cover_id >= 0 && a.peek_clear && !(c.visible && c.firing_lane && !c.protected_here && c.cover_distance > 4)) {
			Vector3 goal = phase < 1.2f ? a.peek_goal : a.cover_goal;
			travel(goal, false);
			if (flat(pos, goal) > .5f) {
				a.crouching = phase >= 1.2f;
				return;
			}
		}
		if (c.visible && c.firing_lane && !c.protected_here && c.cover_distance > 4) {
			a.target = pos;
			a.nav->set_target_position(pos);
		}
		a.crouching = phase >= 1.2f && c.protected_here;
		a.aiming = !a.crouching;
		a.yaw = Math::atan2(-(threat.x - pos.x), -(threat.z - pos.z));
		if (a.aiming && phase > .25f && (choice != Tactic::Suppress || a.burst_shots < 3)) {
			int shots = a.stats.shots;
			shoot(a);
			if (a.stats.shots > shots) {
				++a.burst_shots;
			}
		}
		return;
	}
	if (choice == Tactic::Investigate) {
		travel(a.last_noise, false);
		if (flat(pos, a.last_noise) < 3) {
			a.target = pos;
			a.nav->set_target_position(pos);
			a.yaw = float(elapsed - a.last_noise_at) * 1.2f;
			if (elapsed - a.last_noise_at > 4) {
				a.next_investigate = elapsed + 20;
				a.last_noise_at = -100;
				a.heard_until = 0;
			}
		}
		return;
	}
	if (choice == Tactic::Hold) {
		a.target = pos;
		a.nav->set_target_position(pos);
		a.crouching = danger;
		if (enemy) {
			a.yaw = Math::atan2(-(threat.x - pos.x), -(threat.z - pos.z));
		}
		return;
	}
	if (!danger) {
		if (a.stats.hunger < 65 && inv.food) {
			use_equipment(a, "food");
			return;
		}
		if (inv.hydration < 65 && inv.water) {
			use_equipment(a, "water");
			return;
		}
	}
	if (a.order == 1 || a.order == 2) {
		Vector3 goal = a.order_position;
		if (a.order == 1) {
			goal += Vector3(a.stats.id % 2 ? 3.f : -3.f, 0, 3);
		}
		if (flat(pos, goal) < (a.order == 1 ? 4 : 1.5)) {
			a.target = pos;
			a.nav->set_target_position(pos);
			a.yaw = std::fmod(float(elapsed) * .3f, Math::TAU);
		} else {
			travel(goal, false);
		}
		a.reason = a.order_reason;
		return;
	}
	follow_compound_plan(a);
}

void LastLightGame::step_compound_ai() {
	int budget = 3;
	for (size_t checked = 0; checked < actors.size() && budget > 0; ++checked) {
		int index = decision_cursor++ % actors.size();
		auto &a = actors[index];
		if (!a.stats.active() || a.next_decision > elapsed) {
			continue;
		}
		a.next_decision = elapsed + .2 + (a.stats.id % 3) * .015;
		--budget;
		if (controller == "nomemory" && a.stats.survivor) {
			a.contacts.clear();
			a.memory.clear();
			a.known_items.clear();
			a.known_item_kinds.clear();
			a.known_caches.clear();
			a.heard_until = 0;
		}
		sense(a);
		bool fixture = script_case.begins_with("compound_") && script_case != "compound_hostile";
		if (!fixture && (a.stats.id != 1 || autoplay || smoke)) {
			decide(a);
		}
	}
}

void LastLightGame::refresh_compound_routes() {
	auto nav = NavigationServer3D::get_singleton();
	RID map = get_world_3d()->get_navigation_map();
	if (nav->map_get_iteration_id(map) == 0) {
		return;
	}
	Vector3 anchor = nav->map_get_closest_point(map, Vector3(128, 0, 212));
	if (flat(anchor, Vector3(128, 0, 212)) > 3) {
		return;
	}
	compound_nav_iteration = nav->map_get_iteration_id(map);
	for (auto &edge : strategic_edges) {
		auto a = strategic_nodes[edge.from], b = strategic_nodes[edge.to];
		Vector3 from(a.position.x, a.elevation, a.position.z), to(b.position.x, b.elevation, b.position.z);
		auto route = nav->map_get_path(map, from, to, true);
		edge.enabled = !route.is_empty() && flat(route[route.size() - 1], to) < 2 && Math::abs(route[route.size() - 1].y - to.y) < 1.2f;
		edge.travel_distance = 0;
		for (int p = 1; p < route.size(); ++p) {
			edge.travel_distance += route[p].distance_to(route[p - 1]);
		}
	}
	compound_paths_ready = true;
}
