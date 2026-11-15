#include "last_light_game.h"
#include "scene/main/scene_tree.h"

void LastLightGame::integration_step() {
	using namespace lastlight;
	auto player = find_actor(1);
	auto peer = find_actor(2);
	if (compound_world) {
		compound_integration_step();
	}
	if (script_case == "profiles" && physics_frames == 4) {
		bool automated = automated_run;
		automated_run = false;
		emit_event(1, 2, "attack", find_actor(2)->body->get_position());
		integration_ok = find_actor(2)->personality.harmed == 1 && save_profiles();
		int experienced = find_actor(2)->personality.experiences;
		find_actor(2)->personality = baseline_personality(2);
		load_profiles();
		integration_ok = integration_ok && find_actor(2)->personality.experiences == experienced;
		integration_ok = integration_ok && save_game();
		int revision = find_actor(2)->personality.revision;
		integration_ok = integration_ok && reset_profile(2) && load_game();
		integration_ok = integration_ok && find_actor(2)->personality.revision == revision + 1 && find_actor(2)->personality.experiences == 0 && find_actor(2)->memory.empty();
		automated_run = automated;
	}
	if (script_case == "arctic_crossfire") {
		if (physics_frames == 4) {
			for (auto &actor : actors) {
				if (actor.stats.id != 1 && actor.stats.id != 2 && actor.stats.id != 10) {
					actor.stats.health = 0;
					actor.stats.state = State::Dead;
					actor.body->set_collision_layer(0);
				}
			}
			player->body->set_position(Vector3(142, .1, 249));
			peer->body->set_position(Vector3(140, .1, 249));
			auto enemy = find_actor(10);
			enemy->stats.health = 100;
			enemy->stats.state = State::Idle;
			enemy->body->set_position(Vector3(144, .1, 249));
			enemy->body->set_collision_layer(2);
			enemy->body->show();
			for (auto &actor : actors) {
				actor.target = actor.body->get_position();
				actor.nav->set_target_position(actor.target);
			}
			peer->stats.inventory.pistol = true;
			peer->stats.inventory.owned_weapons = 2;
			peer->stats.inventory.weapon_kind = 1;
			peer->stats.inventory.rounds = 24;
			peer->memory[10].threat = 1;
		}
		if (physics_frames == 15) {
			peer->target = find_actor(10)->body->get_position();
			shoot(*peer);
			integration_ok = integration_ok && peer->stats.inventory.rounds == 24 && peer->reposition_until > elapsed;
		}
		if (physics_frames == 18) {
			decide_strategy(*peer);
			integration_ok = integration_ok && !peer->aiming && peer->target.distance_to(peer->body->get_position()) > 1;
		}
		if (physics_frames == 150) {
			find_actor(10)->crouching = true;
		}
		if (physics_frames == 165) {
			peer->target = find_actor(10)->body->get_position();
			peer->aiming = true;
			shoot(*peer);
		}
		if (physics_frames == 170) {
			integration_ok = integration_ok && find_actor(10)->stats.health < 100 && peer->stats.inventory.rounds == 23;
		}
	}
	if (script_case == "arctic_art" && physics_frames == 4) {
		player->body->set_position(Vector3(77, .1, 112));
		player->target = player->body->get_position();
		player->nav->set_target_position(player->target);
		integration_ok = items.size() == 124 && world_objects.size() == 36 && strategic_nodes.size() == 16;
	}
	if (script_case == "arctic_social") {
		if (physics_frames == 4) {
			player->body->set_position(Vector3(140, .1, 249));
			peer->body->set_position(Vector3(142, .1, 249));
			for (auto &actor : actors) {
				actor.target = actor.body->get_position();
				actor.nav->set_target_position(actor.target);
			}
			peer->stats.health = 40;
			peer->stats.inventory.medkits = 0;
			player->stats.inventory.medkits = 2;
		}
		if (physics_frames == 15) {
			integration_ok = begin(player->stats, State::Interact);
			player->environment_action = "share_medkit";
			player->environment_target = 2;
			player->stats.timer = 1.2;
		}
		if (physics_frames == 110) {
			integration_ok = integration_ok && player->stats.inventory.medkits == 1 && peer->stats.inventory.medkits == 1 && peer->persistent_trust[1] > .1f && peer->personality.helped == 1;
			int health = peer->stats.health, kits = peer->stats.inventory.medkits;
			integration_ok = integration_ok && reset_profile(2) && peer->personality.helped == 0 && peer->stats.health == health && peer->stats.inventory.medkits == kits;
		}
	}
	if (script_case == "arctic_equipment") {
		if (physics_frames == 4) {
			for (auto &actor : actors) {
				if (actor.stats.id != 1) {
					actor.stats.state = State::Dead;
					actor.stats.health = 0;
					actor.body->set_collision_layer(0);
				}
			}
			player->body->set_position(Vector3(140, .1, 249));
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
			player->yaw = 0;
			for (auto &item : items) {
				if (item.kind == "smg" && item.available) {
					integration_ok = acquire_item(*player, item);
					break;
				}
			}
			integration_ok = integration_ok && player->stats.inventory.weapon_kind == 1 && player->stats.inventory.rounds == 24;
			player->stats.inventory.rounds = 7;
			player->stats.inventory.owned_weapons |= 4;
			player->stats.inventory.magazines[2] = 12;
			player->stats.inventory.ammunition[2] = 40;
			integration_ok = integration_ok && switch_weapon(player->stats.inventory, 2) && player->stats.inventory.rounds == 12 && switch_weapon(player->stats.inventory, 1) && player->stats.inventory.rounds == 7;
			player->stats.inventory.smoke = 1;
			use_equipment(*player, "smoke");
		}
		if (physics_frames == 65) {
			Vector3 from(140, 1.2, 249), to(140, 1.2, 242.2);
			integration_ok = integration_ok && player->stats.inventory.smoke == 0 && !effects.empty() && clear_line(from, to) && !can_see(from, to);
			for (auto &item : items) {
				if (item.kind == "medkit") {
					auto inventory = player->stats.inventory;
					player->stats.inventory.medkits = 3;
					bool available = item.available;
					integration_ok = integration_ok && !acquire_item(*player, item) && item.available == available;
					player->stats.inventory = inventory;
					break;
				}
			}
			player->stats.inventory.decoys = 1;
			use_equipment(*player, "decoy");
		}
		if (physics_frames == 125) {
			integration_ok = integration_ok && player->stats.inventory.decoys == 0 && effects.size() == 2;
			Dictionary state = snapshot(), bad = state.duplicate(true);
			Array rows = bad["actors"];
			Dictionary first = rows[0];
			Dictionary equipment = first["equipment"];
			equipment["weapon_kind"] = 99;
			integration_ok = integration_ok && !restore(bad) && player->stats.inventory.weapon_kind == 1 && restore(state);
		}
	}
	if (script_case == "arctic_animation") {
		if (physics_frames == 71 || physics_frames == 116 || physics_frames == 189 || physics_frames == 221 || physics_frames == 341 || physics_frames == 401 || physics_frames == 446) {
			print_line("Animation check " + itos(physics_frames) + ": " + (integration_ok ? "passed" : "FAILED") + " state " + String(state_name(player->stats.state)));
		}
		if (physics_frames == 4) {
			for (auto &other : actors) {
				if (other.stats.id != 1) {
					other.stats.health = 0;
					other.stats.state = State::Dead;
					other.body->set_collision_layer(0);
				}
			}
			player->stats.inventory.pistol = true;
			player->stats.inventory.weapon_kind = 1;
			player->stats.inventory.owned_weapons = 2;
			player->stats.inventory.rounds = 24;
			player->stats.inventory.reserve = 24;
			player->stats.inventory.smoke = 1;
			player->target = Vector3(145, 0, 232);
			player->nav->set_target_position(player->target);
			integration_ok = player->animation_tree && player->skeleton && player->gun_models.size() == 4;
		}
		if (physics_frames == 50) {
			player->crouching = true;
		}
		if (physics_frames == 70) {
			integration_ok = integration_ok && player->capsule->get_height() < 1.2;
		}
		if (physics_frames == 90) {
			player->crouching = false;
			player->body->set_position(Vector3(88, .1, 202));
			player->body->set_velocity(Vector3());
			player->target = Vector3(88, 0, 202);
			player->nav->set_target_position(player->target);
		}
		if (physics_frames == 100) {
			player->jump = true;
		}
		if (physics_frames == 115) {
			integration_ok = integration_ok && player->stats.state == State::Airborne;
		}
		if (physics_frames == 170) {
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
			player->aiming = true;
		}
		if (physics_frames == 175) {
			auto target = find_actor(10);
			target->stats.health = 100;
			target->stats.state = State::Idle;
			target->body->set_position(player->body->get_position() + Vector3(0, 0, -6));
			target->body->set_collision_layer(2);
			target->body->show();
			player->target = target->body->get_position();
			target->target = target->body->get_position();
			target->nav->set_target_position(target->target);
		}
		if (physics_frames == 180) {
			shoot(*player);
		}
		if (physics_frames == 188) {
			integration_ok = integration_ok && player->stats.inventory.rounds == 23 && find_actor(10)->stats.health < 100;
		}
		if (physics_frames == 190) {
			begin(player->stats, State::Reload);
		}
		if (physics_frames == 220) {
			integration_ok = integration_ok && player->stats.state == State::Reload && bool(player->animation_tree->get("parameters/Action/active"));
		}
		if (physics_frames == 340) {
			integration_ok = integration_ok && player->stats.inventory.rounds == 24;
			use_equipment(*player, "smoke");
		}
		if (physics_frames == 400) {
			integration_ok = integration_ok && !effects.empty() && player->stats.inventory.smoke == 0;
		}
		if (physics_frames == 420) {
			use_equipment(*player, "roll");
		}
		if (physics_frames == 445) {
			integration_ok = integration_ok && player->body->get_position().distance_to(player->traversal_start) > 1;
		}
	}
	if (script_case == "arctic_interactions") {
		if (physics_frames == 4) {
			for (auto &other : actors) {
				if (other.stats.id != 1) {
					other.stats.health = 0;
					other.stats.state = State::Dead;
					other.body->set_collision_layer(0);
				}
			}
			player->body->set_position(generator_position + Vector3(0, .1, -2.2));
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
			player->stats.inventory.fuel = 1;
			for (auto &object : world_objects) {
				if (object.kind == "generator") {
					player->environment_target = object.id;
				}
			}
			interact_environment(*player);
		}
		if (physics_frames == 210) {
			integration_ok = integration_ok && power_on && player->stats.inventory.fuel == 0;
			for (auto &object : world_objects) {
				if (object.kind == "crate") {
					player->body->set_position(object.position + Vector3(0, .1, 1.3));
					player->target = player->body->get_position();
					player->nav->set_target_position(player->target);
					player->environment_target = object.id;
					interact_environment(*player);
					break;
				}
			}
		}
		if (physics_frames == 360) {
			bool opened = false;
			for (auto &object : world_objects) {
				if (object.kind == "crate" && object.used) {
					opened = true;
				}
			}
			integration_ok = integration_ok && opened && save_game() && load_game();
		}
	}
	if (script_case == "assets") {
		if (physics_frames == 4) {
			integration_ok = gate_model != nullptr;
			for (auto &actor : actors) {
				integration_ok = integration_ok && actor.skeleton && actor.animation_tree && actor.weapon_socket && actor.pistol_model && actor.sword_model;
				if (actor.animation) {
					for (const String &name : { String("Idle_Loop"), String("Walk_Loop"), String("Sprint_Loop"), String("Pistol_Reload"), String("extra/Sword_Regular_A") }) {
						integration_ok = integration_ok && actor.animation->has_animation(name);
					}
				}
			}
			player->stats.inventory.pistol = true;
			player->stats.inventory.rounds = 1;
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
		}
		if (physics_frames == 30) {
			integration_ok = integration_ok && begin(player->stats, State::Reload);
		}
		if (physics_frames == 60) {
			integration_ok = integration_ok && bool(player->animation_tree->get("parameters/Action/active"));
		}
		if (physics_frames == 150) {
			integration_ok = integration_ok && player->stats.inventory.rounds == 6;
			player->stats.inventory.sword = true;
			player->use_sword = true;
			melee(*player);
		}
		if (physics_frames == 165) {
			integration_ok = integration_ok && player->stats.state == State::Melee && bool(player->animation_tree->get("parameters/Action/active"));
		}
	}
	if (script_case == "gate_save") {
		if (physics_frames == 100) {
			find_actor(3)->body->set_position(Vector3(14, .1, 20));
		}
		if (physics_frames == 165) {
			integration_ok = integration_ok && gate_open && gate_operator_a > 0 && gate_operator_b > 0 && gate_operator_a != gate_operator_b;
			for (int i = 0; i < journal.size(); ++i) {
				Dictionary event = journal[i];
				if (String(event.get("kind", "")) == "help") {
					integration_ok = integration_ok && int(event.get("actor", 0)) > 0;
				}
			}
			integration_ok = integration_ok && save_game() && load_game();
		}
	}
	if (script_case == "social") {
		if (physics_frames == 4) {
			player->body->set_position(Vector3(8, .1, 14));
			peer->body->set_position(Vector3(10, .1, 14));
			find_actor(3)->body->set_position(Vector3(28, .1, 14));
			emit_event(1, 2, "help", Vector3(8, 1, 14));
			integration_ok = peer->memory[1].trust > .2f && find_actor(3)->memory[1].trust == 0;
			emit_event(1, 2, "attack", Vector3(8, 1, 14));
			integration_ok = integration_ok && peer->memory[1].threat > .5f && find_actor(3)->memory[1].threat == 0;
		}
	}
	if (script_case == "gate_negative") {
		if (physics_frames == 4) {
			peer->body->set_position(control_a + Vector3(0, .1, 0));
			peer->interacting = true;
		}
		if (physics_frames == 150) {
			integration_ok = integration_ok && !gate_open && gate_hold == 0;
			peer->interacting = false;
		}
	}
	if (script_case == "melee") {
		if (physics_frames == 4) {
			player->body->set_position(Vector3(8, .1, 14));
			peer->body->set_position(Vector3(8, .1, 12.8));
			player->stats.inventory.sword = true;
			player->use_sword = true;
			player->yaw = 0;
			player->target = player->body->get_position();
			player->nav->set_target_position(player->target);
			peer->target = peer->body->get_position();
			peer->nav->set_target_position(peer->target);
			melee(*player);
		}
		if (physics_frames == 12) {
			integration_ok = integration_ok && peer->stats.health == 100;
		}
		if (physics_frames == 40) {
			integration_ok = integration_ok && peer->stats.health == 65;
		}
		if (physics_frames == 80) {
			integration_ok = integration_ok && peer->stats.health == 65;
			melee(*player);
			cancel(player->stats);
		}
		if (physics_frames == 120) {
			integration_ok = integration_ok && peer->stats.health == 65;
		}
	}
	if (script_case == "lifecycle") {
		if (physics_frames == 4) {
			for (auto &actor : actors) {
				if (actor.stats.survivor) {
					actor.stats.health = 0;
					actor.stats.state = State::Dead;
				}
			}
		}
	}
}
