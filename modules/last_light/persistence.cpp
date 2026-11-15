#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "last_light_game.h"
#include "scene/main/scene_tree.h"
#include <cmath>
#include <set>
#include <sstream>
namespace {
Array vector_data(const Vector3 &v) {
	Array a;
	a.push_back(v.x);
	a.push_back(v.y);
	a.push_back(v.z);
	return a;
}
bool number(const Variant &v) {
	return (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) && std::isfinite(double(v));
}
bool vector_read(const Variant &v, Vector3 &out) {
	if (v.get_type() != Variant::ARRAY) {
		return false;
	}
	Array a = v;
	if (a.size() != 3) {
		return false;
	}
	for (int k = 0; k < 3; ++k) {
		if (!number(a[k]) || Math::abs(double(a[k])) > 1000) {
			return false;
		}
	}
	out = Vector3(double(a[0]), double(a[1]), double(a[2]));
	return true;
}
} //namespace
Dictionary LastLightGame::snapshot() const {
	Dictionary data;
	data["version"] = compound_world ? 4 : 3;
	data["world"] = compound_world ? "frostworks" : arctic ? "arctic"
														   : "lab";
	data["session_id"] = session_id;
	data["power_on"] = power_on;
	Array objects;
	for (const auto &object : world_objects) {
		Dictionary entry;
		entry["id"] = object.id;
		entry["used"] = object.used;
		entry["open"] = object.open;
		objects.push_back(entry);
	}
	data["world_objects"] = objects;
	data["active_bots"] = spawn_count;
	data["event_sequence"] = event_sequence;
	data["cooperative_uses"] = cooperative_uses;
	data["harmful_attacks"] = harmful_attacks;
	data["content_hash"] = JSON::stringify(config).sha256_text();
	std::ostringstream random_state;
	random_state << rng;
	data["rng_state"] = String(random_state.str().c_str());
	data["ai_clock"] = ai_clock;
	data["camera_focus"] = camera_focus;
	data["seed"] = seed;
	data["elapsed"] = elapsed;
	data["mission"] = mission;
	data["key"] = key_found;
	data["radio"] = radio_done;
	data["gate"] = gate_open;
	data["gate_hold"] = gate_hold;
	data["gate_lift"] = gate_lift;
	data["gate_opening"] = gate_opening;
	data["gate_operator_a"] = gate_operator_a;
	data["gate_operator_b"] = gate_operator_b;
	data["victory"] = victory;
	data["defeat"] = defeat;
	data["paused"] = paused;
	data["camera_yaw"] = camera_yaw;
	data["camera_pitch"] = camera_pitch;
	data["shots_hit"] = shots_hit;
	data["wall_blocks"] = wall_blocks;
	data["revivals"] = revivals;
	data["pickups"] = pickups;
	data["python_failures"] = python_failures;
	Array state;
	for (auto &a : actors) {
		auto &s = a.stats;
		auto &i = s.inventory;
		Dictionary d;
		d["id"] = s.id;
		d["profile_revision"] = a.personality.revision;
		d["teammate"] = a.teammate_id;
		Dictionary equipment;
		equipment["weapon_kind"] = i.weapon_kind;
		equipment["owned_weapons"] = i.owned_weapons;
		equipment["bandages"] = i.bandages;
		equipment["water"] = i.water;
		equipment["fuel"] = i.fuel;
		equipment["smoke"] = i.smoke;
		equipment["decoys"] = i.decoys;
		equipment["armour"] = i.armour;
		equipment["backpack"] = i.backpack;
		equipment["tools"] = i.tools;
		equipment["coat"] = i.coat;
		equipment["hydration"] = i.hydration;
		equipment["cold"] = i.cold;
		Array magazines, ammunition;
		for (int value : i.magazines) {
			magazines.push_back(value);
		}
		for (int value : i.ammunition) {
			ammunition.push_back(value);
		}
		equipment["magazines"] = magazines;
		equipment["ammunition"] = ammunition;
		d["equipment"] = equipment;
		d["environment_action"] = a.environment_action;
		d["environment_target"] = a.environment_target;
		Array knowledge;
		for (auto &[id, position] : a.known_items) {
			Dictionary known;
			known["id"] = id;
			known["position"] = vector_data(position);
			auto kind = a.known_item_kinds.find(id);
			known["kind"] = kind == a.known_item_kinds.end() ? String() : kind->second;
			knowledge.push_back(known);
		}
		d["known_items"] = knowledge;
		d["health"] = s.health;
		d["state"] = int(s.state);
		d["state_name"] = lastlight::state_name(s.state);
		d["position"] = vector_data(a.body->get_position());
		d["velocity"] = vector_data(a.body->get_velocity());
		d["target"] = vector_data(a.target);
		d["yaw"] = a.yaw;
		d["stamina"] = s.stamina;
		d["hunger"] = s.hunger;
		d["hunger_clock"] = s.hunger_clock;
		d["timer"] = s.timer;
		d["cooldown"] = s.cooldown;
		d["bleed"] = s.bleed;
		d["pending"] = s.pending_target;
		d["pistol"] = i.pistol;
		d["sword"] = i.sword;
		d["rounds"] = i.rounds;
		d["reserve"] = i.reserve;
		d["medkits"] = i.medkits;
		d["food"] = i.food;
		d["use_sword"] = a.use_sword;
		d["aiming"] = a.aiming;
		d["crouching"] = a.crouching;
		d["sprint"] = a.sprint;
		d["interacting"] = a.interacting;
		d["shots"] = s.shots;
		d["damage_received"] = s.damage_received;
		d["revives"] = s.revives;
		d["pickups"] = s.pickups;
		d["reason"] = a.reason;
		d["bot_role"] = a.bot_role;
		d["travelled"] = a.travelled;
		d["arrived_health"] = a.arrived_health;
		d["arrived_time"] = a.arrived_time;
		d["melee_pending"] = a.melee_pending;
		d["path_points"] = a.nav->get_current_navigation_path().size();
		d["path_reachable"] = a.nav->is_target_reachable();
		Array route;
		for (const auto &point : a.nav->get_current_navigation_path()) {
			route.push_back(vector_data(point));
		}
		d["route"] = route;
		if (!a.nav->get_current_navigation_path().is_empty()) {
			d["first_path_point"] = vector_data(a.nav->get_current_navigation_path()[0]);
		}
		Array blends;
		for (float f : { a.blend.idle, a.blend.walk, a.blend.run, a.blend.crouch, a.blend.aim, a.blend.action, a.blend.phase, a.blend.forward, a.blend.backward, a.blend.left, a.blend.right }) {
			blends.push_back(f);
		}
		d["blend"] = blends;
		Array beliefs;
		for (auto &[id, b] : a.memory) {
			Dictionary belief;
			belief["id"] = id;
			belief["trust"] = b.trust;
			belief["threat"] = b.threat;
			belief["last_seen"] = b.last_seen;
			belief["last_attack"] = b.last_attack;
			beliefs.push_back(belief);
		}
		d["memory"] = beliefs;
		state.push_back(d);
	}
	data["actors"] = state;
	Array loot;
	for (auto &i : items) {
		Dictionary d;
		d["id"] = i.id;
		d["available"] = i.available;
		loot.push_back(d);
	}
	data["items"] = loot;
	data["journal"] = journal;
	if (compound_world) {
		snapshot_compound(data);
	}
	return data;
}
bool LastLightGame::restore(const Dictionary &data) {
	if (int(data.get("version", 0)) != (compound_world ? 4 : 3) || int(data.get("seed", -1)) != seed || String(data.get("content_hash", String())) != JSON::stringify(config).sha256_text()) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	for (String field : { String("elapsed"), String("gate_hold"), String("gate_lift"), String("camera_yaw"), String("camera_pitch") }) {
		if (!number(data.get(field, Variant()))) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
	}
	if (String(data.get("world", "")) != (compound_world ? "frostworks" : arctic ? "arctic"
																				 : "lab") ||
			data.get("session_id", Variant()).get_type() != Variant::STRING || String(data["session_id"]).length() > 128 || data.get("power_on", Variant()).get_type() != Variant::BOOL) {
		return false;
	}
	if (data.get("world_objects", Variant()).get_type() != Variant::ARRAY) {
		return false;
	}
	Array objects = data["world_objects"];
	if (objects.size() != int(world_objects.size())) {
		return false;
	}
	for (int j = 0; j < objects.size(); ++j) {
		if (objects[j].get_type() != Variant::DICTIONARY) {
			return false;
		}
		Dictionary object = objects[j];
		if (int(object.get("id", 0)) != world_objects[j].id || object.get("used", Variant()).get_type() != Variant::BOOL || object.get("open", Variant()).get_type() != Variant::BOOL) {
			return false;
		}
	}
	double time = data["elapsed"];
	int stage = data.get("mission", -1);
	if (time < 0 || time > 36000 || stage < 0 || stage > 2) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	bool key = data.get("key", false), radio = data.get("radio", false);
	if ((stage > 0 && !key) || (stage == 2 && !radio) || (stage == 0 && key) || (stage < 2 && radio)) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	if (data.get("actors", Variant()).get_type() != Variant::ARRAY || data.get("items", Variant()).get_type() != Variant::ARRAY) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	Array states = data["actors"], loot = data["items"];
	if (states.size() != int(actors.size()) || loot.size() != int(items.size())) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	std::mt19937 candidate_rng;
	String random_text = data.get("rng_state", String());
	std::istringstream random_state{ std::string(random_text.utf8().get_data()) };
	if (!(random_state >> candidate_rng)) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	if (!number(data.get("ai_clock", Variant())) || double(data["ai_clock"]) < 0 || double(data["ai_clock"]) > .25) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	if (!number(data.get("active_bots", Variant())) || int(data["active_bots"]) < 0 || int(data["active_bots"]) > (arctic ? 8 : 4)) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	if (double(data["gate_lift"]) < 0 || double(data["gate_lift"]) > 3 || double(data["gate_hold"]) < 0 || double(data["gate_hold"]) > 2 || data.get("gate_opening", Variant()).get_type() != Variant::BOOL) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	for (String field : { String("key"), String("radio"), String("gate"), String("victory"), String("defeat"), String("paused") }) {
		if (data.get(field, Variant()).get_type() != Variant::BOOL) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
	}
	for (String field : { String("gate_operator_a"), String("gate_operator_b"), String("event_sequence"), String("cooperative_uses"), String("harmful_attacks"), String("shots_hit"), String("wall_blocks"), String("revivals"), String("pickups") }) {
		if (!number(data.get(field, Variant())) || Math::floor(double(data[field])) != double(data[field]) || int64_t(data[field]) < 0 || int64_t(data[field]) > 10000000) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
	}
	if (data.get("journal", Variant()).get_type() != Variant::ARRAY || Array(data["journal"]).size() > 2000) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	const bool candidate_open = data["gate"], candidate_opening = data["gate_opening"];
	const double candidate_lift = data["gate_lift"];
	if ((candidate_open && (candidate_lift != 3 || candidate_opening)) || (!candidate_open && !candidate_opening && candidate_lift != 0) || (candidate_opening && candidate_lift >= 3)) {
		print_line("Save validation rejected field check " + itos(__LINE__));
		return false;
	}
	auto candidate = actors;
	std::set<int> known;
	for (int n = 0; n < states.size(); ++n) {
		if (states[n].get_type() != Variant::DICTIONARY) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		Dictionary d = states[n];
		auto &a = candidate[n];
		auto &s = a.stats;
		auto &i = s.inventory;
		if (int(d.get("id", -1)) != s.id) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		for (String field : { String("health"), String("state"), String("stamina"), String("hunger"), String("hunger_clock"), String("timer"), String("cooldown"), String("bleed"), String("rounds"), String("reserve"), String("medkits"), String("food"), String("yaw") }) {
			if (!number(d.get(field, Variant()))) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		for (String field : { String("health"), String("state"), String("rounds"), String("reserve"), String("medkits"), String("food") }) {
			double n = d[field];
			if (n < -100000 || n > 100000 || Math::floor(n) != n) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		for (String field : { String("stamina"), String("hunger"), String("hunger_clock"), String("timer"), String("cooldown"), String("bleed"), String("yaw") }) {
			if (Math::abs(double(d[field])) > 100000) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		for (String field : { String("pistol"), String("sword"), String("use_sword") }) {
			if (d.get(field, Variant()).get_type() != Variant::BOOL) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		for (String field : { String("travelled"), String("arrived_health"), String("arrived_time") }) {
			if (!number(d.get(field, Variant())) || double(d[field]) < 0 || double(d[field]) > 100000) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		if (!number(d.get("bot_role", Variant())) || int(d["bot_role"]) < 0 || int(d["bot_role"]) > 4 || d.get("melee_pending", Variant()).get_type() != Variant::BOOL) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		a.bot_role = d["bot_role"];
		a.melee_pending = d["melee_pending"];
		a.travelled = d["travelled"];
		a.arrived_health = d["arrived_health"];
		a.arrived_time = d["arrived_time"];
		a.visible_actors.clear();
		a.visible_items.clear();
		s.health = int(d["health"]);
		s.state = lastlight::State(int(d["state"]));
		s.stamina = d["stamina"];
		s.hunger = d["hunger"];
		s.hunger_clock = d["hunger_clock"];
		s.timer = d["timer"];
		s.cooldown = d["cooldown"];
		s.bleed = d["bleed"];
		s.pending_target = d.get("pending", 0);
		i.pistol = d.get("pistol", false);
		i.sword = d.get("sword", false);
		i.rounds = d["rounds"];
		i.reserve = d["reserve"];
		i.medkits = d["medkits"];
		i.food = d["food"];
		a.yaw = d["yaw"];
		a.use_sword = d.get("use_sword", false);
		a.aiming = d.get("aiming", false);
		a.crouching = d.get("crouching", false);
		a.sprint = d.get("sprint", false);
		a.interacting = d.get("interacting", false);
		s.shots = d.get("shots", 0);
		s.damage_received = d.get("damage_received", 0);
		s.revives = d.get("revives", 0);
		s.pickups = d.get("pickups", 0);
		if (d.get("equipment", Variant()).get_type() != Variant::DICTIONARY) {
			return false;
		}
		Dictionary equipment = d["equipment"];
		const char *names[] = { "weapon_kind", "owned_weapons", "bandages", "water", "fuel", "smoke", "decoys", "armour", "backpack", "tools", "coat" };
		int *fields[] = { &i.weapon_kind, &i.owned_weapons, &i.bandages, &i.water, &i.fuel, &i.smoke, &i.decoys, &i.armour, &i.backpack, &i.tools, &i.coat };
		const int limits[] = { 3, 15, 5, 4, 2, 3, 3, 100, 1, 1, 1 };
		for (int field = 0; field < 11; ++field) {
			Variant value = equipment.get(names[field], Variant());
			if (!number(value) || double(value) < 0 || double(value) > limits[field] || Math::floor(double(value)) != double(value)) {
				return false;
			}
			*fields[field] = int(value);
		}
		for (String field : { String("hydration"), String("cold") }) {
			if (!number(equipment.get(field, Variant())) || double(equipment[field]) < 0 || double(equipment[field]) > 100) {
				return false;
			}
		}
		i.hydration = equipment["hydration"];
		i.cold = equipment["cold"];
		Array magazines = equipment.get("magazines", Array()), ammunition = equipment.get("ammunition", Array());
		if (magazines.size() != 4 || ammunition.size() != 4) {
			return false;
		}
		for (int k = 0; k < 4; ++k) {
			if (!number(magazines[k]) || !number(ammunition[k]) || double(magazines[k]) < 0 || double(magazines[k]) > lastlight::weapon_spec(k).magazine || double(ammunition[k]) < 0 || double(ammunition[k]) > 120) {
				return false;
			}
			i.magazines[k] = magazines[k];
			i.ammunition[k] = ammunition[k];
		}
		if (d.get("environment_action", Variant()).get_type() != Variant::STRING || String(d["environment_action"]).length() > 32 || !number(d.get("environment_target", Variant()))) {
			return false;
		}
		if (!number(d.get("teammate", Variant())) || int(d["teammate"]) < 0 || int(d["teammate"]) > 5 || int(d["teammate"]) == s.id) {
			return false;
		}
		a.teammate_id = d["teammate"];
		a.environment_action = d["environment_action"];
		a.environment_target = d["environment_target"];
		a.plan.steps.clear();
		a.alternatives.clear();
		a.plan_step = 0;
		a.next_plan = 0;
		a.reposition_until = 0;
		a.known_items.clear();
		a.known_item_kinds.clear();
		if (d.get("known_items", Variant()).get_type() != Variant::ARRAY) {
			return false;
		}
		Array knowledge = d["known_items"];
		if (knowledge.size() > 256) {
			return false;
		}
		for (int k = 0; k < knowledge.size(); ++k) {
			if (knowledge[k].get_type() != Variant::DICTIONARY) {
				return false;
			}
			Dictionary known = knowledge[k];
			Vector3 kp;
			int id = known.get("id", 0);
			if (!vector_read(known.get("position", Variant()), kp) || id < 101 || id >= 101 + int(items.size()) || known.get("kind", Variant()).get_type() != Variant::STRING) {
				return false;
			}
			a.known_items[id] = kp;
			a.known_item_kinds[id] = known["kind"];
		}
		if (!lastlight::valid(s) || ((s.state == lastlight::State::Dead || s.state == lastlight::State::Downed) != (s.health == 0)) || s.shots < 0 || s.damage_received < 0 || s.revives < 0 || s.pickups < 0) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		Vector3 p, v, t;
		if (!vector_read(d.get("position", Variant()), p) || !vector_read(d.get("velocity", Variant()), v) || !vector_read(d.get("target", Variant()), t) || p.x < 1 || p.x > world_extent - 1 || p.z < 1 || p.z > (arctic ? 299 : 31) || p.y < (arctic ? -2 : -.5) || p.y > 15) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		int x = int(Math::round(p.x / 2)), z = int(Math::round(p.z / 2));
		if (!arctic && z >= 0 && z < int(map_rows.size()) && x >= 0 && x < map_rows[z].length() && map_rows[z][x] == '#') {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		Array blend = d.get("blend", Array());
		if (blend.size() != 11) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		for (int b = 0; b < blend.size(); ++b) {
			if (!number(blend[b]) || double(blend[b]) < 0 || double(blend[b]) > 1) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
		}
		if (Math::abs(double(blend[0]) + double(blend[1]) + double(blend[2]) + double(blend[3]) - 1) > .001) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		a.blend.idle = blend[0];
		a.blend.walk = blend[1];
		a.blend.run = blend[2];
		a.blend.crouch = blend[3];
		a.blend.aim = blend[4];
		a.blend.action = blend[5];
		a.blend.phase = blend[6];
		a.blend.forward = blend[7];
		a.blend.backward = blend[8];
		a.blend.left = blend[9];
		a.blend.right = blend[10];
		a.memory.clear();
		Array beliefs = d.get("memory", Array());
		if (beliefs.size() > 32) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		for (int b = 0; b < beliefs.size(); ++b) {
			if (beliefs[b].get_type() != Variant::DICTIONARY) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
			Dictionary v = beliefs[b];
			int id = v.get("id", -1);
			if (!find_actor(id)) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
			lastlight::Belief belief;
			for (String name : { String("trust"), String("threat"), String("last_seen"), String("last_attack") }) {
				if (!number(v.get(name, Variant()))) {
					print_line("Save validation rejected field check " + itos(__LINE__));
					return false;
				}
			}
			belief.trust = v["trust"];
			belief.threat = v["threat"];
			belief.last_seen = v["last_seen"];
			belief.last_attack = v["last_attack"];
			if (belief.trust < 0 || belief.trust > 1 || belief.threat < 0 || belief.threat > 1 || belief.last_seen > time) {
				print_line("Save validation rejected field check " + itos(__LINE__));
				return false;
			}
			a.memory[id] = belief;
		}
		if (int(d.get("profile_revision", -1)) != a.personality.revision) {
			a.memory.clear();
			a.teammate_id = 0;
			a.known_items.clear();
			a.known_item_kinds.clear();
		}
	}
	for (int n = 0; n < loot.size(); ++n) {
		if (loot[n].get_type() != Variant::DICTIONARY) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
		Dictionary d = loot[n];
		if (int(d.get("id", -1)) != items[n].id || d.get("available", Variant()).get_type() != Variant::BOOL) {
			print_line("Save validation rejected field check " + itos(__LINE__));
			return false;
		}
	}
	if (compound_world && !validate_compound_save(data, candidate, time)) {
		return false;
	}
	// Commit only after every field and actor has passed validation.
	actors = candidate;
	rng = candidate_rng;
	ai_clock = data["ai_clock"];
	camera_focus = CLAMP(int(data.get("camera_focus", 1)), 1, 5);
	for (int n = 0; n < states.size(); ++n) {
		Dictionary d = states[n];
		Vector3 p, v, t;
		vector_read(d["position"], p);
		vector_read(d["velocity"], v);
		vector_read(d["target"], t);
		actors[n].body->set_position(p);
		actors[n].body->set_velocity(v);
		if (compound_world) {
			actors[n].body->set_collision_layer(actors[n].stats.active() || actors[n].stats.state == lastlight::State::Downed ? 2 : 0);
		}
		actors[n].target = t;
		actors[n].nav->set_target_position(t);
		actors[n].visual->set_visible(actors[n].stats.state != lastlight::State::Extracted);
		actors[n].body->set_visible(actors[n].stats.survivor || actors[n].stats.health > 0);
	}
	session_id = data["session_id"];
	power_on = data["power_on"];
	for (int j = 0; j < objects.size(); ++j) {
		Dictionary object = objects[j];
		world_objects[j].used = object["used"];
		bool opened = object["open"];
		world_objects[j].open = opened;
		if (world_objects[j].kind == "door" && world_objects[j].body) {
			world_objects[j].body->set_collision_layer(opened ? 0 : 1);
			world_objects[j].value = opened ? 1 : 0;
			world_objects[j].body->set_rotation(Vector3(0, opened ? 1.45f : 0.f, 0));
		}
	}
	elapsed = time;
	mission = stage;
	key_found = key;
	radio_done = radio;
	gate_open = data.get("gate", false);
	gate_hold = data["gate_hold"];
	gate_lift = data["gate_lift"];
	gate_opening = data["gate_opening"];
	gate_operator_a = data["gate_operator_a"];
	gate_operator_b = data["gate_operator_b"];
	camera_yaw = data["camera_yaw"];
	camera_pitch = data["camera_pitch"];
	victory = data.get("victory", false);
	defeat = data.get("defeat", false);
	paused = data.get("paused", false);
	shots_hit = data.get("shots_hit", 0);
	wall_blocks = data.get("wall_blocks", 0);
	revivals = data.get("revivals", 0);
	pickups = data.get("pickups", 0);
	journal = data.get("journal", Array());
	spawn_count = data["active_bots"];
	event_sequence = data.get("event_sequence", 0);
	cooperative_uses = data.get("cooperative_uses", 0);
	harmful_attacks = data.get("harmful_attacks", 0);
	for (int n = 0; n < loot.size(); ++n) {
		Dictionary d = loot[n];
		items[n].available = d["available"];
		items[n].visual->set_visible(items[n].available);
	}
	gate->set_position(gate_closed_position + Vector3(0, gate_lift, 0));
	gate->set_collision_layer(1);
	gate->set_collision_mask(2);
	gate_mesh->set_visible(!gate_model && !gate_open);
	region->bake_navigation_mesh(false);
	if (compound_world) {
		restore_compound(data);
		compound_paths_ready = false;
	}
	return true;
}
bool LastLightGame::save_game() {
	Dictionary data = snapshot();
	String temp = save_path + ".tmp";
	auto file = FileAccess::open(temp, FileAccess::WRITE);
	if (file.is_null()) {
		notify("Save failed: cannot open user data directory");
		return false;
	}
	file->store_string(JSON::stringify(data, "", true, true));
	file->flush();
	file.unref();
	String destination = ProjectSettings::get_singleton()->globalize_path(save_path);
	String backup = destination + ".bak";
	DirAccess::remove_absolute(backup);
	if (FileAccess::exists(save_path)) {
		DirAccess::rename_absolute(destination, backup);
	}
	if (DirAccess::rename_absolute(ProjectSettings::get_singleton()->globalize_path(temp), destination) != OK) {
		DirAccess::rename_absolute(backup, destination);
		notify("Save failed; previous save retained");
		return false;
	} else {
		DirAccess::remove_absolute(backup);
		notify("Game saved (F9 to load)");
	}
	return true;
}
bool LastLightGame::load_game() {
	auto file = FileAccess::open(save_path, FileAccess::READ);
	if (file.is_null() || file->get_length() > 1024 * 1024) {
		notify("No valid save found");
		return false;
	}
	Variant value = JSON::parse_string(file->get_as_text());
	if (value.get_type() != Variant::DICTIONARY || !restore(value)) {
		notify("Save rejected; current game unchanged");
		return false;
	}
	if (!python_executable.is_empty()) {
		stop_python();
		start_python();
	}
	notify("Game restored");
	return true;
}
void LastLightGame::finish_report() {
	if (reported) {
		return;
	}
	reported = true;
	Dictionary report = snapshot();
	report["phase"] = victory ? "Victory" : defeat ? "Defeat"
												   : "Incomplete";
	report["physics_frames"] = physics_frames;
	report["navigation_polygons"] = region->get_navigation_mesh()->get_polygon_count();
	report["placeholder_visuals"] = !actors[0].skeleton;
	report["skeletal_actors"] = actors[0].skeleton != nullptr;
	report["environment_art"] = gate_model != nullptr;
	report["world"] = compound_world ? "frostworks" : arctic ? "arctic"
															 : "lab";
	report["loot_total"] = int(items.size());
	report["interactions"] = int(world_objects.size());
	Array survivor_profiles;
	for (const auto &actor : actors) {
		if (actor.stats.id >= 2 && actor.stats.id <= 5) {
			survivor_profiles.push_back(profile_info(actor));
		}
	}
	report["profiles"] = survivor_profiles;
	report["godot_version"] = "4.5.1-stable";
	if (script_case == "physics") {
		auto p = find_actor(1);
		integration_ok = integration_ok && p->body->get_position().x >= 1.25 && p->body->get_position().x < 1.5 && p->body->is_on_floor();
	}
	if (script_case == "combat") {
		integration_ok = integration_ok && wall_blocks >= 1 && shots_hit >= 1 && find_actor(1)->stats.inventory.rounds == 6;
	}
	if (script_case == "gate" || script_case == "gate_save") {
		integration_ok = integration_ok && gate_open;
	}
	report["test_case"] = script_case;
	report["test_passed"] = integration_ok;
	int arrived = 0, alive = 0;
	for (auto &a : actors) {
		if (a.stats.survivor) {
			if (a.stats.state == lastlight::State::Extracted) {
				++arrived;
			}
			if (a.stats.health > 0) {
				++alive;
			}
		}
	}
	report["arrived"] = arrived;
	report["alive"] = alive;
	report["fitness"] = lastlight::fitness(arrived, alive, elapsed, harmful_attacks);
	report["cooperative_uses"] = cooperative_uses;
	report["harmful_attacks"] = harmful_attacks;
	double path_total = 0, arrival_health = 0, damage_total = 0;
	for (auto &actor : actors) {
		if (actor.stats.survivor) {
			path_total += actor.travelled;
			arrival_health += actor.arrived_health;
			damage_total += actor.stats.damage_received;
		}
	}
	report["path_cost"] = path_total;
	report["health_at_arrival"] = arrived ? arrival_health / arrived : 0.;
	report["damage_received"] = damage_total;
	String output = JSON::stringify(report, "  ");
	if (!report_path.is_empty()) {
		DirAccess::make_dir_recursive_absolute(report_path.get_base_dir());
		auto f = FileAccess::open(report_path, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_string(output);
		}
	}
	print_line(output);
	get_tree()->quit(((victory || smoke) && integration_ok) ? 0 : 2);
}
