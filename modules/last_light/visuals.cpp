#include "core/io/resource_loader.h"
#include "last_light_game.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#include "scene/animation/animation_blend_space_1d.h"
#include "scene/animation/animation_blend_space_2d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/material.h"
#include "servers/display_server.h"

namespace {
AnimationPlayer *find_player(Node *node) {
	if (auto value = Object::cast_to<AnimationPlayer>(node)) {
		return value;
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		if (auto value = find_player(node->get_child(i))) {
			return value;
		}
	}
	return nullptr;
}
Skeleton3D *find_skeleton(Node *node) {
	if (auto value = Object::cast_to<Skeleton3D>(node)) {
		return value;
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		if (auto value = find_skeleton(node->get_child(i))) {
			return value;
		}
	}
	return nullptr;
}
Ref<AnimationNodeAnimation> clip(const String &name) {
	Ref<AnimationNodeAnimation> node;
	node.instantiate();
	node->set_animation(name);
	return node;
}
void tint(Node *node, const Color &color, bool detailed = false) {
	if (auto mesh = Object::cast_to<MeshInstance3D>(node); mesh && mesh->get_mesh().is_valid()) {
		for (int surface = 0; surface < (detailed ? mesh->get_mesh()->get_surface_count() : MIN(1, mesh->get_mesh()->get_surface_count())); ++surface) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_albedo(surface > 0 ? color.darkened(.65f) : color);
			mat->set_roughness(.8);
			mesh->set_surface_override_material(surface, mat);
		}
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		tint(node->get_child(i), color, detailed);
	}
}
} //namespace

Ref<PackedScene> LastLightGame::load_asset_scene(const String &path) {
	if (path.is_empty()) {
		return Ref<PackedScene>();
	}
	if (scene_cache.has(path)) {
		return scene_cache[path];
	}
	Ref<PackedScene> packed;
	if (path.get_extension().to_lower() == "glb") {
		Ref<GLTFDocument> document;
		document.instantiate();
		Ref<GLTFState> state;
		state.instantiate();
		if (document->append_from_file(path, state) != OK) {
			return packed;
		}
		Node *node = document->generate_scene(state);
		if (!node) {
			return packed;
		}
		packed.instantiate();
		if (packed->pack(node) != OK) {
			packed.unref();
		}
		memdelete(node);
	} else if (ResourceLoader::exists(path)) {
		packed = ResourceLoader::load(path);
	}
	if (packed.is_valid()) {
		scene_cache[path] = packed;
	}
	return packed;
}

