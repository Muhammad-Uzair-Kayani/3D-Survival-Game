#include "core/config/project_settings.h"
#include "last_light_game.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/material.h"
#include "servers/display_server.h"
#include <cmath>
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
float random_unit(uint32_t x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return (x & 0xffff) / 32767.5f - 1.f;
}
} //namespace
void LastLightGame::play_spatial(const String &kind, const Vector3 &where, float pitch) {
	if (!compound_world || DisplayServer::get_singleton()->get_name() == "headless" || master_volume <= 0) {
		return;
	}
	String path = "res://assets/arctic/sci-fi-sounds/" + kind + ".wav";
	if (!FileAccess::exists(path)) {
		return;
	}
	Ref<AudioStreamWAV> stream;
	if (scene_cache.has(path)) {
		stream = scene_cache[path];
	} else {
		stream = AudioStreamWAV::load_from_file(path, Dictionary());
		if (stream.is_null()) {
			return;
		}
		scene_cache[path] = stream;
	}
	auto voice = memnew(AudioStreamPlayer3D);
	level_root->add_child(voice);
	voice->set_position(where);
	voice->set_stream(stream);
	voice->set_max_distance(65);
	voice->set_unit_size(5);
	voice->set_volume_db(Math::linear_to_db(MAX(.001f, master_volume)) - 8);
	voice->set_pitch_scale(pitch);
	voice->play();
	voice->connect("finished", callable_mp((Node *)voice, &Node::queue_free));
}
void LastLightGame::shoot_compound(Actor &a) {
	using namespace lastlight;
	auto &inv = a.stats.inventory;
	bool human = a.stats.id == 1 && !autoplay && !smoke;
	if (!a.stats.active() || a.stats.busy() || !inv.pistol || inv.rounds <= 0 || a.stats.cooldown > 0) {
		return;
	}
	Vector3 from = a.body->get_position() + Vector3(0, a.crouching ? .8f : 1.25f, 0);
	auto space = get_world_3d()->get_direct_space_state();
	Vector3 aim = a.aim_point;
	if (human) {
		PhysicsDirectSpaceState3D::RayParameters ray;
		if (overview) {
			Vector2 mouse = get_viewport()->get_mouse_position();
			ray.from = camera->project_ray_origin(mouse);
			ray.to = ray.from + camera->project_ray_normal(mouse) * 200;
		} else {
			ray.from = camera->get_global_position();
			ray.to = ray.from - camera->get_global_basis().get_column(2) * 150;
		}
		ray.collision_mask = 3;
		ray.exclude.insert(a.body->get_rid());
		PhysicsDirectSpaceState3D::RayResult hit;
		aim = space->intersect_ray(ray, hit) ? hit.position : ray.to;
	} else {
		auto found = a.contacts.find(a.engagement_id);
		if (found == a.contacts.end() || found->second.health_band == 0 || !found->second.hostile || ((!found->second.visible) && !(a.tactic == Tactic::Suppress && elapsed - found->second.seen < 2))) {
			return;
		}
		aim = found->second.position + Vector3(0, .95, 0);
		if (found->second.visible) {
			auto target = find_actor(a.engagement_id);
			if (target && target->crouching) {
				aim.y = found->second.position.y + .65f;
			}
		}
	}
	Vector3 direction = (aim - from).normalized();
	if (direction.length() < .1) {
		return;
	}
	auto spec = weapon_spec(inv.weapon_kind);
	int pellets = inv.weapon_kind == 3 ? 6 : 1;
	float spread = inv.weapon_kind == 3 ? .065f : inv.weapon_kind == 2 ? .0035f
			: inv.weapon_kind == 1									   ? .012f
																	   : .008f;
	if (!a.aiming) {
		spread *= 2;
	}
	spread += a.suppression * .02f + a.body->get_velocity().length() * .003f;
	Vector3 right = direction.cross(Vector3(0, 1, 0)).normalized();
	if (right.length() < .1) {
		right = Vector3(1, 0, 0);
	}
	Vector3 up = right.cross(direction).normalized();
	struct Impact {
		Actor *victim;
		Vector3 point;
		bool hit;
	};
	std::vector<Impact> hits;
	for (int pellet = 0; pellet < pellets; ++pellet) {
		uint32_t noise = uint32_t(seed) * 31337U + uint32_t(a.stats.id) * 971U + uint32_t(a.stats.shots + 1) * 1709U + pellet * 31U;
		Vector3 ray_direction = (direction + right * random_unit(noise) * spread + up * random_unit(noise + 17) * spread).normalized();
		PhysicsDirectSpaceState3D::RayParameters ray;
		ray.from = from;
		ray.to = from + ray_direction * spec.range;
		ray.collision_mask = 3;
		ray.exclude.insert(a.body->get_rid());
		PhysicsDirectSpaceState3D::RayResult impact;
		bool hit = space->intersect_ray(ray, impact);
		Actor *victim = nullptr;
		if (hit) {
			for (auto &other : actors) {
				if (impact.collider == other.body) {
					victim = &other;
					break;
				}
			}
		}
		if (!human && victim && victim->stats.survivor == a.stats.survivor && !a.contacts[victim->stats.id].hostile) {
			a.reason = "Fire vetoed: neutral or teammate crosses the actual ray";
			a.flank_clear = true;
			a.tactical_until = 0;
			return;
		}
		hits.push_back({ victim, hit ? impact.position : ray.to, hit });
	}
	if (!fire(a.stats)) {
		return;
	}
	std::map<int, int> damage_by_actor;
	for (auto &hit : hits) {
		if (hit.victim && hit.victim->stats.active()) {
			damage_by_actor[hit.victim->stats.id] += inv.weapon_kind == 3 ? 10 : spec.damage;
		} else if (hit.hit) {
			++wall_blocks;
		}
		for (auto &other : actors) {
			if (other.stats.id == a.stats.id || !other.stats.active()) {
				continue;
			}
			Vector3 segment = hit.point - from;
			float t = CLAMP((other.body->get_position() + Vector3(0, 1, 0) - from).dot(segment) / MAX(.01f, segment.length_squared()), 0.f, 1.f);
			if (t > .1f && (from + segment * t - other.body->get_position() - Vector3(0, 1, 0)).length() < 1.8) {
				other.suppression = MIN(1.f, other.suppression + .22f);
				other.last_noise = from;
				other.last_noise_at = elapsed;
				other.noise_danger = true;
			}
		}
		if (DisplayServer::get_singleton()->get_name() != "headless" && effects.size() < 70) {
			Vector3 delta = hit.point - from;
			auto tracer = make_box(level_root, (from + hit.point) * .5f, Vector3(.025, .025, delta.length()), Color(1, .72, .33));
			tracer->look_at(hit.point, Vector3(0, 1, 0));
			effects.push_back({ "tracer", hit.point, elapsed + .055, tracer, a.stats.id });
		}
	}
	for (auto &[id, amount] : damage_by_actor) {
		auto victim = find_actor(id);
		if (!victim) {
			continue;
		}
		if (a.stats.survivor && victim->stats.survivor && a.memory[id].threat < .25f) {
			++harmful_attacks;
		}
		damage(victim->stats, amount);
		victim->last_damage_at = elapsed;
		victim->suppression = MIN(1.f, victim->suppression + .45f);
		victim->last_noise = from;
		victim->last_noise_at = elapsed;
		victim->noise_danger = true;
		++shots_hit;
		a.hit_feedback_time = elapsed;
		emit_event(a.stats.id, id, "attack", from);
	}
	emit_event(a.stats.id, 0, "shot", from);
	play_spatial(inv.weapon_kind == 0 ? "pistol" : inv.weapon_kind == 1 ? "smg"
					: inv.weapon_kind == 2								? "rifle"
																		: "shotgun",
			from);
	if (human && !overview) {
		camera_pitch = CLAMP(camera_pitch + (inv.weapon_kind == 3 ? .05f : .018f), -1.1f, .5f);
	}
}
void LastLightGame::update_compound(float dt) {
	using namespace lastlight;
	for (auto &a : actors) {
		a.suppression = MAX(0.f, a.suppression - dt * .18f);
		if (a.stats.state == State::Dead || a.stats.state == State::Extracted) {
			a.body->set_collision_layer(0);
		}
		if (!a.stats.active()) {
			continue;
		}
		if (a.stats.id != 1 || autoplay || smoke) {
			if (!a.stats.busy() && a.target.distance_to(a.body->get_position()) > 1.5) {
				for (auto &door : world_objects) {
					if (door.kind == "door" && !door.open && (!door.locked || key_found || a.stats.inventory.tools) && flat(a.body->get_position(), door.position) < 3.5 && Math::abs(a.body->get_position().y - door.position.y) < 1) {
						set_door(door, true);
						break;
					}
				}
			}
		}
		if (a.name_tag) {
			auto human = find_actor(1);
			a.name_tag->set_visible(!human || human->visible_actors.count(a.stats.id));
		}
		if (a.task_claim && mission_claims.count(a.task_claim)) {
			auto &claim = mission_claims[a.task_claim];
			if (claim.owner == a.stats.id) {
				float distance = flat(a.body->get_position(), claim.position);
				if (distance < a.task_distance - 1) {
					a.task_distance = distance;
					a.task_progress_at = elapsed;
				}
				if ((elapsed - a.task_progress_at > 18 && distance > 4) || (elapsed - a.task_progress_at > 25 && !a.stats.busy() && a.task_claim != 10003 && a.task_claim != 10004 && !(a.task_claim >= 12000 && a.task_claim < 13000))) {
					a.failed_jobs[a.task_claim] = elapsed + 25;
					claim.until = elapsed;
					a.task_claim = 0;
					a.next_plan = 0;
				}
				if (elapsed - a.task_progress_at < 15 || distance < 4) {
					claim.until = elapsed + 10;
					claim.peer_position = a.body->get_position();
				}
			}
		}

		if (a.tactical_decision.legal[int(Tactic::Peek)] && !a.cover_protected) {
			a.exposure_seconds += dt;
		}
		if (a.cover_id >= 0 && size_t(a.cover_id) < cover_points.size() && flat(a.body->get_position(), a.cover_goal) < 1.5f) {
			cover_points[a.cover_id].owner = a.stats.id;
			cover_points[a.cover_id].reserved_until = elapsed + 2;
		}
	}
	for (auto it = mission_claims.begin(); it != mission_claims.end();) {
		bool complete = (it->first == 10000 && key_found) || (it->first == 10001 && power_on) || (it->first == 10002 && radio_done) || ((it->first == 10006 || it->first == 10007) && gate_open);
		for (auto &o : world_objects) {
			if (o.id == it->first && (o.kind == "relay" || o.kind == "security") && o.used) {
				complete = true;
			}
		}
		if (complete || it->second.until < elapsed) {
			it = mission_claims.erase(it);
		} else {
			++it;
		}
	}
	for (auto &r : roofs) {
		auto actor = find_actor(camera_focus);
		if (!actor || !r.node) {
			continue;
		}
		Vector3 p = actor->body->get_position();
		bool inside = Math::abs(p.x - r.center.x) < r.size.x * .5f + .8f && Math::abs(p.z - r.center.z) < r.size.z * .5f + .8f && p.y < r.center.y - 1;
		r.node->set_visible(!inside);
	}
	for (int i = 0; i < 2; ++i) {
		if (!beacon_active[i]) {
			continue;
		}
		Vector3 p = i ? alternative_exit : exit_position;
		bool contested = false;
		for (auto &a : actors) {
			if (!a.stats.survivor && a.stats.active() && (a.bot_role == 0 || a.bot_role == 4) && flat(a.body->get_position(), p) < 14) {
				contested = true;
				break;
			}
		}
		beacon_contested[i] = contested;
		if (contested) {
			beacon_hold[i] = 0;
		}
		if (contested && elapsed < beacon_times[i]) {
			beacon_times[i] += dt;
		}
		if (elapsed >= beacon_times[i] && !contested) {
			bool occupant = false;
			for (auto &a : actors) {
				if (a.stats.survivor && a.stats.active() && flat(a.body->get_position(), p) < 4) {
					occupant = true;
				}
			}
			if (occupant) {
				beacon_hold[i] += dt;
			}
		}
	}
	if (radio_done && spawn_count > 0 && wave_stage == 0) {
		next_wave = elapsed + 20;
		wave_stage = 1;
	}
	if (wave_stage == 1 && elapsed >= next_wave) {
		int spawned = 0;
		for (auto &a : actors) {
			if (a.stats.id < 10 + spawn_count || a.stats.id > 17 || a.stats.survivor) {
				continue;
			}
			Vector3 p = (a.stats.id % 2) ? Vector3(215, .1, 105) : Vector3(72, .1, 183);
			bool visible_to_human = false;
			auto human = find_actor(1);
			if (human && flat(human->body->get_position(), p) < 35) {
				visible_to_human = true;
			}
			if (visible_to_human) {
				continue;
			}
			a.stats.health = 100;
			a.stats.state = State::Idle;
			a.stats.inventory = Inventory();
			a.bot_role = 0;
			a.stats.inventory.pistol = true;
			a.stats.inventory.weapon_kind = 0;
			a.stats.inventory.owned_weapons = 1;
			a.stats.inventory.rounds = 6;
			a.stats.inventory.reserve = 18;
			a.body->set_position(p);
			a.body->set_collision_layer(2);
			a.body->show();
			++spawned;
			if (spawned >= 2) {
				break;
			}
		}
		wave_stage = 2;
		if (spawned) {
			notify("Radio traffic drew raiders to the approach roads. Secure a landing.");
		}
	}
}
