#include "last_light_game.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/navigation_server_3d.h"
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
} //namespace
void LastLightGame::compound_integration_step() {
	using namespace lastlight;
	auto player = find_actor(1), peer = find_actor(2);
	static int stage = 0;
	if (physics_frames == 4 && script_case.begins_with("compound_") && script_case != "compound_hostile") {
		for (auto &a : actors) {
			a.target = a.body->get_position();
			a.nav->set_target_position(a.target);
			a.move = Vector3();
			a.interacting = false;
		}
	}
    if(script_case=="compound_animation"){
        static Quaternion leg_pose;
        if(physics_frames==8){player->target=Vector3(130,0,192);player->nav->set_target_position(player->target);integration_ok=integration_ok&&player->skeleton&&player->animation_tree&&player->pack_model&&player->vest_model;}
        if(player->skeleton){int leg=player->skeleton->find_bone("thigh_l");if(physics_frames==25&&leg>=0)leg_pose=player->skeleton->get_bone_pose_rotation(leg);if(physics_frames==45){integration_ok=integration_ok&&leg>=0&&leg_pose.angle_to(player->skeleton->get_bone_pose_rotation(leg))>.03f;}if(physics_frames==60)player->crouching=true;if(physics_frames==85)integration_ok=integration_ok&&player->stance_weight>.8f;}
    }
	if (script_case == "compound_neural_training" && physics_frames >= 120 &&
			physics_frames < 120 + 24 * 54 && (physics_frames - 120) % 24 == 0) {
		// Exercise controlled perception and equipment states on actual terrain.
		// In particular, place two discoverable casualties at different positions.
		const int scenario = (physics_frames - 120) / 24;
		auto &a = *find_actor(4);
		auto &enemy = *find_actor(10);
		const Vector3 sites[] = { { 160, .1, 230 }, { 127, .1, 195 }, { 110, .1, 152 }, { 176, .1, 156 }, { 148, .1, 171 } };
		const bool supply_trial = scenario >= 30 && scenario < 48;
		const bool landing_trial = scenario >= 48;
		const Vector3 pos = landing_trial ? (scenario % 2 ? exit_position : alternative_exit) :
				supply_trial ? sites[0] : sites[scenario / 6];
		a.body->set_position(pos);
		a.target = pos;
		a.nav->set_target_position(pos);
		a.yaw = Math::PI;
		a.stats.state = State::Idle;
		a.stats.health = scenario % 6 == 0 ? 24 : scenario % 6 == 1 ? 63 : 100;
		a.stats.hunger = scenario % 6 == 0 ? 18 : 90;
		a.stats.stamina = scenario % 6 == 0 ? 20 : 100;
		a.order = scenario < 30 ? 0 : supply_trial ? 3 : 1 + scenario % 4;
		a.order_peer = 2;
		a.order_until = elapsed + (a.order ? 10 + scenario % 30 : 0);
		a.order_position = pos + Vector3(2, 0, 2);
		a.stats.inventory = Inventory();
		a.stats.inventory.pistol = true;
		a.stats.inventory.owned_weapons = 1;
		a.stats.inventory.rounds = scenario % 6 == 1 ? 0 : scenario % 6 == 2 ? 1 : weapon_spec(a.stats.inventory.weapon_kind).magazine;
		a.stats.inventory.hydration = scenario % 6 == 0 ? 22 : 85;
		a.stats.inventory.smoke = scenario % 6 == 2 ? 2 : 0;
		a.stats.inventory.decoys = scenario % 6 == 2 ? 2 : 0;
		a.stats.inventory.cold = scenario % 6 == 0 ? 84 : 0;
		a.stats.inventory.medkits = 2;
		a.stats.inventory.armour = scenario % 6 == 3 ? 65 : 0;
		a.stats.inventory.fuel = scenario % 6 == 3;
		a.stats.inventory.tools = scenario % 6 == 3;
		a.stats.inventory.coat = scenario % 6 == 3;
		a.stats.inventory.backpack = scenario % 6 == 3;
		a.suppression = scenario % 6 == 2 ? .75f : .05f;
		a.last_damage_at = scenario % 6 == 2 ? elapsed - 1 : -100;
		a.next_tactical_scan = 0;
		a.next_investigate = 0;
		a.tactical_until = 0;
		a.contacts.clear();
		const bool combat = landing_trial || (!supply_trial && (scenario % 6 == 2 || scenario % 6 == 3 || scenario % 6 == 5));
		enemy.bot_role = 0;
		enemy.stats.health = combat ? 100 : 0;
		enemy.stats.state = combat ? State::Idle : State::Dead;
		enemy.body->set_position(pos + Vector3(0, 0, landing_trial ? 5 : 14));
		enemy.target = enemy.body->get_position();
		enemy.nav->set_target_position(enemy.target);
		enemy.body->set_collision_layer(combat ? 2 : 0);
		for (int id : { 2, 3 }) {
			auto &ally = *find_actor(id);
			const bool downed = scenario % 6 == 4 || scenario % 6 == 5;
			ally.stats.state = downed ? State::Downed : State::Idle;
			ally.stats.health = downed ? 0 : 100;
			ally.stats.bleed = id == 2 ? 7 : 17;
			ally.body->set_position(pos + Vector3(id == 2 ? -3 : 3, 0, 5));
			ally.nav->set_target_position(ally.body->get_position());
			a.memory[id].trust = scenario % 2 ? .75f : .15f;
		}
		a.noise_danger = false;
		a.last_noise = pos + Vector3(4, 0, 7);
		a.last_noise_at = scenario % 6 == 1 ? elapsed - 1 : -100;
		if (scenario >= 24) {
			key_found = power_on = true;
			radio_done = !supply_trial;
			mission = radio_done ? 2 : 1;
			gate_open = scenario % 2 == 0;
		}
		if (landing_trial) {
			const int landing = scenario % 2 ? 0 : 1;
			beacon_active[landing] = true;
			beacon_times[landing] = elapsed + 20;
		}
		if (supply_trial) {
			// Use real item definitions and make one supply visible on clear ground.
			// An unarmed actor seeks a gun. An armed actor seeks compatible reserve.
			static const char *const supplies[] = { "pistol", "smg", "rifle", "shotgun", "ammo", "smg_ammo", "rifle_ammo", "shells",
				"medkit", "bandage", "food", "water", "fuel", "smoke", "armour", "coat", "backpack", "tools" };
			int supply = scenario - 30;
			auto &inventory = a.stats.inventory;
			inventory.pistol = supply >= 4;
			inventory.weapon_kind = supply >= 4 && supply < 8 ? supply - 4 : 0;
			inventory.owned_weapons = inventory.pistol ? 1 << inventory.weapon_kind : 0;
			inventory.rounds = inventory.reserve = 0;
			inventory.medkits = inventory.bandages = inventory.food = inventory.water = 0;
			inventory.tools = inventory.coat = inventory.backpack = inventory.armour = 0;
			a.known_items.clear();
			a.known_item_kinds.clear();
			for (auto &item : items) {
				if (item.kind == supplies[supply]) {
					item.position = pos + Vector3(0, .3, 3);
					item.visual->set_position(item.position);
					item.available = true;
					item.visual->show();
					break;
				}
			}
		}
		perceive_compound(a);
		a.react_at = elapsed;
		a.next_plan = 0;
		decide_compound(a);
		build_compound_plan(a);
		integration_ok = integration_ok && a.stats.active();
	}
	if (script_case == "compound_security") {
		WorldObject *panel = nullptr;
		for (auto &o : world_objects) {
			if (o.kind == "security") {
				panel = &o;
			}
		}
		if (physics_frames == 20 && panel) {
			power_on = true;
			player->body->set_position(panel->position + Vector3(0, 0, 1.5));
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
			player->stats.inventory.tools = 0;
			player->environment_target = panel->id;
			interact_environment(*player);
			integration_ok = integration_ok && !player->stats.busy() && !panel->used;
			player->stats.inventory.tools = 1;
			interact_environment(*player);
			integration_ok = integration_ok && player->stats.busy();
		}
		if (physics_frames == 280 && panel) {
			integration_ok = integration_ok && panel->used;
			auto saboteur = find_actor(13);
			saboteur->stats.health = 100;
			saboteur->stats.state = State::Idle;
			saboteur->bot_role = 3;
			saboteur->body->set_position(generator_position + Vector3(0, 0, 1));
			saboteur->next_environment_use = elapsed - 1;
			decide_distractor(*saboteur);
			integration_ok = integration_ok && power_on;
			panel->used = false;
			saboteur->next_environment_use = elapsed - 1;
			saboteur->stats.state = State::Idle;
			decide_distractor(*saboteur);
			integration_ok = integration_ok && !power_on;
		}
	}
	if (script_case == "compound_routes" && physics_frames == 120) {
		refresh_compound_routes();
		int count = 0;
		for (auto &edge : strategic_edges) {
			if (edge.enabled) {
				++count;
			}
		}
		integration_ok = integration_ok && compound_paths_ready && count > 30;
		build_compound_plan(*player);
		integration_ok = integration_ok && !player->plan.steps.empty() && player->plan.expanded > 0;
		print_line("Measured route edges " + itos(count));
	}
	if (script_case == "compound_inventory" && physics_frames == 20) {
		String before = frontend_mode;
		frontend_mode = "inventory";
		int rounds = player->stats.inventory.reserve;
		Dictionary d;
		d["kind"] = "ammo";
		integration_ok = integration_ok && game_command("discard_item", d) && player->stats.inventory.reserve == MAX(0, rounds - 12);
		d["kind"] = "unrecognized";
		integration_ok = integration_ok && !game_command("discard_item", d);
		frontend_mode = before;
	}
	if (script_case == "compound_engagement") {
		auto guard = find_actor(4), enemy = find_actor(10);
		if (physics_frames == 8) {
			guard->body->set_position(Vector3(160, .1, 230));
			guard->target = guard->body->get_position();
			guard->nav->set_target_position(guard->target);
			guard->yaw = Math::PI;
			enemy->body->set_position(Vector3(160, .1, 240));
			enemy->target = enemy->body->get_position();
			enemy->nav->set_target_position(enemy->target);
			enemy->stats.state = State::Idle;
			enemy->stats.health = 100;
			enemy->body->set_collision_layer(2);
		}
		if (physics_frames > 15 && physics_frames % 12 == 0 && physics_frames < 840) {
			perceive_compound(*guard);
			decide_compound(*guard);
		}
		if (physics_frames == 850) {
			integration_ok = integration_ok && guard->stats.shots >= 2 && enemy->stats.health < 100;
			print_line("Guard engagement shots " + itos(guard->stats.shots));
		}
	}
	if (script_case == "compound_nav") {
		const Vector3 targets[] = { { 127, 4.2, 160 }, { 114, 4.2, 160 }, { 169, 4.2, 160 }, { 172, 0, 141 } };
		if (physics_frames == 8) {
			auto server = NavigationServer3D::get_singleton();
			auto map = get_world_3d()->get_navigation_map();
			print_line("Gantry projection " + String(server->map_get_closest_point(map, Vector3(127, 4.4, 160))));
			int high = 0;
			for (auto v : region->get_navigation_mesh()->get_vertices()) {
				if (v.y > 2 && v.y < 6) {
					++high;
				}
			}
			print_line("Gantry high vertices " + itos(high));
			PhysicsDirectSpaceState3D::RayParameters ray;
			ray.from = Vector3(127, 10, 170);
			ray.to = Vector3(127, -1, 170);
			ray.collision_mask = 1;
			PhysicsDirectSpaceState3D::RayResult hit;
			if (get_world_3d()->get_direct_space_state()->intersect_ray(ray, hit)) {
				print_line("Ramp surface " + String(hit.position) + " normal " + String(hit.normal));
			}
			stage = 0;
			player->body->set_position(Vector3(127, .1, 180));
			player->target = targets[0];
			player->nav->set_target_position(player->target);
		}
		if (physics_frames > 8 && stage < 4 && flat(player->body->get_position(), targets[stage]) < 1.3 && Math::abs(player->body->get_position().y - targets[stage].y) < .7) {
			print_line("Gantry physical checkpoint " + itos(stage) + " " + String(player->body->get_position()));
			++stage;
			if (stage < 4) {
				player->target = targets[stage];
				player->nav->set_target_position(player->target);
			} else {
				player->target = player->body->get_position();
				player->nav->set_target_position(player->target);
			}
		}
		if (physics_frames == max_frames) {
			integration_ok = integration_ok && stage == 4;
		}
	}
	if (script_case == "compound_policy") {
		auto enemy = find_actor(10);
		if (physics_frames == 8) {
			player->body->set_position(Vector3(160, .1, 235));
			peer->body->set_position(Vector3(160, .1, 230));
			enemy->body->set_position(Vector3(160, .1, 240));
			enemy->stats.health = 100;
			enemy->stats.state = State::Idle;
			enemy->body->set_collision_layer(2);
			peer->yaw = Math::PI;
			peer->stats.inventory.rounds = 6;
			for (auto a : { player, peer, enemy }) {
				a->target = a->body->get_position();
				a->nav->set_target_position(a->target);
			}
		}
		if (physics_frames == 20) {
			perceive_compound(*peer);
			peer->engagement_id = 10;
			peer->tactic = Tactic::Peek;
			peer->aiming = true;
			peer->react_at = 0;
			shoot(*peer);
			integration_ok = integration_ok && peer->stats.inventory.rounds == 6 && enemy->stats.health == 100;
			player->body->set_position(Vector3(164, .1, 235));
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
		}
		if (physics_frames == 25) {
			perceive_compound(*peer);
			shoot(*peer);
			integration_ok = integration_ok && peer->stats.inventory.rounds == 5 && enemy->stats.health < 100;
		}
		if (physics_frames == 40) {
			peer->contacts.clear();
			peer->visible_actors.clear();
			peer->known_items.clear();
			peer->known_item_kinds.clear();
			peer->last_noise = Vector3(170, 0, 230);
			peer->last_noise_at = elapsed;
			peer->engagement_id = 0;
			int rounds = peer->stats.inventory.rounds;
			shoot(*peer);
			integration_ok = integration_ok && peer->stats.inventory.rounds == rounds;
		}
	}
	if (script_case == "compound_save" && physics_frames == 20) {
		key_found = power_on = radio_done = true;
		mission = 2;
		beacon_active[0] = true;
		beacon_times[0] = elapsed + 22;
		beacon_hold[0] = .25;
		wave_stage = 1;
		next_wave = elapsed + 20;
		peer->contacts[10] = { Vector3(170, 0, 230), Vector3(), elapsed, .8f, true, false, 75 };
		peer->suppression = .65;
		spawn_gameplay_effect("smoke", Vector3(161, 0, 230), 2, elapsed + 10);
		peer->order = 2;
		peer->order_until = elapsed + 30;
		peer->order_position = peer->body->get_position();
		auto saved = snapshot();
		auto bad = saved.duplicate(true);
		Array badsites = bad["beacons"];
		Dictionary site = badsites[0];
		site["arrival"] = -100;
		badsites[0] = site;
		bad["beacons"] = badsites;
		integration_ok = integration_ok && !restore(bad) && beacon_times[0] > elapsed;
		peer->contacts.clear();
		peer->suppression = 0;
		beacon_times[0] = 0;
		wave_stage = 2;
		integration_ok = integration_ok && restore(saved);
		peer = find_actor(2);
		integration_ok = integration_ok && peer->contacts.count(10) && peer->suppression > .6f && beacon_times[0] > elapsed + 21 && wave_stage == 1 && effects.size() == 1 && peer->order == 2;
		bool automatic = automated_run;
		automated_run = false;
		auto revision = peer->personality.revision;
		reset_profile(2);
		automated_run = automatic;
		integration_ok = integration_ok && restore(saved);
		peer = find_actor(2);
		integration_ok = integration_ok && peer->contacts.empty() && peer->contextual_values.empty() && peer->order == 0 && peer->personality.revision == revision + 1;
	}
	if (script_case == "compound_learning" && physics_frames == 20) {
		bool automatic = automated_run;
		automated_run = false;
		peer->learned_context = 35;
		peer->learned_action = int(Tactic::Cover);
		peer->last_learning = elapsed - 5;
		peer->previous_health = 100;
		peer->stats.health = 60;
		learn_tick(*peer);
		integration_ok = integration_ok && peer->contextual_values.count(35) && peer->contextual_values[35][int(Tactic::Cover)] < 0 && save_profiles();
		auto value = peer->contextual_values[35][int(Tactic::Cover)];
		peer->contextual_values.clear();
		load_profiles();
		peer = find_actor(2);
		integration_ok = integration_ok && peer->contextual_values.count(35) && Math::abs(peer->contextual_values[35][int(Tactic::Cover)] - value) < .0001;
		reset_profile(2);
		integration_ok = integration_ok && peer->contextual_values.empty();
		automated_run = automatic;
	}
	if (script_case == "compound_orders" && physics_frames == 20) {
		power_on = false;
		peer->visible_actors.erase(1);
		integration_ok = integration_ok && !issue_order(2, 1);
		power_on = true;
		integration_ok = integration_ok && issue_order(2, 2) && peer->order == 2;
		peer->memory[1].threat = 1;
		integration_ok = integration_ok && !issue_order(2, 1) && peer->order == 2;
		peer->memory[1].threat = 0;
		integration_ok = integration_ok && issue_order(2, 0) && peer->order == 0;
	}
	if (script_case == "compound_beacon") {
		if (physics_frames == 10) {
			key_found = power_on = radio_done = true;
			mission = 2;
			beacon_active[0] = true;
			beacon_times[0] = elapsed + 1;
			auto enemy = find_actor(10);
			enemy->stats.health = 100;
			enemy->stats.state = State::Idle;
			enemy->bot_role = 0;
			enemy->body->set_position(exit_position + Vector3(6, .1, 0));
			enemy->target = enemy->body->get_position();
			enemy->nav->set_target_position(enemy->target);
			stage = 0;
		}
		if (physics_frames == 120) {
			integration_ok = integration_ok && beacon_contested[0] && beacon_times[0] > elapsed + .8;
			auto enemy = find_actor(10);
			enemy->body->set_position(Vector3(200, 0, 230));
			enemy->target = enemy->body->get_position();
			enemy->nav->set_target_position(enemy->target);
		}
		if (physics_frames == 200) {
			integration_ok = integration_ok && !beacon_contested[0] && beacon_times[0] < elapsed;
			player->body->set_position(exit_position);
			player->target = exit_position;
			player->nav->set_target_position(exit_position);
		}
		if (physics_frames == 280) {
			integration_ok = integration_ok && beacon_hold[0] > .7f;
		}
	}
}