void LastLightGame::try_asset(Actor &a) {
	Ref<PackedScene> packed = load_asset_scene(assets.get("survivor_scene", String()));
	if (packed.is_null()) {
		return;
	}
	Node *node = packed->instantiate();
	auto visual = Object::cast_to<Node3D>(node);
	if (!visual) {
		memdelete(node);
		return;
	}
	a.body->add_child(visual);
	const float scale = assets.get("survivor_scale", .94);
	visual->set_scale(Vector3(1, 1, 1) * scale);
	a.visual_yaw_offset = Math::deg_to_rad(float(assets.get("survivor_yaw_degrees", 180.)));
	a.visual->hide();
	a.visual = visual;
	a.animation = find_player(visual);
	a.skeleton = find_skeleton(visual);
	const Color colors[] = { Color(.22, .55, .64), Color(.66, .48, .3), Color(.34, .55, .43), Color(.5, .4, .64), Color(.26, .52, .59) };
	tint(visual, a.stats.survivor ? colors[(a.stats.id - 1) % 5] : a.bot_role == 0 ? Color(.6, .25, .23)
																				   : Color(.6, .5, .25),
			compound_world);
	if (a.animation) {
		Ref<PackedScene> actions = load_asset_scene(assets.get("actions_scene", String()));
		if (actions.is_valid()) {
			Node *extra = actions->instantiate();
			if (auto player = find_player(extra); player && player->has_animation_library(StringName())) {
				a.animation->add_animation_library("extra", player->get_animation_library(StringName()));
			}
			memdelete(extra);
		}
		List<StringName> animations;
		a.animation->get_animation_list(&animations);
		for (const auto &name : animations) {
			Ref<Animation> animation = a.animation->get_animation(name);
			if (String(name).contains("Loop") || String(name) == "Sword_Idle") {
				animation->set_loop_mode(Animation::LOOP_LINEAR);
			}
		}
		setup_animation_graph(a);
	}
	visual->set_rotation(Vector3(0, a.yaw + a.visual_yaw_offset, 0));
	if (a.skeleton && a.skeleton->find_bone("hand_r") >= 0) {
		a.animation->play("Pistol_Aim_Neutral");
		a.animation->advance(0);
		a.animation->stop(true);
		Basis hand = a.skeleton->get_global_transform().basis * a.skeleton->get_bone_global_pose(a.skeleton->find_bone("hand_r")).basis;
		Basis calibration = hand.orthonormalized().inverse() * a.visual->get_global_transform().basis.orthonormalized();
		a.weapon_socket = memnew(BoneAttachment3D);
		a.skeleton->add_child(a.weapon_socket);
		a.weapon_socket->set_bone_name("hand_r");
		for (const String kind : { String("pistol"), String("sword") }) {
			Ref<PackedScene> weapon = load_asset_scene(assets.get(kind + "_scene", String()));
			if (weapon.is_null()) {
				continue;
			}
			auto model = Object::cast_to<Node3D>(weapon->instantiate());
			if (!model) {
				continue;
			}
			a.weapon_socket->add_child(model);
			model->hide();
			Dictionary transforms = assets.get(kind + "_transform", Dictionary());
			Array translation = transforms.get("position", Array());
			Array rotation = transforms.get("rotation", Array());
			if (translation.size() == 3) {
				model->set_position(Vector3(translation[0], translation[1], translation[2]));
			}
			if (rotation.size() == 3) {
				model->set_rotation_degrees(Vector3(rotation[0], rotation[1], rotation[2]));
			}
			model->set_basis(calibration * model->get_basis());
			if (kind == "pistol") {
				a.pistol_model = model;
			} else {
				a.sword_model = model;
			}
		}
	}
	if (a.weapon_socket) {
		a.gun_models.push_back(a.pistol_model);
		Array scenes = assets.get("gun_scenes", Array());
		for (int index = 1; index < 4; ++index) {
			Node3D *model = nullptr;
			if (index < scenes.size()) {
				auto packed = load_asset_scene(scenes[index]);
				if (packed.is_valid()) {
					model = Object::cast_to<Node3D>(packed->instantiate());
					if (model) {
						a.weapon_socket->add_child(model);
						model->set_basis(a.pistol_model ? a.pistol_model->get_basis() : Basis());
						model->hide();
					}
				}
			}
			a.gun_models.push_back(model);
		}
		auto bottle = load_asset_scene("res://assets/arctic/survival-kit/bottle.glb");
		if (bottle.is_valid()) {
			a.held_item = Object::cast_to<Node3D>(bottle->instantiate());
			if (a.held_item) {
				a.weapon_socket->add_child(a.held_item);
				a.held_item->set_scale(Vector3(.35, .35, .35));
				a.held_item->hide();
			}
		}
	}
	if (compound_world) {
		add_compound_gear(a);
	}
}

