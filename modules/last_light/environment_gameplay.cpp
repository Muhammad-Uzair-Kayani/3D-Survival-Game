#include "last_light_game.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include <cmath>
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
} //namespace
void LastLightGame::set_door(WorldObject &door, bool open) {
	if (door.kind != "door" || !door.body || door.open == open) {
		return;
	}
	// Closing is rejected while a capsule occupies the doorway.
	if (!open) {
		for (auto &a : actors) {
			CollisionShape3D *shape = nullptr;
			for (int child = 0; child < door.body->get_child_count(); ++child) {
				shape = Object::cast_to<CollisionShape3D>(door.body->get_child(child));
				if (shape) {
					break;
				}
			}
			Vector3 half(1.2, 2, 1.2);
			if (shape) {
				Ref<BoxShape3D> box = shape->get_shape();
				if (box.is_valid()) {
					half = box->get_size() * .5f;
				}
			}
			Vector3 local = a.body->get_position() - door.body->get_position();
			if (a.stats.active() && Math::abs(local.x) < half.x + .4f && Math::abs(local.z) < half.z + .4f && local.y + 1.65f > -half.y && local.y < half.y) {
				return;
			}
		}
	}
	door.open = open;
	door.body->set_collision_layer(open ? 0 : 1);
	door.value = open ? 1 : 0;
}
bool LastLightGame::acquire_item(Actor &a, Item &item) {
	auto i = a.stats.inventory;
	String kind = item.kind;
	if (!item.available || !a.stats.active() || a.stats.busy()) {
		return false;
	}
	if (i.weight() >= i.capacity() && kind != "key" && kind != "backpack") {
		return false;
	}
	if (kind == "key") {
		if (!a.stats.survivor) {
			return false;
		}
		key_found = true;
		mission = MAX(mission, 1);
		notify("Access key recovered. Restore power and calibrate two relays for the radio.");
	} else if (kind == "pistol" || kind == "smg" || kind == "rifle" || kind == "shotgun") {
		int weapon = kind == "pistol" ? 0 : kind == "smg" ? 1
				: kind == "rifle"						  ? 2
														  : 3;
		if (i.owned_weapons & (1 << weapon)) {
			return false;
		}
		if (i.pistol) {
			i.magazines[i.weapon_kind] = i.rounds;
			i.ammunition[i.weapon_kind] = i.reserve;
		}
		i.owned_weapons |= 1 << weapon;
		i.weapon_kind = weapon;
		i.pistol = true;
		i.rounds = lastlight::weapon_spec(weapon).magazine;
		i.reserve = i.ammunition[weapon];
		a.use_sword = false;
	} else if (kind == "ammo" || kind == "smg_ammo" || kind == "rifle_ammo" || kind == "shells") {
		int weapon = kind == "ammo" ? 0 : kind == "smg_ammo" ? 1
				: kind == "rifle_ammo"						 ? 2
															 : 3;
		int &reserve = weapon == i.weapon_kind ? i.reserve : i.ammunition[weapon];
		if (reserve >= 120) {
			return false;
		}
		reserve = MIN(120, reserve + (weapon == 3 ? 10 : 24));
	} else if (kind == "sword") {
		if (i.sword) {
			return false;
		}
		i.sword = true;
	} else if (kind == "medkit") {
		if (i.medkits >= 3) {
			return false;
		}
		++i.medkits;
	} else if (kind == "bandage") {
		if (i.bandages >= 5) {
			return false;
		}
		++i.bandages;
	} else if (kind == "food") {
		if (i.food >= 4) {
			return false;
		}
		++i.food;
	} else if (kind == "water") {
		if (i.water >= 4) {
			return false;
		}
		++i.water;
	} else if (kind == "fuel") {
		if (i.fuel >= 2) {
			return false;
		}
		++i.fuel;
	} else if (kind == "smoke") {
		if (i.smoke >= 3) {
			return false;
		}
		++i.smoke;
	} else if (kind == "decoy") {
		if (i.decoys >= 3) {
			return false;
		}
		++i.decoys;
	} else if (kind == "armour") {
		if (i.armour >= 100) {
			return false;
		}
		i.armour = 100;
	} else if (kind == "backpack") {
		if (i.backpack) {
			return false;
		}
		i.backpack = 1;
	} else if (kind == "tools") {
		if (i.tools) {
			return false;
		}
		i.tools = 1;
	} else if (kind == "coat") {
		if (i.coat) {
			return false;
		}
		i.coat = 1;
	} else {
		return false;
	}
	if (kind != "key" && i.weight() > i.capacity()) {
		return false;
	}
	a.stats.inventory = i;
	item.available = false;
	item.visual->hide();
	++pickups;
	++a.stats.pickups;
	a.known_items.erase(item.id);
	a.known_item_kinds.erase(item.id);
	a.next_plan = 0;
	emit_event(a.stats.id, item.id, "pickup", a.body->get_position());
	return true;
}
void LastLightGame::use_equipment(Actor &a, const String &kind) {
	auto &i = a.stats.inventory;
	if (!a.stats.active() || a.stats.busy()) {
		return;
	}
	if ((kind == "bandage" && (!i.bandages || a.stats.health >= 100)) || (kind == "food" && (!i.food || a.stats.hunger >= 100)) || (kind == "water" && (!i.water || i.hydration >= 100)) || (kind == "smoke" && !i.smoke) || (kind == "decoy" && !i.decoys)) {
		return;
	}
	if (kind == "roll" && a.stats.stamina < 25) {
		return;
	}
	if (!lastlight::begin(a.stats, lastlight::State::Interact)) {
		return;
	}
	a.environment_action = kind;
	a.environment_target = 0;
	a.stats.timer = (kind == "smoke" || kind == "decoy") ? .65f : kind == "roll" ? .7f
																				 : 1.2f;
	if (kind == "roll") {
		a.stats.stamina -= 25;
		a.traversal_start = a.body->get_position();
		a.traversal_end = a.traversal_start - Basis(Vector3(0, 1, 0), a.yaw).get_column(2) * 3;
	}
}
bool LastLightGame::interact_environment(Actor &a) {
	if (!a.stats.active() || a.stats.busy()) {
		return false;
	}
	WorldObject *nearest = nullptr;
	float distance = 3;
	for (auto &object : world_objects) {
		if ((object.kind == "security" && object.used) || object.kind == "shelter" || object.kind == "crate" && object.used || object.kind == "door" && object.open || object.kind == "generator" && power_on || object.kind == "relay" && object.used || object.kind == "radio" && radio_done) {
			continue;
		}
		float d = flat(a.body->get_position(), object.position);
		if (d >= 3 || (d >= distance && object.id != a.environment_target)) {
			continue;
		}
		PhysicsDirectSpaceState3D::RayParameters query;
		query.from = a.body->get_position() + Vector3(0, 1.2, 0);
		query.to = object.position + Vector3(0, .8, 0);
		query.collision_mask = 1;
		PhysicsDirectSpaceState3D::RayResult hit;
		bool blocked = get_world_3d()->get_direct_space_state()->intersect_ray(query, hit);
		if (blocked && (!object.body || hit.collider != object.body)) {
			continue;
		}
		nearest = &object;
		distance = d;
		if (object.id == a.environment_target) {
			break;
		}
	}
	if (!nearest) {
		return false;
	}
	if (nearest->kind == "door" && nearest->locked && !key_found && !a.stats.inventory.tools) {
		if (a.stats.id == 1) {
			notify("This door needs the access key or tools.");
		}
		return true;
	}
	if (nearest->kind == "security" && (!power_on || !a.stats.inventory.tools)) {
		if (a.stats.id == 1) {
			notify("Power and a tool kit are required to lock the service panel.");
		}
		return true;
	}
	if (nearest->kind == "relay" && !power_on) {
		if (a.stats.id == 1) {
			notify("Restore generator power before calibrating a relay.");
		}
		return true;
	}
	if (compound_world && nearest->kind == "extraction" && !radio_done) {
		if (a.stats.id == 1) {
			notify("Restore the signal before calling an airlift.");
		}
		return true;
	}
	if (nearest->kind == "radio") {
		int relays = 0;
		for (auto &object : world_objects) {
			if (object.kind == "relay" && object.used) {
				++relays;
			}
		}
		if (!key_found || !power_on || relays < 2) {
			if (a.stats.id == 1) {
				notify("Radio needs the access key, generator power and two calibrated relays.");
			}
			return true;
		}
	}
	if (!lastlight::begin(a.stats, lastlight::State::Interact)) {
		return true;
	}
	a.environment_target = nearest->id;
	a.environment_action = nearest->kind;
	a.stats.timer = nearest->kind == "security" ? 4.f : nearest->kind == "door" ? .45f
			: nearest->kind == "generator"										? (a.stats.inventory.fuel ? 3.f : compound_world && a.stats.inventory.tools ? 6.f
																																							: 12.f)
			: nearest->kind == "radio"											? 3.f
																				: 2.f;
	a.reason = nearest->kind == "generator" && !a.stats.inventory.fuel ? "Hand-cranking backup power" : nearest->name;
	return true;
}
void LastLightGame::complete_environment_action(Actor &a) {
	String kind = a.environment_action;
	int target = a.environment_target;
	a.environment_action = String();
	a.environment_target = 0;
	auto &i = a.stats.inventory;
	if (!a.stats.active()) {
		return;
	}
	if (kind.begins_with("share_")) {
		auto peer = find_actor(target);
		if (!peer || !peer->stats.active() || flat(a.body->get_position(), peer->body->get_position()) > 3.2f || !clear_line(a.body->get_position() + Vector3(0, 1, 0), peer->body->get_position() + Vector3(0, 1, 0))) {
			return;
		}
		auto recipient = peer->stats.inventory;
		auto donor = i;
		bool shared = false;
		if (kind == "share_medkit" && donor.medkits > 1 && recipient.medkits < 3) {
			--donor.medkits;
			++recipient.medkits;
			shared = true;
		}
		if (kind == "share_food" && donor.food > 1 && recipient.food < 4) {
			--donor.food;
			++recipient.food;
			shared = true;
		}
		if (kind == "share_water" && donor.water > 1 && recipient.water < 4) {
			--donor.water;
			++recipient.water;
			shared = true;
		}
		if (kind == "share_ammo" && donor.weapon_kind == recipient.weapon_kind && donor.reserve >= 24 && recipient.reserve <= 108) {
			donor.reserve -= 12;
			recipient.reserve += 12;
			shared = true;
		}
		if (shared && recipient.weight() <= recipient.capacity()) {
			i = donor;
			peer->stats.inventory = recipient;
			peer->next_plan = 0;
			emit_event(a.stats.id, target, "help", a.body->get_position());
		}
		return;
	}
	if (!target) {
		if (kind == "bandage" && i.bandages) {
			--i.bandages;
			a.stats.health = MIN(100, a.stats.health + 22);
		}
		if (kind == "food" && i.food) {
			--i.food;
			a.stats.hunger = MIN(100.f, a.stats.hunger + 45);
		}
		if (kind == "water" && i.water) {
			--i.water;
			i.hydration = MIN(100.f, i.hydration + 50);
		}
		if ((kind == "smoke" && i.smoke) || (kind == "decoy" && i.decoys)) {
			if (kind == "smoke") {
				--i.smoke;
			} else {
				--i.decoys;
			}
			Vector3 forward = -Basis(Vector3(0, 1, 0), a.yaw).get_column(2);
			Vector3 position = a.body->get_position() + forward * (kind == "smoke" ? 3.5f : 6.f);
			if (compound_world) {
				PhysicsDirectSpaceState3D::RayParameters ray;
				ray.from = a.body->get_position() + Vector3(0, 1.2, 0);
				ray.to = position + Vector3(0, .35, 0);
				ray.collision_mask = 1;
				PhysicsDirectSpaceState3D::RayResult hit;
				if (get_world_3d()->get_direct_space_state()->intersect_ray(ray, hit)) {
					position = hit.position + hit.normal * .3f - Vector3(0, .3, 0);
				}
			}
			spawn_gameplay_effect(kind, position, a.stats.id, elapsed + (kind == "smoke" ? 12 : 8));
			emit_event(a.stats.id, 0, "noise", position);
		}
		return;
	}
	for (auto &object : world_objects) {
		if (object.id == target) {
			if (compound_world && kind == "extraction" && radio_done && flat(a.body->get_position(), object.position) < 4) {
				int landing = int(object.value);
				if (landing >= 0 && landing < 2 && !beacon_active[landing]) {
					beacon_active[landing] = true;
					beacon_times[landing] = elapsed + (landing ? 32 : 22);
					notify("Airlift requested. Hold a safe landing perimeter until arrival.");
					emit_event(a.stats.id, 0, "airlift", object.position);
				}
				return;
			}

			if ((compound_world && Math::abs(a.body->get_position().y - object.position.y) > 1.8f) || flat(a.body->get_position(), object.position) > 3.2) {
				return;
			}
			if (kind == "door") {
				if (!object.locked || key_found || i.tools) {
					set_door(object, true);
				}
			}
			if (kind == "crate" && !object.used) {
				object.used = true;
				for (auto &item : items) {
					if (item.container == object.id) {
						item.available = true;
						item.visual->show();
					}
				}
				emit_event(a.stats.id, object.id, "search", object.position);
				a.next_plan = 0;
			}
			if (kind == "generator" && !power_on) {
				if (i.fuel) {
					--i.fuel;
				}
				power_on = true;
				object.used = true;
				emit_event(a.stats.id, object.id, "power", object.position);
				notify("Generator online. Calibrate any two relays, then use the radio.");
			}
			if (kind == "security" && power_on && i.tools && !object.used) {
				object.used = true;
				emit_event(a.stats.id, object.id, "secure", object.position);
				notify("Turbine service panel locked. Opportunistic sabotage is blocked.");
			}
			if (kind == "relay" && power_on && !object.used) {
				object.used = true;
				emit_event(a.stats.id, object.id, "relay", object.position);
				notify("Relay calibrated. Two relays are required for the radio.");
			}
			if (kind == "radio" && !radio_done && power_on && key_found) {
				int relays = 0;
				for (auto &relay : world_objects) {
					if (relay.kind == "relay" && relay.used) {
						++relays;
					}
				}
				if (relays >= 2) {
					radio_done = true;
					mission = 2;
					object.used = true;
					emit_event(a.stats.id, 0, "radio", object.position);
					notify("Signal restored. Either landing zone can evacuate survivors.");
				}
			}
			return;
		}
	}
}
void LastLightGame::update_environment(float dt) {
	for (size_t n = 0; n < effects.size();) {
		auto &effect = effects[n];
		if (effect.until <= elapsed) {
			if (effect.visual) {
				effect.visual->queue_free();
			}
			effects.erase(effects.begin() + n);
			continue;
		}
		if (effect.kind == "decoy" && int(elapsed * 2) % 3 == 0) {
			for (auto &a : actors) {
				if (a.stats.active() && flat(a.body->get_position(), effect.position) < 40) {
					a.heard_position = effect.position;
					a.heard_until = elapsed + 2;
				}
			}
		}
		++n;
	}
	for (auto &door : world_objects) {
		if (door.kind == "door" && door.body) {
			Vector3 rotation = door.body->get_rotation();
			rotation.y = Math::lerp_angle(rotation.y, door.open ? float(Math::PI / 2) : 0.f, MIN(1.f, dt * 5));
			door.body->set_rotation(rotation);
		}
	}
	for (auto &a : actors) {
		if (a.stats.survivor && a.stats.state == lastlight::State::Dead && !a.outcome_recorded) {
			emit_event(a.stats.id, 0, "death", a.body->get_position());
			record_experience(a, "died", 0, -1);
			a.outcome_recorded = true;
		}
		if (!a.stats.active()) {
			continue;
		}
		auto &i = a.stats.inventory;
		bool sheltered = false;
		for (const auto &node : strategic_nodes) {
			if (node.shelter && flat(a.body->get_position(), Vector3(node.position.x, 0, node.position.z)) < 7) {
				sheltered = true;
			}
		}
		bool water = a.body->get_position().y < -.5;
		i.cold = CLAMP(i.cold + dt * (sheltered ? -3.5f : water ? 2.f
													 : i.coat	? .012f
																: .05f),
				0.f, 100.f);
		i.hydration = MAX(0.f, i.hydration - dt * (a.sprint ? .07f : .035f));
		if ((i.cold > 90 || i.hydration <= 0) && int(elapsed * 60) % 300 == 0) {
			lastlight::damage(a.stats, 2);
		}
		if (i.cold > 70) {
			a.stats.stamina = MAX(0.f, a.stats.stamina - dt * 2);
		}
		if (a.stats.id >= 2 && a.stats.id <= 5) {
			learn_tick(a);
		}
	}
	if (elapsed >= next_profile_save) {
		next_profile_save = elapsed + 10;
		save_profiles();
	}
}

