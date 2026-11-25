#include "core/config/engine.h"
#include "core/input/input.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "last_light_game.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/navigation_mesh.h"
#include <cmath>

namespace {
const char *names[] = { "You", "Maya", "Rafi", "Lian", "Omar" };
float distance_flat(Vector3 a, Vector3 b) {
	a.y = 0;
	b.y = 0;
	return a.distance_to(b);
}
} //namespace

void LastLightGame::configure_bots(int count) {
	spawn_count = CLAMP(count, 0, arctic ? 8 : 4);
	for (auto &a : actors) {
		if (a.stats.survivor) {
			continue;
		}
		const bool active = a.stats.id - 10 < spawn_count;
		a.stats.health = active ? 100 : 0;
		a.stats.state = active ? lastlight::State::Idle : lastlight::State::Dead;
		a.stats.timer = a.stats.cooldown = a.stats.bleed = 0;
		a.stats.inventory.pistol = arctic ? (a.bot_role == 0 || a.bot_role == 4) : a.bot_role == 0;
		a.stats.inventory.rounds = a.stats.inventory.pistol ? lastlight::weapon_spec(a.stats.inventory.weapon_kind).magazine : 0;
		if (arctic) {
			a.stats.inventory.reserve = a.stats.inventory.pistol ? 30 : 0;
			a.stats.inventory.owned_weapons = a.stats.inventory.pistol ? (1 << a.stats.inventory.weapon_kind) : 0;
			a.stats.inventory.decoys = a.bot_role == 3 ? 3 : 0;
		}
		if (!arctic) {
			a.stats.inventory.reserve = 0;
		}
		a.body->set_visible(active);
		a.body->set_collision_layer(active ? 2 : 0);
	}
}

void LastLightGame::sense(Actor &a) {
	if (compound_world) {
		perceive_compound(a);
		return;
	}
	for (auto &[id, belief] : a.memory) {
		if (elapsed - belief.last_attack > 6) {
			belief.threat *= .985f;
		}
	}
	a.visible_actors.clear();
	a.visible_items.clear();
	if (!a.stats.active()) {
		return;
	}
	if (arctic) {
		for (auto &object : world_objects) {
			if (object.kind == "crate" && object.used && distance_flat(a.body->get_position(), object.position) < 14 && can_see(a.body->get_position() + Vector3(0, 1.2, 0), object.position + Vector3(0, 1, 0))) {
				a.known_empty_crates.insert(object.id);
			}
		}
	}
	for (auto &other : actors) {
		if (visible(a, other)) {
			a.visible_actors.insert(other.stats.id);
			a.memory[other.stats.id].last_seen = elapsed;
		}
	}
	for (auto &item : items) {
		if (item.available && distance_flat(a.body->get_position(), item.position) < 14 &&
				can_see(a.body->get_position() + Vector3(0, 1.2, 0), item.position)) {
			a.visible_items.insert(item.id);
			if (arctic) {
				a.known_items[item.id] = item.position;
				a.known_item_kinds[item.id] = item.kind;
			}
		}
	}
}