void LastLightGame::setup_animation_graph(Actor &a) {
	if (!a.animation->has_animation("Idle_Loop")) {
		return;
	}
	Ref<AnimationNodeBlendTree> graph;
	graph.instantiate();
	Ref<AnimationNodeBlendTree> ground;
	ground.instantiate();
	Ref<AnimationNodeBlendSpace2D> movement;
	movement.instantiate();
	movement->set_min_space(Vector2(-5, -5));
	movement->set_max_space(Vector2(5, 5));
	movement->add_blend_point(clip("Idle_Loop"), Vector2());
	for (float speed : { 2.f, 3.5f, 5.f }) {
		for (int direction = 0; direction < 8; ++direction) {
			float angle = direction * Math::TAU / 8;
			Vector2 point(Math::sin(angle) * speed, Math::cos(angle) * speed);
			auto node = clip(speed < 3 ? "Walk_Loop" : speed < 4 ? "Jog_Fwd_Loop"
																 : "Sprint_Loop");
			if (point.y < -.1) {
				node->set_play_mode(AnimationNodeAnimation::PLAY_MODE_BACKWARD);
			}
			movement->add_blend_point(node, point);
		}
	}
	Ref<AnimationNodeBlendSpace1D> crouch;
	crouch.instantiate();
	crouch->set_min_space(0);
	crouch->set_max_space(1.5);
	crouch->add_blend_point(clip("Crouch_Idle_Loop"), 0);
	crouch->add_blend_point(clip("Crouch_Fwd_Loop"), 1.5);
	ground->add_node("Move", movement);
	ground->add_node("Crouch", crouch);
	Ref<AnimationNodeBlend2> stance;
	stance.instantiate();
	ground->add_node("Stance", stance);
	ground->connect_node("Stance", 0, "Move");
	ground->connect_node("Stance", 1, "Crouch");
	ground->add_node("CarryClip", clip("extra/Walk_Carry_Loop"));
	Ref<AnimationNodeBlend2> carry;
	carry.instantiate();
	ground->add_node("Carry", carry);
	ground->connect_node("Carry", 0, "Stance");
	ground->connect_node("Carry", 1, "CarryClip");
	ground->connect_node("output", 0, "Carry");
	Ref<AnimationNodeStateMachine> state;
	state.instantiate();
	state->add_node("Ground", ground);
	state->add_node("JumpStart", clip("Jump_Start"));
	state->add_node("JumpLoop", clip("Jump_Loop"));
	state->add_node("Landing", clip("Jump_Land"));
	state->add_node("Downed", clip("Death01"));
	Ref<AnimationNodeBlendSpace1D> swim;
	swim.instantiate();
	swim->set_min_space(0);
	swim->set_max_space(1.2);
	swim->add_blend_point(clip("Swim_Idle_Loop"), 0);
	swim->add_blend_point(clip("Swim_Fwd_Loop"), 1.2);
	state->add_node("Swimming", swim);
	const char *states[] = { "Ground", "JumpStart", "JumpLoop", "Landing", "Downed", "Swimming" };
	for (const char *from : states) {
		for (const char *to : states) {
			if (String(from) != String(to)) {
				Ref<AnimationNodeStateMachineTransition> transition;
				transition.instantiate();
				transition->set_xfade_time(String(to) == "Downed" ? .18 : .12);
				state->add_transition(from, to, transition);
			}
		}
	}
	graph->add_node("Movement", state);
	Ref<AnimationNodeBlendSpace1D> aim;
	aim.instantiate();
	aim->set_min_space(-1);
	aim->set_max_space(1);
	aim->add_blend_point(clip("Pistol_Aim_Down"), -1);
	aim->add_blend_point(clip("Pistol_Aim_Neutral"), 0);
	aim->add_blend_point(clip("Pistol_Aim_Up"), 1);
	graph->add_node("AimPose", aim);
	graph->add_node("SwordIdle", clip("Sword_Idle"));
	graph->add_node("SwordBlock", clip("extra/Sword_Block"));
	Ref<AnimationNodeBlend2> block;
	block.instantiate();
	graph->add_node("Block", block);
	graph->connect_node("Block", 0, "SwordIdle");
	graph->connect_node("Block", 1, "SwordBlock");
	Ref<AnimationNodeBlend2> weapon;
	weapon.instantiate();
	graph->add_node("Weapon", weapon);
	graph->connect_node("Weapon", 0, "AimPose");
	graph->connect_node("Weapon", 1, "Block");
	Ref<AnimationNodeBlend2> upper;
	upper.instantiate();
	upper->set_filter_enabled(true);
	Ref<AnimationNodeOneShot> action;
	action.instantiate();
	action->set_fade_in_time(.12);
	action->set_fade_out_time(.18);
	action->set_filter_enabled(true);
	Ref<Animation> sample = a.animation->get_animation("Pistol_Aim_Neutral");
	for (int t = 0; t < sample->get_track_count(); ++t) {
		NodePath path = sample->track_get_path(t);
		if (!path.get_subname_count()) {
			continue;
		}
		String bone = path.get_subname(0);
		if (bone.contains("spine") || bone.contains("clavicle") || bone.contains("arm") || bone.contains("hand") || bone.contains("finger") || bone.contains("thumb") || bone.contains("index") || bone.contains("middle") || bone.contains("ring") || bone.contains("pinky") || bone.contains("neck") || bone.to_lower().contains("head")) {
			upper->set_filter_path(path, true);
			action->set_filter_path(path, true);
		}
	}
	graph->add_node("Upper", upper);
	graph->connect_node("Upper", 0, "Movement");
	graph->connect_node("Upper", 1, "Weapon");
	graph->add_node("ActionClip", clip("Pistol_Shoot"));
	Ref<AnimationNodeTimeScale> timescale;
	timescale.instantiate();
	graph->add_node("ActionSpeed", timescale);
	graph->connect_node("ActionSpeed", 0, "ActionClip");
	graph->add_node("Action", action);
	graph->connect_node("Action", 0, "Upper");
	graph->connect_node("Action", 1, "ActionSpeed");
	graph->connect_node("output", 0, "Action");
	a.animation_tree = memnew(AnimationTree);
	a.body->add_child(a.animation_tree);
	a.animation_tree->set_animation_player(a.animation_tree->get_path_to(a.animation));
	a.animation_tree->set_root_animation_node(graph);
	a.animation_tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
	a.animation_tree->set_active(true);
	Ref<AnimationNodeStateMachinePlayback> playback = a.animation_tree->get("parameters/Movement/playback");
	if (playback.is_valid()) {
		playback->start("Ground");
	}
}
void LastLightGame::update_animation_graph(Actor &a, float dt, const Vector3 &velocity) {
	if (DisplayServer::get_singleton()->get_name() == "headless" && physics_frames > 5 && script_case != "assets" && script_case != "arctic_animation" && script_case!="compound_animation") {
		return;
	}
	auto tree = a.animation_tree;
	float speed = velocity.length();
	bool dead = a.stats.state == lastlight::State::Dead || a.stats.state == lastlight::State::Downed;
	bool grounded = physics_frames < 4 && !automated_run ? true : a.body->is_on_floor();
	bool swimming = arctic && a.body->get_position().y < -.5;
	float alpha = MIN(1.f, dt * 8);
	a.smooth_speed = Math::lerp(a.smooth_speed, speed, alpha);
	a.stance_weight = Math::lerp(a.stance_weight, a.crouching ? 1.f : 0.f, alpha);
	a.aim_weight = Math::lerp(a.aim_weight, dead || swimming ? 0.f : (a.use_sword && a.stats.inventory.sword ? 1.f : a.stats.inventory.pistol ? (a.aiming ? 1.f : .35f)
																																			  : 0.f),
			alpha);
	Vector3 local_velocity = Basis(Vector3(0, 1, 0), a.yaw).inverse().xform(velocity);
	Vector2 direction(local_velocity.x, -local_velocity.z);
	if (direction.length() > .02) {
		direction = direction.normalized() * a.smooth_speed;
	}
	tree->set("parameters/Movement/Ground/Move/blend_position", direction);
	tree->set("parameters/Movement/Ground/Crouch/blend_position", speed);
	tree->set("parameters/Movement/Ground/Stance/blend_amount", a.stance_weight);
	tree->set("parameters/Movement/Ground/Carry/blend_amount", arctic && a.stats.inventory.fuel > 0 && !a.stats.inventory.pistol ? MIN(1.f, speed) : 0.f);
	tree->set("parameters/Movement/Swimming/blend_position", speed);
	tree->set("parameters/Upper/blend_amount", a.aim_weight);
	tree->set("parameters/Weapon/blend_amount", a.use_sword ? 1.f : 0.f);
	tree->set("parameters/Block/blend_amount", a.use_sword && a.aiming ? 1.f : 0.f);
	tree->set("parameters/AimPose/blend_position", a.stats.id == 1 ? CLAMP(-camera_pitch, -1.f, 1.f) : 0.f);
	Ref<AnimationNodeStateMachinePlayback> playback = tree->get("parameters/Movement/playback");
	if (playback.is_valid()) {
		String current = playback->get_current_node();
		String desired = current;
		if (dead) {
			desired = "Downed";
		} else if (swimming) {
			desired = "Swimming";
		} else if (!grounded && a.was_grounded) {
			desired = "JumpStart";
		} else if (!grounded && current == "JumpStart" && playback->get_current_play_pos() > .18) {
			desired = "JumpLoop";
		} else if (grounded && !a.was_grounded) {
			desired = "Landing";
		} else if (grounded && (current == "Landing" && playback->get_current_play_pos() > .16 || current == "Downed" || current == "Swimming" || current == "JumpLoop" || current == "JumpStart" && playback->get_current_play_pos() > .18)) {
			desired = "Ground";
		}
		if (desired != current) {
			playback->travel(desired);
		}
	}
	a.was_grounded = grounded;
	a.idle_clock = speed < .08 && !a.stats.busy() ? a.idle_clock + dt : 0;
	String state = String(lastlight::state_name(a.stats.state)) + ":" + a.environment_action;
	if (state != a.animation_state) {
		String name;
		float duration = 1;
		bool full = false;
		if (a.stats.state == lastlight::State::Fire) {
			name = "Pistol_Shoot";
			duration = MAX(.1f, lastlight::weapon_spec(a.stats.inventory.weapon_kind).cooldown);
		}
		if (a.stats.state == lastlight::State::Reload) {
			name = "Pistol_Reload";
			duration = lastlight::weapon_spec(a.stats.inventory.weapon_kind).reload;
		}
		if (a.stats.state == lastlight::State::Melee) {
			name = a.stats.inventory.sword && a.use_sword ? (a.melee_combo % 3 == 0 ? "extra/Sword_Regular_A" : a.melee_combo % 3 == 1 ? "extra/Sword_Regular_B"
																																	   : "extra/Sword_Regular_C")
														  : "Punch_Jab";
			duration = .55;
		}
		if (a.stats.state == lastlight::State::Heal) {
			name = "extra/Consume";
			duration = 1.5;
			full = true;
		}
		if (a.stats.state == lastlight::State::Revive) {
			name = "Fixing_Kneeling";
			duration = 2;
			full = true;
		}
		if (a.stats.state == lastlight::State::Stunned) {
			name = "Hit_Chest";
			duration = .25;
		}
		if (a.stats.state == lastlight::State::Interact) {
			duration = MAX(.1f, a.stats.timer);
			full = a.environment_action != "smoke" && a.environment_action != "decoy";
			name = a.environment_action == "crate" ? "extra/Chest_Open" : a.environment_action == "door"					   ? "Interact"
					: a.environment_action == "smoke" || a.environment_action == "decoy"									   ? "extra/OverhandThrow"
					: a.environment_action == "roll"																		   ? "Roll"
					: a.environment_action == "generator"																	   ? "Push_Loop"
					: a.environment_action == "relay" || a.environment_action == "radio" || a.environment_action == "security" ? "Fixing_Kneeling"
																															   : "extra/Consume";
		}
		Ref<AnimationNodeBlendTree> graph = tree->get_root_animation_node();
		Ref<AnimationNodeOneShot> action = graph->get_node("Action");
		if (!name.is_empty() && a.animation->has_animation(name)) {
			action->set_filter_enabled(!full);
			Ref<AnimationNodeAnimation> clip_node = graph->get_node("ActionClip");
			clip_node->set_animation(name);
			tree->set("parameters/ActionSpeed/scale", a.animation->get_animation(name)->get_length() / duration);
			tree->set("parameters/Action/request", AnimationNodeOneShot::ONE_SHOT_REQUEST_FIRE);
		} else if (a.stats.state == lastlight::State::Idle || dead) {
			tree->set("parameters/Action/request", AnimationNodeOneShot::ONE_SHOT_REQUEST_FADE_OUT);
		}
		a.animation_state = state;
	}
	tree->advance(dt);
	// Standard UAL has forward clips. Reverse gait handles backing up; pelvis
	// steering/counter-rotation keeps the aim layer stable while side-stepping.
	if (a.skeleton && a.aiming && speed > .15 && !dead && !swimming) {
		float yaw = compound_world ? Math::atan2(local_velocity.x, -local_velocity.z) : Math::atan2(velocity.x, -velocity.z);
		if (Math::abs(yaw) > Math::PI / 2) {
			yaw = std::remainder(yaw + Math::PI, float(Math::TAU));
		}
		yaw = CLAMP(yaw, -.8f, .8f);
		int pelvis = a.skeleton->find_bone("pelvis"), spine = a.skeleton->find_bone("spine_02");
		auto rotate = [&](int bone, float angle) {if(bone<0){return;
}int parent=a.skeleton->get_bone_parent(bone);Basis basis=a.skeleton->get_global_basis();if(parent>=0){basis=basis*a.skeleton->get_bone_global_pose(parent).basis;
}Vector3 axis=basis.inverse().xform(Vector3(0,1,0)).normalized();a.skeleton->set_bone_pose_rotation(bone,Quaternion(axis,angle)*a.skeleton->get_bone_pose_rotation(bone)); };
		rotate(pelvis, yaw);
		rotate(spine, -yaw);
	}
	if (compound_world) {
		update_equipment_pose(a);
	}
}