void LastLightGame::spawn_gameplay_effect(const String &kind, const Vector3 &position, int owner, double until) {
	Node3D *visual = nullptr;
	if (kind == "smoke") {
		visual = memnew(Node3D);
		level_root->add_child(visual);
		visual->set_position(position);
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code("shader_type spatial;render_mode unshaded,cull_disabled,depth_draw_never;void fragment(){ALBEDO=vec3(.4,.5,.57);ALPHA=pow(max(dot(normalize(NORMAL),normalize(VIEW)),0.0),0.7)*.24;}");
		Ref<ShaderMaterial> material;
		material.instantiate();
		material->set_shader(shader);
		Ref<SphereMesh> puff;
		puff.instantiate();
		puff->set_radius(1.6);
		puff->set_height(3.2);
		puff->set_radial_segments(12);
		puff->set_rings(6);
		puff->set_material(material);
		for (int j = 0; j < 8; ++j) {
			auto mesh = memnew(MeshInstance3D);
			visual->add_child(mesh);
			mesh->set_mesh(puff);
			mesh->set_position(Vector3(Math::cos(j * 2.4) * 1.2, .8 + (j % 3) * .35, Math::sin(j * 2.4) * 1.2));
		}
	} else {
		visual = make_box(level_root, position + Vector3(0, .15, 0), Vector3(.3, .3, .3), Color(1, .5, .2));
	}
	effects.push_back({ kind, position, until, visual, owner });
}