void LastLightGame::attach_frontend() {
	frontend_attached = true;
	for (auto &a : actors) {
		if (a.animation_tree) {
			update_animation_graph(a, 1.f / 60, Vector3());
		}
	}
	if (hud) {
		hud->hide();
	}
	if (message_label) {
		message_label->hide();
	}
	if (legacy_crosshair) {
		legacy_crosshair->hide();
	}
	if (!automated_run) {
		session_started = false;
		paused = true;
		frontend_mode = "menu";
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
}

void LastLightGame::apply_options(const Dictionary &options) {
	Variant volume = options.get("volume", master_volume);
	if ((volume.get_type() == Variant::FLOAT || volume.get_type() == Variant::INT) && std::isfinite(double(volume))) {
		master_volume = CLAMP(double(volume), 0., 1.);
	}
	Variant input = options.get("sensitivity", mouse_sensitivity);
	if (input.get_type() != Variant::FLOAT && input.get_type() != Variant::INT) {
		return;
	}
	const double sensitivity = input;
	if (std::isfinite(sensitivity)) {
		mouse_sensitivity = CLAMP(sensitivity, .0005, .01);
	}
	if (options.get("invert_y", Variant()).get_type() == Variant::BOOL) {
		invert_mouse = options["invert_y"];
	}
}

bool LastLightGame::game_command(const String &command, const Dictionary &payload) {
	if (command == "inventory") {
		if (!compound_world || !session_started || paused || victory || defeat) {
			return false;
		}
		frontend_mode = frontend_mode == "inventory" ? "playing" : "inventory";
		Input::get_singleton()->set_mouse_mode(frontend_mode == "playing" && !overview ? Input::MOUSE_MODE_CAPTURED : Input::MOUSE_MODE_VISIBLE);
		return true;
	}
	if (command == "use_item" || command == "discard_item") {
		auto player = find_actor(1);
		if (!compound_world || frontend_mode != "inventory" || paused || !player || !player->stats.active() || player->stats.busy()) {
			return false;
		}
		String kind = payload.get("kind", "");
		auto &i = player->stats.inventory;
		if (command == "use_item") {
			if (kind == "medkit") {
				return lastlight::begin(player->stats, lastlight::State::Heal);
			}
			if (kind != "bandage" && kind != "water" && kind != "food" && kind != "smoke" && kind != "decoy") {
				return false;
			}
			use_equipment(*player, kind);
			return player->stats.busy();
		}
		int *count = kind == "medkit" ? &i.medkits : kind == "bandage" ? &i.bandages
				: kind == "food"									   ? &i.food
				: kind == "water"									   ? &i.water
				: kind == "fuel"									   ? &i.fuel
				: kind == "smoke"									   ? &i.smoke
				: kind == "decoy"									   ? &i.decoys
																	   : nullptr;
		if (count && *count > 0) {
			--*count;
			notify("Discarded one " + kind + " to free carry capacity");
			return true;
		}
		if (kind == "ammo" && i.reserve > 0) {
			int removed = MIN(12, i.reserve);
			i.reserve -= removed;
			notify("Discarded " + itos(removed) + " compatible reserve rounds");
			return true;
		}
		return false;
	}
	if (command == "squad") {
		if (!compound_world || !session_started || paused || victory || defeat) {
			return false;
		}
		frontend_mode = frontend_mode == "squad" ? "playing" : "squad";
		camera_focus = 1;
		Input::get_singleton()->set_mouse_mode(frontend_mode == "playing" && !overview ? Input::MOUSE_MODE_CAPTURED : Input::MOUSE_MODE_VISIBLE);
		return true;
	}
	if (command == "order") {
		Variant id = payload.get("id", 0), order = payload.get("order", -1);
		if (id.get_type() != Variant::INT || order.get_type() != Variant::INT || paused) {
			return false;
		}
		return issue_order(id, order);
	}
	if (command == "inspect_actor") {
		int id = payload.get("id", 2);
		if (id < 2 || id > 5) {
			return false;
		}
		camera_focus = id;
		return true;
	}
	if (command == "reset_profile") {
		Variant id = payload.get("id", 0);
		if (id.get_type() != Variant::INT) {
			return false;
		}
		return reset_profile(int(id));
	}
	if (command == "reset_all_profiles") {
		bool ok = true;
		for (int id = 2; id <= 5; ++id) {
			ok = reset_profile(id) && ok;
		}
		return ok;
	}
	if (command == "map") {
		if (!session_started || paused || victory || defeat) {
			return false;
		}
		frontend_mode = frontend_mode == "map" ? "playing" : "map";
		camera_focus = 1;
		Input::get_singleton()->set_mouse_mode(frontend_mode == "map" || overview ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_CAPTURED);
		return true;
	}
	if (command == "inspect") {
		if (!session_started || victory || defeat || paused) {
			return false;
		}
		frontend_mode = frontend_mode == "inspect" ? "playing" : "inspect";
		camera_focus = frontend_mode == "inspect" ? inspector_actor : 1;
		Input::get_singleton()->set_mouse_mode(frontend_mode == "inspect" || overview ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_CAPTURED);
		return true;
	}

	if (command == "quit") {
		get_tree()->quit();
		return true;
	}
	if (command == "start" && !session_started) {
		Variant count = payload.get("bots", spawn_count);
		if (count.get_type() != Variant::INT || int(count) < 0 || int(count) > (arctic ? 8 : 4)) {
			return false;
		}
		configure_bots(int(count));
		session_started = true;
		paused = false;
		frontend_mode = "playing";
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
		notify(arctic ? "Find the access key, restore power and two relays, then the radio. M opens the base map; I inspects survivor learning." : "Recover the yard key. Helping survivors is optional; every survivor has a solo route.");
		return true;
	}
	if (command == "load") {
		if (!load_game()) {
			return false;
		}
		session_started = true;
		paused = false;
		frontend_mode = "playing";
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
		return true;
	}
	if (command == "menu" || command == "restart") {
		restart();
		return true;
	}
	if (!session_started || victory || defeat) {
		return false;
	}
	if (command == "pause" || command == "resume") {
		paused = command == "pause";
		frontend_mode = paused ? "pause" : "playing";
		Input::get_singleton()->set_mouse_mode(paused || overview ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_CAPTURED);
		return true;
	}
	if (command == "save") {
		return save_game();
	}
	if (command == "autoplay") {
		autoplay = !autoplay;
		return true;
	}
	if (paused) {
		return false;
	}
	auto player = find_actor(1);
	if (!player || !player->stats.active()) {
		return false;
	}
	if (command == "reload") {
		return lastlight::begin(player->stats, lastlight::State::Reload);
	}
	if (command == "heal") {
		return lastlight::begin(player->stats, lastlight::State::Heal);
	}
	if (command == "cancel") {
		lastlight::cancel(player->stats);
		return true;
	}
	return false;
}

Vector3 LastLightGame::mission_target_for(const Actor &actor) const {
	if (mission == 0) {
		return key_position;
	}
	if (mission == 2) {
		if (arctic && distance_flat(actor.body->get_position(), alternative_exit) < distance_flat(actor.body->get_position(), exit_position)) {
			return alternative_exit;
		}
		return exit_position;
	}
	if (arctic) {
		if (!power_on) {
			return generator_position;
		}
		int count = 0;
		for (auto &object : world_objects) {
			if (object.kind == "relay" && object.used) {
				++count;
			}
		}
		if (count < 2) {
			Vector3 result = radio_position;
			float nearest = 1e9;
			for (auto &object : world_objects) {
				if (object.kind == "relay" && !object.used) {
					float d = distance_flat(actor.body->get_position(), object.position);
					if (d < nearest) {
						nearest = d;
						result = object.position;
					}
				}
			}
			return result;
		}
	}
	return radio_position;
}
Dictionary LastLightGame::get_game_state() const {
	Dictionary out;
	const Actor *p = find_actor(1);
	if (!p) {
		return out;
	}
	out["mode"] = victory || defeat ? "results" : frontend_mode;
	out["started"] = session_started;
	out["paused"] = paused;
	out["victory"] = victory;
	out["defeat"] = defeat;
	out["mission"] = mission;
	out["elapsed"] = elapsed;
	out["deadline"] = config.get("deadline_seconds", 600);
	out["health"] = p->stats.health;
	out["stamina"] = p->stats.stamina;
	out["hunger"] = p->stats.hunger;
	out["medkits"] = p->stats.inventory.medkits;
	out["food"] = p->stats.inventory.food;
	out["rounds"] = p->stats.inventory.rounds;
	out["reserve"] = p->stats.inventory.reserve;
	out["weapon"] = p->use_sword && p->stats.inventory.sword ? "SWORD" : p->stats.inventory.pistol && !p->use_sword ? String(lastlight::weapon_spec(p->stats.inventory.weapon_kind).name).to_upper()
																													: "UNARMED";
	out["player_state"] = p->stats.state == lastlight::State::Interact ? p->environment_action : String(lastlight::state_name(p->stats.state));
	out["action_timer"] = p->stats.timer;
	out["arctic"] = arctic;
	out["extent"] = world_extent;
	out["power"] = power_on;
	int relays = 0;
	for (auto &object : world_objects) {
		if (object.kind == "relay" && object.used) {
			++relays;
		}
	}
	out["relays"] = relays;
	out["hydration"] = p->stats.inventory.hydration;
	out["cold"] = p->stats.inventory.cold;
	out["armour"] = p->stats.inventory.armour;
	out["weight"] = p->stats.inventory.weight();
	out["capacity"] = p->stats.inventory.capacity();
	out["bandages"] = p->stats.inventory.bandages;
	out["water_bottles"] = p->stats.inventory.water;
	out["smoke"] = p->stats.inventory.smoke;
	out["decoys"] = p->stats.inventory.decoys;
	out["fuel"] = p->stats.inventory.fuel;
	Array profiles;
	for (auto &actor : actors) {
		if (actor.stats.id >= 2 && actor.stats.id <= 5) {
			profiles.push_back(profile_info(actor));
		}
	}
	out["profiles"] = profiles;
	out["compound"] = compound_world;
	out["overview"] = overview;
	out["suppression"] = p->suppression;
	out["cover_points"] = int(cover_points.size());
	out["draw_calls"] = int(RenderingServer::get_singleton()->get_rendering_info(RenderingServer::RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME));
	Array landings;
	for (int n = 0; n < 2; ++n) {
		Dictionary site;
		site["active"] = beacon_active[n];
		site["remaining"] = MAX(0., beacon_times[n] - elapsed);
		site["contested"] = beacon_contested[n];
		landings.push_back(site);
	}
	out["landings"] = landings;

	out["message"] = message_time > 0 ? message : String();
	out["hint"] = interaction_hint;
	out["debug"] = debug_overlay;
	neural_status(out);
	out["autoplay"] = autoplay;
	out["python"] = python_ready;
	out["python_failures"] = python_failures;
	out["save_available"] = FileAccess::exists(save_path);
	out["bots"] = spawn_count;
	out["gate_open"] = gate_open;
	out["gate_hold"] = CLAMP(gate_hold, 0., 1.);
	out["hit_confirm"] = elapsed - p->hit_feedback_time < .18;
	out["shots_hit"] = shots_hit;
	out["revives"] = revivals;
	out["pickups"] = pickups;
	out["fps"] = Engine::get_singleton()->get_frames_per_second();
	out["navigation_polygons"] = region && region->get_navigation_mesh().is_valid() ? region->get_navigation_mesh()->get_polygon_count() : 0;
	Vector3 destination = mission_target_for(*p);
	out["objective_distance"] = distance_flat(p->body->get_position(), destination);
	out["player_position"] = Vector2(p->body->get_position().x, p->body->get_position().z);
	out["objective_position"] = Vector2(destination.x, destination.z);
	out["map_rows"] = config.get("map", Array());
	Array landmarks;
	for (int i = 0; i < int(strategic_nodes.size()); ++i) {
		Dictionary landmark;
		landmark["name"] = String(strategic_nodes[i].name.c_str());
		landmark["position"] = Vector2(strategic_nodes[i].position.x, strategic_nodes[i].position.z);
		landmarks.push_back(landmark);
	}
	out["landmarks"] = landmarks;
	out["camera_yaw"] = camera_yaw;
	Array structures;
	for (auto &roof : roofs) {
		Dictionary b;
		b["position"] = Vector2(roof.center.x, roof.center.z);
		b["size"] = Vector2(roof.size.x, roof.size.z);
		structures.push_back(b);
	}
	out["structures"] = structures;
	Array contacts;
	for (auto &[id, c] : p->contacts) {
		if (id <= 5 || c.confidence < .15f) {
			continue;
		}
		Dictionary info;
		info["position"] = Vector2(c.position.x, c.position.z);
		info["hostile"] = c.hostile;
		info["confidence"] = c.confidence;
		contacts.push_back(info);
	}
	out["contacts"] = contacts;
	out["cover"] = p->cover_manual;
	Array route;
	for (auto point : p->nav->get_current_navigation_path()) {
		route.push_back(Vector2(point.x, point.z));
	}
	if (autoplay) {
		out["route"] = route;
	}
	Array team, seen_items, events;
	int arrived = 0, alive = 0;
	for (auto &a : actors) {
		if (!a.stats.survivor) {
			continue;
		}
		bool known = a.stats.id == 1 || radio_done || p->visible_actors.count(a.stats.id);
		Dictionary row;
		row["id"] = a.stats.id;
		row["name"] = names[a.stats.id - 1];
		row["known"] = known;
		row["health"] = known ? a.stats.health : -1;
		row["state"] = known ? String(lastlight::state_name(a.stats.state)) : "Out of sight";
		row["reason"] = known ? a.reason : "No recent contact";
		row["position"] = known ? Vector2(a.body->get_position().x, a.body->get_position().z) : Vector2(-100, -100);
		auto memory = p->memory.find(a.stats.id);
		row["trust"] = memory == p->memory.end() ? 0.f : memory->second.trust;
		row["threat"] = memory == p->memory.end() ? 0.f : memory->second.threat;
		if (a.stats.state == lastlight::State::Extracted) {
			++arrived;
		}
		if (a.stats.health > 0) {
			++alive;
		}
		team.push_back(row);
	}
	for (auto &item : items) {
		if (item.available && p->visible_items.count(item.id)) {
			Dictionary row;
			row["kind"] = item.kind;
			row["position"] = Vector2(item.position.x, item.position.z);
			seen_items.push_back(row);
		}
	}
	for (int i = MAX(0, journal.size() - 6); i < journal.size(); ++i) {
		events.push_back(journal[i]);
	}
	out["team"] = team;
	out["items"] = seen_items;
	out["events"] = events;
	out["arrived"] = arrived;
	out["alive"] = alive;
	return out;
}
