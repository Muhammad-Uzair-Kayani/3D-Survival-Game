#include "last_light_game.h"
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/packed_scene.h"
#include "servers/display_server.h"
#include "servers/navigation_server_3d.h"
#include <algorithm>
#include <cmath>
#include <set>
using namespace lastlight;
namespace {
Dictionary read_json(const String &path) {
	auto f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		return {};
	}
	Variant value = JSON::parse_string(f->get_as_text());
	return value.get_type() == Variant::DICTIONARY ? Dictionary(value) : Dictionary();
}
AnimationPlayer *find_animation(Node *n) {
	if (auto p = Object::cast_to<AnimationPlayer>(n)) {
		return p;
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		if (auto p = find_animation(n->get_child(i))) {
			return p;
		}
	}
	return nullptr;
}
float flat_distance(const Vector3 &a, const Vector3 &b) {
	Vector3 d = a - b;
	d.y = 0;
	return d.length();
}
Color survivor_color(int i) {
	Color c[] = { Color(.2, .65, .9), Color(.9, .65, .18), Color(.4, .8, .35), Color(.75, .4, .85), Color(.2, .8, .7) };
	return c[(i - 1) % 5];
}
} //namespace
void LastLightGame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("attach_frontend"), &LastLightGame::attach_frontend);
	ClassDB::bind_method(D_METHOD("get_game_state"), &LastLightGame::get_game_state);
	ClassDB::bind_method(D_METHOD("game_command", "command", "payload"), &LastLightGame::game_command, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("apply_options", "options"), &LastLightGame::apply_options);
}
LastLightGame::~LastLightGame() {
	save_profiles();
	stop_python();
}
LastLightGame::Actor *LastLightGame::find_actor(int id) {
	for (auto &a : actors) {
		if (a.stats.id == id) {
			return &a;
		}
	}
	return nullptr;
}
const LastLightGame::Actor *LastLightGame::find_actor(int id) const {
	for (auto &a : actors) {
		if (a.stats.id == id) {
			return &a;
		}
	}
	return nullptr;
}
void LastLightGame::_notification(int what) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (what == NOTIFICATION_READY) {
		setup();
	}
	if (what == NOTIFICATION_PHYSICS_PROCESS) {
		physics(float(get_physics_process_delta_time()));
	}
	if (what == NOTIFICATION_PROCESS) {
		poll_python();
		if (camera_rig && find_actor(camera_focus)) {
			camera_rig->set_global_position(find_actor(camera_focus)->body->get_global_position() + Vector3(0, 1.4, overview ? -20 : 0));
			camera->set_projection(overview ? Camera3D::PROJECTION_ORTHOGONAL : Camera3D::PROJECTION_PERSPECTIVE);
			if (overview) {
				camera->set_size(overview_zoom);
			}
			arm->set_mask(overview ? 0 : 1);
			camera_rig->set_rotation(Vector3(camera_pitch, camera_yaw, 0));
			auto focus = find_actor(camera_focus);
			const bool aim = focus && focus->stats.active() && focus->aiming && !paused;
			const float blend = MIN(1.f, float(get_process_delta_time()) * 10);
			arm->set_length(Math::lerp(arm->get_length(), overview ? 65.f : (aim ? 2.8f : 4.8f), blend));
			arm->set_position(Vector3(aim ? .45f : .2f, 0, 0));
			camera->set_fov(Math::lerp(camera->get_fov(), aim ? 55.f : 70.f, blend));
		}
	}
	if (what == NOTIFICATION_EXIT_TREE) {
		stop_python();
	}
}
void LastLightGame::setup() {
	if (FileAccess::exists("res://game-content.pck") && PackedData::get_singleton()->add_pack("res://game-content.pck", true, 0) != OK) {
		print_error("Game content pack could not be mounted");
		get_tree()->quit(1);
		return;
	}
	config = read_json("res://content/game.json");
	assets = read_json("res://content/assets.json");
	seed = int(config.get("seed", 42));
	spawn_count = int(config.get("active_bots", 2));
	List<String> args = OS::get_singleton()->get_cmdline_user_args();
	bool bad_args = false;
	String trace_path, requested_world;
	for (auto *e = args.front(); e; e = e->next()) {
		String key = e->get();
		auto next = [&]() { if(e->next() && !e->next()->get().begins_with("--")) { e=e->next(); return e->get(); } bad_args=true; return String(); };
		auto integer = [&](int low, int high) { String value=next(); if(!value.is_valid_int() || value.to_int()<low || value.to_int()>high) {bad_args=true; return low;} return int(value.to_int()); };
		if (key == "--autoplay") {
			autoplay = true;
		} else if (key == "--smoke") {
			smoke = true;
		} else if (key == "--frames") {
			max_frames = integer(60, 180000);
		} else if (key == "--seed") {
			seed = integer(0, 2147483647);
		} else if (key == "--bots") {
			spawn_count = integer(0, 8);
		} else if (key == "--report") {
			report_path = next();
		} else if (key == "--case") {
			script_case = next();
		} else if (key == "--python") {
			python_executable = next();
		} else if (key == "--policy") {
			policy_path = next();
		} else if (key == "--controller") {
			controller = next();
		} else if (key == "--world") {
			requested_world = next();
			if (requested_world != "arctic" && requested_world != "lab" && requested_world != "classic") {
				bad_args = true;
			}
		} else if (key == "--trace") {
			trace_path = next();
		} else {
			bad_args = true;
		}
	}
	if (controller != "rule" && controller != "random" && controller != "neural" && controller != "nomemory") {
		bad_args = true;
	}
	if (bad_args) {
		print_error("Invalid Last Light arguments. Use --bots 0..8, --seed 0..2147483647, --frames 60..180000, --controller rule|random|neural|nomemory, --world arctic|classic|lab, --report PATH, --trace PATH, --python PATH, --policy PATH.");
		get_tree()->quit(64);
		return;
	}
	if (!trace_path.is_empty()) {
		DirAccess::make_dir_recursive_absolute(trace_path.get_base_dir());
		trace_file = FileAccess::open(trace_path, FileAccess::WRITE);
		if (trace_file.is_null()) {
			print_error("Cannot open decision trace");
			get_tree()->quit(64);
			return;
		}
	}
	arctic = requested_world.is_empty() ? (!smoke || script_case.begins_with("arctic") || script_case == "profiles") : requested_world == "arctic";
	if (requested_world == "classic") {
		arctic = true;
	}
	if (script_case.begins_with("compound")) {
		arctic = true;
	}
	compound_world = arctic && requested_world != "classic" && (!smoke || script_case.begins_with("compound"));
	world_extent = arctic ? 300 : 40;
	if (compound_world) {
		save_path = "user://save-frostworks.json";
	}
	session_id = itos(OS::get_singleton()->get_process_id()) + "-" + itos(OS::get_singleton()->get_ticks_usec());
	automated_run = autoplay || smoke;
	rng.seed(seed);
	set_process(true);
	set_physics_process(true);
	set_process_input(true);
    if(automated_run){
        // Simulation-clock episodes must not depend on the worker's wall-clock
        // completion frame when a navigation map/region is merged.
        NavigationServer3D::get_singleton()->map_set_use_async_iterations(get_world_3d()->get_navigation_map(),false);
        ProjectSettings::get_singleton()->set("navigation/world/region_use_async_iterations",false);
    }
	create_level();
	load_profiles();
	if (!autoplay && !smoke) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
	}
	if (!python_executable.is_empty()) {
		start_python();
	}
	notify("Last Light ready. F1 shows diagnostics. Esc opens the mission menu.");
}
Node3D *LastLightGame::make_box(Node3D *parent, const Vector3 &position, const Vector3 &size, const Color &color, bool collision) {
	Node3D *root = collision ? static_cast<Node3D *>(memnew(StaticBody3D)) : memnew(Node3D);
	parent->add_child(root);
	root->set_position(position);
	auto visual = memnew(MeshInstance3D);
	Ref<BoxMesh> mesh;
	mesh.instantiate();
	mesh->set_size(size);
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(color);
	material->set_roughness(.9);
	Ref<Material> themed;
	if (arctic && parent == region && collision && size.y < .5) {
		themed = level_material(true);
	}
	if (!arctic && parent == region && collision) {
		themed = level_material(size.y < 1);
	}
	mesh->set_material(themed.is_valid() ? themed : Ref<Material>(material));
	visual->set_mesh(mesh);
	root->add_child(visual);
	if (collision) {
		auto body = Object::cast_to<StaticBody3D>(root);
		body->set_collision_layer(1);
		body->set_collision_mask(2);
		auto shape_node = memnew(CollisionShape3D);
		Ref<BoxShape3D> shape;
		shape.instantiate();
		shape->set_size(size);
		shape_node->set_shape(shape);
		body->add_child(shape_node);
	}
	return root;
}
void LastLightGame::create_level() {
	if (arctic) {
		create_arctic_level();
		create_presentation();
		return;
	}
	level_root = memnew(Node3D);
	level_root->set_name("ClientWorld");
	add_child(level_root);
	region = memnew(NavigationRegion3D);
	region->set_name("WalkableWorld");
	level_root->add_child(region);
	Array rows = config.get("map", Array());
	if (rows.is_empty()) {
		get_tree()->quit(1);
		return;
	}
	if (rows.size() != 17) {
		notify("Invalid map: expected 17 rows");
		get_tree()->quit(1);
		return;
	}
	for (int z = 0; z < rows.size(); ++z) {
		String row = rows[z];
		if (row.length() != 21) {
			notify("Invalid map: expected 21 columns");
			get_tree()->quit(1);
			return;
		}
		for (int x = 0; x < row.length(); ++x) {
			if (row[x] != '#' && row[x] != '.') {
				notify("Invalid map symbol");
				get_tree()->quit(1);
				return;
			}
		}
		map_rows.push_back(row);
	}
	int width = map_rows[0].length(), height = map_rows.size();
	make_box(region, Vector3((width - 1), -.25, (height - 1)), Vector3(width * 2, .5, height * 2), Color(.19, .23, .21), true);
	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width;) {
			if (map_rows[z][x] != '#') {
				++x;
				continue;
			}
			int begin = x;
			while (x < width && map_rows[z][x] == '#') {
				++x;
			}
			make_box(region, Vector3(float(begin + x - 1), 1.5f, z * 2.f), Vector3((x - begin) * 2.f, 3, 2), Color(.26, .3, .34), true);
		}
	}
	gate = Object::cast_to<StaticBody3D>(make_box(region, Vector3(20, 1.5, 14), Vector3(.3, 3, 2), Color(.55, .22, .15), true));
	gate->set_name("CooperativeGate");
	gate_mesh = Object::cast_to<MeshInstance3D>(gate->get_child(0));
	control_a = Vector3(18, .05, 12);
	control_b = Vector3(18, .05, 16);
	make_box(level_root, control_a, Vector3(1.3, .1, 1.3), Color(.2, .6, .8));
	make_box(level_root, control_b, Vector3(1.3, .1, 1.3), Color(.2, .6, .8));
	key_position = Vector3(12 + int(rng() % 3) * 2, .4, 24 + int(rng() % 2) * 2);
	radio_position = Vector3(28, .6, 14);
	exit_position = Vector3(36, 0, 24);
	make_box(level_root, radio_position, Vector3(.7, 1.2, .6), Color(.3, .55, .3));
	make_box(level_root, exit_position + Vector3(0, .03, 0), Vector3(2, .06, 2), Color(.15, .8, .3));
	struct SpawnItem {
		const char *kind;
		Vector3 p;
	};
	SpawnItem loot[] = { { "pistol", { 6, .4, 6 } }, { "sword", { 8, .4, 20 } }, { "pistol", { 12, .4, 18 } }, { "ammo", { 14, .4, 10 } }, { "medkit", { 12, .4, 24 } }, { "food", { 6, .4, 26 } }, { "key", key_position } };
	int object_id = 101;
	for (auto &s : loot) {
		if (s.kind != String("key")) {
			s.p.x += (int(rng() % 5) - 2) * .15f;
			s.p.z += (int(rng() % 5) - 2) * .15f;
		}
		Item item{ object_id++, s.kind, s.p };
		item.visual = make_box(level_root, s.p, Vector3(.55, .5, .55), s.kind == String("key") ? Color(.95, .8, .1) : Color(.25, .7, .8));
		items.push_back(item);
	}
	decorate_level();
	Ref<NavigationMesh> nav;
	nav.instantiate();
	nav->set_parsed_geometry_type(NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS);
	nav->set_collision_mask(1);
	nav->set_cell_size(.25);
	nav->set_cell_height(.25);
	nav->set_agent_radius(.5);
	nav->set_agent_height(1.75);
	nav->set_agent_max_climb(.25);
	nav->set_agent_max_slope(45);
	region->set_navigation_mesh(nav);
	region->bake_navigation_mesh(false);
	for (int id = 1; id <= 5; ++id) {
		create_actor(id, Vector3(4, .1f, 4 + (id - 1) * 4), true, survivor_color(id));
	}
	for (int n = 0; n < 4; ++n) {
		int id = 10 + n;
		int lane = int(rng() % 3);
		create_actor(id, Vector3(28 + lane * 2, .1, n == 3 ? 6 : 10 + n * 8), false, n == 0 ? Color(.8, .22, .18) : Color(.8, .55, .2));
		Actor &a = actors.back();
		a.bot_role = n % 2 == 0 ? 0 : 1 + int(rng() % 2);
		if (a.bot_role == 0) {
			a.stats.inventory.pistol = true;
			a.stats.inventory.rounds = 6;
			a.stats.inventory.reserve = 0;
			a.reason = "Aggressor";
		} else {
			a.reason = a.bot_role == 1 ? "Scavenger" : "Panicked runner";
		}
	}
	configure_bots(spawn_count);
	create_presentation();
}
void LastLightGame::create_presentation() {
	auto light = memnew(DirectionalLight3D);
	level_root->add_child(light);
	light->set_rotation_degrees(Vector3(-55, -30, 0));
	light->set_shadow(compound_world);
	if (compound_world) {
		light->set_shadow_mode(DirectionalLight3D::SHADOW_ORTHOGONAL);
		light->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, 55);
		RenderingServer::get_singleton()->directional_shadow_atlas_set_size(1024, true);
	}
	light->set_param(Light3D::PARAM_ENERGY, arctic ? .55 : 1.1);
	if (arctic) {
		light->set_color(Color(.65, .8, 1));
	}
	if (compound_world) {
		light->set_rotation_degrees(Vector3(-43, -35, 0));
		light->set_color(Color(1, .87, .68));
		light->set_param(Light3D::PARAM_ENERGY, 1.0);
	}
	auto env_node = memnew(WorldEnvironment);
	level_root->add_child(env_node);
	Ref<Environment> env;
	env.instantiate();
	env->set_background(Environment::BG_COLOR);
	env->set_bg_color(arctic ? Color(.015, .035, .075) : Color(.16, .22, .29));
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_ambient_light_color(arctic ? Color(.38, .52, .72) : Color(.7, .75, .8));
	env->set_ambient_light_energy(arctic ? .4 : .65);
	if (arctic) {
		env->set_fog_enabled(true);
		env->set_fog_light_color(Color(.08, .16, .25));
		env->set_fog_density(.0015);
	}
	if (compound_world) {
		env->set_bg_color(Color(.36, .47, .52));
		env->set_ambient_light_color(Color(.64, .75, .84));
		env->set_ambient_light_energy(.38);
		env->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
		env->set_tonemap_exposure(.95);
		env->set_fog_light_color(Color(.48, .59, .65));
		env->set_fog_density(.001);
	}
	env_node->set_environment(env);
	camera_rig = memnew(Node3D);
	level_root->add_child(camera_rig);
	arm = memnew(SpringArm3D);
	camera_rig->add_child(arm);
	arm->set_length(4.8);
	arm->set_mask(1);
	arm->set_margin(.2);
	camera = memnew(Camera3D);
	arm->add_child(camera);
	camera->set_current(true);
	camera->set_fov(70);
	camera->set_far(arctic ? 230 : 90);
	auto canvas = memnew(CanvasLayer);
	add_child(canvas);
	hud = memnew(Label);
	canvas->add_child(hud);
	hud->set_position(Vector2(14, 10));
	hud->add_theme_font_size_override("font_size", 18);
	hud->add_theme_color_override("font_color", Color(.95, .98, 1));
	message_label = memnew(Label);
	canvas->add_child(message_label);
	message_label->set_position(Vector2(14, 610));
	message_label->add_theme_font_size_override("font_size", 16);
	auto crosshair = memnew(Label);
	legacy_crosshair = crosshair;
	canvas->add_child(crosshair);
	crosshair->set_anchors_and_offsets_preset(Control::PRESET_CENTER);
	crosshair->set_text("+");
	crosshair->add_theme_font_size_override("font_size", 24);
}
void LastLightGame::create_actor(int id, const Vector3 &position, bool survivor, const Color &color, int role) {
	Actor a;
	a.bot_role = role;
	a.stats.id = id;
	a.stats.survivor = survivor;
	a.personality = lastlight::baseline_personality(id);
	a.body = memnew(CharacterBody3D);
	a.body->set_name("Actor" + itos(id));
	level_root->add_child(a.body);
	a.body->set_position(position);
	a.spawn_position = position;
	a.body->set_collision_layer(2);
	a.body->set_collision_mask(3);
	a.body->set_floor_snap_length(.35);
	a.body->set_floor_max_angle(Math::deg_to_rad(45.f));
	a.body->set_floor_stop_on_slope_enabled(true);
	auto collider = memnew(CollisionShape3D);
	a.body->add_child(collider);
	a.collider = collider;
	Ref<CapsuleShape3D> capsule;
	capsule.instantiate();
	a.capsule = capsule;
	capsule->set_radius(.3);
	capsule->set_height(1.65);
	collider->set_shape(capsule);
	collider->set_position(Vector3(0, .85, 0));
	a.visual = memnew(Node3D);
	a.body->add_child(a.visual);
	make_box(a.visual, Vector3(0, 1.1, 0), Vector3(.55, .65, .3), color);
	make_box(a.visual, Vector3(0, 1.65, 0), Vector3(.35, .35, .35), color.lightened(.25));
	a.left_leg = memnew(Node3D);
	a.visual->add_child(a.left_leg);
	a.left_leg->set_position(Vector3(-.16, .75, 0));
	make_box(a.left_leg, Vector3(0, -.3, 0), Vector3(.2, .6, .22), color.darkened(.35));
	a.right_leg = memnew(Node3D);
	a.visual->add_child(a.right_leg);
	a.right_leg->set_position(Vector3(.16, .75, 0));
	make_box(a.right_leg, Vector3(0, -.3, 0), Vector3(.2, .6, .22), color.darkened(.35));
	a.left_arm = memnew(Node3D);
	a.visual->add_child(a.left_arm);
	a.left_arm->set_position(Vector3(-.4, 1.35, 0));
	make_box(a.left_arm, Vector3(0, -.25, 0), Vector3(.16, .5, .18), color);
	a.right_arm = memnew(Node3D);
	a.visual->add_child(a.right_arm);
	a.right_arm->set_position(Vector3(.4, 1.35, 0));
	make_box(a.right_arm, Vector3(0, -.25, 0), Vector3(.16, .5, .18), color);
	auto gun_root = make_box(a.right_arm, Vector3(0, -.45, -.15), Vector3(.12, .13, .32), Color(.15, .16, .18));
	a.pistol = Object::cast_to<MeshInstance3D>(gun_root->get_child(0));
	auto blade = make_box(a.right_arm, Vector3(0, -.8, 0), Vector3(.06, .8, .1), Color(.7, .75, .8));
	a.sword = Object::cast_to<MeshInstance3D>(blade->get_child(0));
	a.nav = memnew(NavigationAgent3D);
	a.body->add_child(a.nav);
	// Recast's two voxel layers place this flat nav surface 0.5m above
	// the physical floor. Follow it at foot height instead of stalling at
	// the vertically offset first path point.
	a.nav->set_path_height_offset(compound_world ? .3 : .5);
	a.nav->set_path_desired_distance(.5);
	a.nav->set_target_desired_distance(compound_world ? .15f : .7f);
	a.nav->set_radius(.32);
	a.nav->set_height(1.7);
	a.nav->set_avoidance_enabled(false);
	a.target = position;
	if (id == 1) {
		a.yaw = camera_yaw;
	}
	if (arctic && id != 1) {
		a.name_tag = memnew(Label3D);
		a.body->add_child(a.name_tag);
		a.name_tag->set_position(Vector3(0, 2.1, 0));
		a.name_tag->set_font_size(36);
		a.name_tag->set_pixel_size(.004);
		a.name_tag->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		a.name_tag->set_visibility_range_end(25);
		a.name_tag->set_text(id <= 5 ? String(id == 2 ? "Maya" : id == 3 ? "Rafi"
													   : id == 4		 ? "Lian"
																		 : "Omar")
									 : String("Stranger ") + itos(id));
	}
	actors.push_back(a);
	if (!(autoplay && DisplayServer::get_singleton()->get_name() == "headless" && script_case != "assets")) {
		try_asset(actors.back());
	}
}
bool LastLightGame::clear_line(const Vector3 &from, const Vector3 &to) const {
	auto space = get_world_3d()->get_direct_space_state();
	PhysicsDirectSpaceState3D::RayParameters q;
	q.from = from;
	q.to = to;
	q.collision_mask = 1;
	PhysicsDirectSpaceState3D::RayResult result;
	return !space->intersect_ray(q, result);
}
bool LastLightGame::can_see(const Vector3 &from, const Vector3 &to) const {
	if (arctic) {
		for (const auto &effect : effects) {
			if (effect.kind == "smoke" && effect.until > elapsed) {
				Vector3 direction = to - from;
				float t = CLAMP((effect.position - from).dot(direction) / MAX(.001f, direction.length_squared()), 0.f, 1.f);
				if ((from + direction * t - effect.position).length() < 4) {
					return false;
				}
			}
		}
	}
	return clear_line(from, to);
}
bool LastLightGame::visible(const Actor &a, const Actor &b, float range) const {
	return a.stats.id != b.stats.id && b.stats.active() && flat_distance(a.body->get_position(), b.body->get_position()) <= range && can_see(a.body->get_position() + Vector3(0, 1.2, 0), b.body->get_position() + Vector3(0, 1.2, 0));
}
void LastLightGame::emit_event(int source, int target, const String &kind, const Vector3 &where) {
	Dictionary event;
	event["sequence"] = ++event_sequence;
	event["time"] = elapsed;
	event["actor"] = source;
	event["target"] = target;
	event["kind"] = kind;
	journal.push_back(event);
	if (arctic) {
		auto victim = find_actor(target);
		auto helper = find_actor(source);
		if (kind == "attack" && victim) {
			if (victim->teammate_id == source) {
				victim->teammate_id = 0;
			}
			record_experience(*victim, "attacked", compound_world && !victim->visible_actors.count(source) ? 0 : source, -.8f);
			if (helper && helper->stats.survivor) {
				bool defensive = victim->stats.survivor ? helper->memory[target].threat > .25 : true;
				record_experience(*helper, defensive ? "defended" : "harmful", target, defensive ? .1f : -1.f);
			}
		}
		if (kind == "pickup" && helper) {
			record_experience(*helper, "scavenged", 0, .1f);
		}
		if ((kind == "help" || kind == "offer") && victim && helper) {
			record_experience(*victim, "help_received", source, .7f);
			record_experience(*helper, "help_given", target, .4f);
		}
	}
	if (journal.size() > 200) {
		journal.pop_front();
	}
	for (auto &observer : actors) {
		if (!observer.stats.active() || observer.stats.id == source) {
			continue;
		}
		float distance = flat_distance(observer.body->get_position(), where);
		const bool witnessed = distance < 18 && can_see(observer.body->get_position() + Vector3(0, 1.2, 0), where + Vector3(0, 1.2, 0));
		if ((kind == "attack" || kind == "shot" || kind == "noise") && distance < (arctic ? 45 : 25)) {
			observer.heard_position = where;
			if (compound_world) {
				observer.noise_danger = kind == "attack" || kind == "shot";
			}
			observer.heard_until = elapsed + 3;
		}
		if (!witnessed) {
			continue;
		}
		if (kind == "sabotage") {
			observer.memory[source].threat = MAX(.6f, observer.memory[source].threat);
			observer.memory[source].last_attack = elapsed;
		}
		if (kind == "attack") {
			// Defensive fire at a known aggressor is not evidence of betrayal.
			auto known = observer.memory.find(target);
			const bool defending = known != observer.memory.end() && known->second.threat > .25f && target != observer.stats.id;
			if (!defending) {
				witness(observer.memory[source], "attack", elapsed);
				record_experience(observer, "aggression_witnessed", source, -.1f);
			} else {
				observer.memory[source].trust = MIN(1.f, observer.memory[source].trust + .04f);
			}
		} else if ((kind == "help" || kind == "offer") && find_actor(source)) {
			witness(observer.memory[source], kind.utf8().get_data(), elapsed);
			record_experience(observer, "help_witnessed", source, .15f);
		}
	}
}
void LastLightGame::notify(const String &text) {
	message = text;
	message_time = 5;
	print_line(text);
}
void LastLightGame::input(const Ref<InputEvent> &event) {
	Ref<InputEventMouseMotion> motion = event;
	if (motion.is_valid() && !overview && Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED && !paused) {
		camera_yaw -= motion->get_relative().x * mouse_sensitivity;
		camera_pitch = CLAMP(camera_pitch - motion->get_relative().y * mouse_sensitivity * (invert_mouse ? -1.f : 1.f), -1.1f, .5f);
	}
	Ref<InputEventMouseButton> wheel = event;
	if (wheel.is_valid() && wheel->is_pressed() && overview && frontend_mode == "playing") {
		if (wheel->get_button_index() == MouseButton::WHEEL_UP) {
			overview_zoom = MAX(35.f, overview_zoom - 6);
		}
		if (wheel->get_button_index() == MouseButton::WHEEL_DOWN) {
			overview_zoom = MIN(140.f, overview_zoom + 6);
		}
	}
	Ref<InputEventKey> key = event;
	if (key.is_null() || !key->is_pressed() || key->is_echo()) {
		return;
	}
	::Key code = key->get_physical_keycode();
	auto player = find_actor(1);
	if (code == Key::J && compound_world) {
		game_command("inventory");
		return;
	}
	if (frontend_mode == "inventory") {
		if (code == Key::ESCAPE) {
			game_command("inventory");
		}
		return;
	}
	if (code == Key::F3 && compound_world) {
		game_command("squad");
		return;
	}
	if (frontend_mode == "squad") {
		if (code == Key::ESCAPE) {
			game_command("squad");
		}
		return;
	}
	if (code == Key::I && arctic && session_started && !paused && !victory && !defeat) {
		game_command("inspect");
		return;
	}
	if (code == Key::M && arctic && session_started && !paused && !victory && !defeat) {
		game_command("map");
		return;
	}
	if (frontend_mode == "map") {
		if (code == Key::ESCAPE) {
			game_command("map");
		}
		return;
	}
	if (frontend_mode == "inspect") {
		if (code == Key::ESCAPE) {
			game_command("inspect");
		}
		return;
	}
	if (code == Key::ESCAPE) {
		if (frontend_attached && (!session_started || victory || defeat)) {
			return;
		}
		paused = !paused;
		frontend_mode = paused ? "pause" : "playing";
		Input::get_singleton()->set_mouse_mode(paused || overview ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_CAPTURED);
		return;
	}
	if (code == Key::TAB) {
		for (int n = 1; n <= 5; ++n) {
			int candidate = (camera_focus + n - 1) % 5 + 1;
			if (auto other = find_actor(candidate); other && other->stats.active()) {
				camera_focus = candidate;
				break;
			}
		}
		return;
	}
	if (code == Key::F2 && compound_world && !paused && session_started) {
		overview = !overview;
		if (overview) {
			previous_view_yaw = camera_yaw;
			previous_view_pitch = camera_pitch;
			camera_yaw = .65;
			camera_pitch = -.95;
		} else {
			camera_yaw = previous_view_yaw;
			camera_pitch = previous_view_pitch;
		}
		Input::get_singleton()->set_mouse_mode(overview ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_CAPTURED);
		return;
	}
	if (code == Key::F1) {
		debug_overlay = !debug_overlay;
		return;
	}
	if (frontend_attached && !session_started && code != Key::F9) {
		return;
	}
	if (code == Key::F5) {
		save_game();
		return;
	}
	if (code == Key::F9) {
		if (frontend_attached) {
			game_command("load");
			return;
		}
		load_game();
		return;
	}
	if (code == Key::F10) {
		autoplay = !autoplay;
		notify(autoplay ? "Autoplay enabled" : "Human control enabled");
		return;
	}
	if (code == Key::BACKSPACE) {
		restart();
		return;
	}
	if (paused || victory || defeat || !player || !player->stats.active()) {
		return;
	}
	if (code == Key::Z && compound_world) {
		if (player->cover_manual) {
			player->cover_manual = false;
			return;
		}
		float best = 3.2f;
		for (auto &p : cover_points) {
			float d = player->body->get_position().distance_to(p.position);
			if (d < best) {
				best = d;
				player->cover_id = p.id;
				player->cover_goal = p.position;
			}
		}
		if (best < 3.2f) {
			player->cover_manual = true;
			notify("Cover: A/D slide, RMB expose, Z or W/S leave. Physical walls still block shots.");
		} else {
			notify("Move within three metres of a cover face, then press Z.");
		}
		return;
	}
	if (code == Key::R) {
		begin(player->stats, State::Reload);
	}
	if (code == Key::H) {
		begin(player->stats, State::Heal);
	}
	if (code == Key::X) {
		cancel(player->stats);
	}
	if (code == Key::KEY_1) {
		player->use_sword = false;
	}
	if (code == Key::KEY_2) {
		player->use_sword = true;
	}
	if (arctic) {
		if (code == Key::KEY_3 || code == Key::KEY_4 || code == Key::KEY_5) {
			if (!player->stats.busy() && switch_weapon(player->stats.inventory, int(code) - int(Key::KEY_3) + 1)) {
				player->use_sword = false;
			}
		}
		if (code == Key::KEY_1 && !player->stats.busy()) {
			switch_weapon(player->stats.inventory, 0);
		}
		if (code == Key::B) {
			use_equipment(*player, "bandage");
		}
		if (code == Key::T) {
			use_equipment(*player, "water");
		}
		if (code == Key::Q) {
			use_equipment(*player, "smoke");
		}
		if (code == Key::V) {
			use_equipment(*player, "decoy");
		}
		if (code == Key::ALT) {
			use_equipment(*player, "roll");
		}
		if (code == Key::C) {
			for (auto &door : world_objects) {
				if (door.kind == "door" && flat_distance(player->body->get_position(), door.position) < 3) {
					set_door(door, !door.open);
					break;
				}
			}
		}
	}

	if (code == Key::SPACE && player->body->is_on_floor() && !player->stats.busy()) {
		player->jump = true;
	}
	if (arctic && code == Key::F) {
		use_equipment(*player, "food");
		return;
	}
	if (code == Key::F && !player->stats.busy() && player->stats.inventory.food > 0 && player->stats.hunger < 100) {
		--player->stats.inventory.food;
		player->stats.hunger = MIN(100.f, player->stats.hunger + 45);
		notify("Food consumed");
	}
	if (arctic && code == Key::Y) {
		Actor *peer = nullptr;
		float nearest = 6;
		for (auto &other : actors) {
			if (other.stats.survivor && other.stats.id != 1 && other.stats.active() && flat_distance(player->body->get_position(), other.body->get_position()) < nearest) {
				nearest = flat_distance(player->body->get_position(), other.body->get_position());
				peer = &other;
			}
		}
		if (peer) {
			if (peer->personality.loyalty + MAX(peer->memory[1].trust, peer->persistent_trust[1]) > .65f && peer->memory[1].threat < .25f) {
				peer->teammate_id = 1;
				player->teammate_id = peer->stats.id;
				peer->next_plan = 0;
				notify("Survivor agreed to travel with you. Y can invite another nearby survivor.");
			} else {
				notify("Survivor declined: trust is low or you are considered a threat.");
			}
		}
		return;
	}
	if (code == Key::G) {
		for (auto &other : actors) {
			if (other.stats.survivor && other.stats.id != 1 && other.stats.active() && flat_distance(player->body->get_position(), other.body->get_position()) < 2 && clear_line(player->body->get_position() + Vector3(0, 1.2, 0), other.body->get_position() + Vector3(0, 1.2, 0)) && player->stats.inventory.rounds > 0 && player->stats.inventory.weapon_kind == other.stats.inventory.weapon_kind && other.stats.inventory.pistol && other.stats.inventory.rounds < weapon_spec(other.stats.inventory.weapon_kind).magazine && !player->stats.busy()) {
				--player->stats.inventory.rounds;
				++other.stats.inventory.rounds;
				emit_event(1, other.stats.id, "offer", player->body->get_position());
				break;
			}
		}
	}
}
void LastLightGame::move_actor(Actor &a, float dt) {
	Vector3 before = a.body->get_global_position();
	auto input = Input::get_singleton();
	bool human = a.stats.id == 1 && !autoplay && !smoke;
	if (a.stats.active() && (!a.stats.busy() || (a.stats.state == State::Interact && (a.environment_action == "smoke" || a.environment_action == "decoy")))) {
		if (human && frontend_mode == "playing") {
			Vector2 move(float(input->is_physical_key_pressed(Key::D)) - float(input->is_physical_key_pressed(Key::A)), float(input->is_physical_key_pressed(Key::S)) - float(input->is_physical_key_pressed(Key::W)));
			if (move.length() > 1) {
				move.normalize();
			}
			Basis basis(Vector3(0, 1, 0), camera_yaw);
			a.move = basis.xform(Vector3(move.x, 0, move.y));
			a.sprint = input->is_physical_key_pressed(Key::SHIFT);
			a.crouching = input->is_physical_key_pressed(Key::CTRL);
			a.aiming = input->is_mouse_button_pressed(MouseButton::RIGHT);
			a.interacting = input->is_physical_key_pressed(Key::E);
			if (a.aiming) {
				a.yaw = camera_yaw;
				if (overview) {
					Vector2 mouse = get_viewport()->get_mouse_position();
					Vector3 origin = camera->project_ray_origin(mouse), direction = camera->project_ray_normal(mouse);
					if (Math::abs(direction.y) > .01f) {
						Vector3 point = origin + direction * ((before.y + 1 - origin.y) / direction.y);
						a.yaw = Math::atan2(-(point.x - before.x), -(point.z - before.z));
					}
				}
			}
			if (compound_world && a.cover_manual) {
				if (Math::abs(move.y) > .1f || a.sprint || a.jump || a.cover_id < 0 || size_t(a.cover_id) >= cover_points.size()) {
					a.cover_manual = false;
				} else {
					auto &cover = cover_points[a.cover_id];
					Vector3 gap = a.cover_goal - before;
					gap.y = 0;
					if (gap.length() > .35f) {
						a.move = gap.normalized();
					} else {
						a.move = Vector3(-cover.normal.z, 0, cover.normal.x) * move.x;
					}
					a.crouching = !a.aiming;
					a.sprint = false;
					if (!a.aiming) {
						a.yaw = Math::atan2(cover.normal.x, cover.normal.z);
					}
					if (flat_distance(before, a.cover_goal) > 3.3f) {
						a.cover_manual = false;
					}
				}
			}
			if (input->is_mouse_button_pressed(MouseButton::LEFT)) {
				if (a.use_sword || !a.stats.inventory.pistol) {
					melee(a);
				} else {
					shoot(a);
				}
			}
		} else if (!human && (!a.aiming || (compound_world && (a.tactic == lastlight::Tactic::Cover || a.tactic == lastlight::Tactic::Peek || a.tactic == lastlight::Tactic::Flank || a.tactic == lastlight::Tactic::Withdraw || a.tactic == lastlight::Tactic::Suppress))) && a.stats.state != State::Revive && flat_distance(before, a.target) > (compound_world ? .13f : .65f)) {
			Vector3 next = a.nav->get_next_path_position();
			a.move = next - before;
			a.move.y = 0;
			if (a.move.length() > .01) {
				a.move.normalize();
			}
		} else {
			a.move = Vector3();
		}
	} else {
		a.move = Vector3();
	}
	if (smoke && !script_case.is_empty()) {
		if (script_case == "physics" && a.stats.id == 1) {
			a.move = Vector3(-1, 0, 0);
			a.aiming = false;
		}
		if (script_case == "combat" && (a.stats.id == 1 || a.stats.id == 10)) {
			a.move = Vector3();
			a.interacting = false;
		}
		if ((script_case == "gate" || script_case == "gate_save")) {
			a.move = Vector3();
			a.interacting = (a.stats.id == 2 || (a.stats.id == 3 && elapsed > .5));
		}
	}
	if (!a.stats.active()) {
		a.move = Vector3();
	}
	auto collider = Object::cast_to<CollisionShape3D>(a.body->get_child(0));
	Ref<CapsuleShape3D> capsule = collider->get_shape();
	if (!a.crouching && capsule->get_height() < 1.6) {
		Ref<CapsuleShape3D> standing;
		standing.instantiate();
		standing->set_radius(.3);
		standing->set_height(1.65);
		PhysicsDirectSpaceState3D::ShapeParameters check;
		check.shape_rid = standing->get_rid();
		check.transform = a.body->get_global_transform();
		check.transform.origin += Vector3(0, .85, 0);
		check.collision_mask = 1;
		check.exclude.insert(a.body->get_rid());
		PhysicsDirectSpaceState3D::ShapeResult result[1];
		if (get_world_3d()->get_direct_space_state()->intersect_shape(check, result, 1) > 0) {
			a.crouching = true;
		}
	}
	float desired_capsule = (compound_world && (a.stats.state == State::Downed || a.stats.state == State::Dead)) ? .65f : (a.crouching ? 1.1f : 1.65f);
	if (Math::abs(capsule->get_height() - desired_capsule) > .001) {
		capsule->set_height(desired_capsule);
		collider->set_position(Vector3(0, desired_capsule * .5f + .025f, 0));
	}
	bool running = a.stats.inventory.weight() < a.stats.inventory.capacity() && !a.aiming && a.sprint && a.stats.stamina > 1 && !a.crouching && a.move.length() > .1;
	if (running) {
		a.stats.stamina = MAX(0.f, a.stats.stamina - dt * 16);
	}
	float speed = a.crouching ? 1.5f : (running ? 5.f : 3.f);
	if (a.aiming) {
		speed = MIN(speed, 2.f);
	}
	if (!human && a.move.length() > .1 && flat_distance(before, a.target) > 1.5) {
		Vector3 separation;
		for (auto &other : actors) {
			if (other.stats.id != a.stats.id && other.stats.active()) {
				Vector3 away = before - other.body->get_position();
				away.y = 0;
				float length = away.length();
				const float radius = arctic ? 1.4f : 1.f;
				if (length > .01 && length < radius) {
					separation += away / length * (radius - length) * (arctic ? .9f : .6f);
					if (arctic && a.move.dot(away / length) < -.2f) {
						separation += Vector3(-a.move.z, 0, a.move.x) * (1.4f - length) * 1.8f;
					}
				}
			}
		}
		a.move = (a.move + separation).normalized();
	}
	if (arctic && a.body->get_position().y < -.5) {
		speed = 1.2;
	}
	// Local capsule steering handles temporarily occupied paths and movable
	// doorway/crate geometry without teleporting or discarding the route.
	if (arctic && !human && !a.aiming && !a.stats.busy() && a.move.length() > .1f && a.path_retry > .25f) {
		Vector3 direction = a.move.normalized();
		for (float angle : { (a.stats.id % 2 ? 1.f : -1.f) * 1.05f, (a.stats.id % 2 ? -1.f : 1.f) * 1.05f, 1.57f, -1.57f }) {
			Vector3 alternative = direction.rotated(Vector3(0, 1, 0), angle);
			Vector3 destination = before + alternative * .9f;
			Vector3 walkable = NavigationServer3D::get_singleton()->map_get_closest_point(get_world_3d()->get_navigation_map(), destination);
			if (flat_distance(destination, walkable) < .5f && !a.body->test_move(a.body->get_global_transform(), alternative * .8f)) {
				a.move = alternative;
				break;
			}
		}
	}
	if (compound_world && !human && a.move.length() > .1f) {
		auto nav = NavigationServer3D::get_singleton();
		RID map = get_world_3d()->get_navigation_map();
		if (before.y < -.5f) {
			Vector3 shore = nav->map_get_closest_point(map, before);
			if (flat_distance(shore, before) < 3 && shore.y > -.1f && a.body->is_on_floor()) {
				a.jump = true;
			}
		} else {
			Vector3 next = before + a.move * .75f;
			Vector3 safe = nav->map_get_closest_point(map, next);
			if (flat_distance(safe, next) > .22f && Math::abs(safe.y - before.y) < .65f) {
				Vector3 correction = safe - before;
				correction.y = 0;
				a.move = correction.length() > .05f ? correction.normalized() : Vector3();
			}
		}
	}
	if (compound_world && !human && !running) {
		speed = MIN(speed, MAX(.4f, flat_distance(before, a.target) * 2.f));
	}
	Vector3 desired = a.move * speed;
	if (a.stats.state == State::Interact && a.environment_action == "roll") {
		desired = (a.traversal_end - a.traversal_start).normalized() * 4.5f;
	}
	Vector3 velocity = a.body->get_velocity();
	velocity.x = Math::move_toward(velocity.x, desired.x, dt * 18);
	velocity.z = Math::move_toward(velocity.z, desired.z, dt * 18);
	if (!a.body->is_on_floor()) {
		velocity.y -= 18 * dt;
	} else if (velocity.y < 0) {
		velocity.y = -.5;
	}
	if (a.jump && a.body->is_on_floor() && !a.crouching) {
		velocity.y = 6;
	}
	a.jump = false;
	// Step up only when both the upward and forward sweeps are clear.
	Vector3 horizontal(velocity.x * dt, 0, velocity.z * dt);
	if (a.body->is_on_floor() && horizontal.length() > .001 && a.body->test_move(a.body->get_global_transform(), horizontal)) {
		Transform3D elevated = a.body->get_global_transform();
		elevated.origin.y += .3;
		if (!a.body->test_move(a.body->get_global_transform(), Vector3(0, .3, 0)) && !a.body->test_move(elevated, horizontal)) {
			a.body->set_global_position(elevated.origin);
		}
	}
	a.body->set_velocity(velocity);
	a.body->move_and_slide();
	if (a.stats.active() && !a.stats.busy() && a.stats.state != State::Fire) {
		if (!a.body->is_on_floor()) {
			a.stats.state = State::Airborne;
		} else if (a.aiming) {
			a.stats.state = State::Aim;
		} else if (a.move.length() < .1) {
			a.stats.state = State::Idle;
		} else {
			a.stats.state = a.crouching ? State::Crouch : (running ? State::Run : State::Walk);
		}
	}
	if (!a.aiming && a.move.length() > .1) {
		a.yaw = Math::lerp_angle(a.yaw, Math::atan2(-a.move.x, -a.move.z), MIN(1.f, dt * 12));
	}
	if (a.interacting && a.stats.active() && !a.stats.busy()) {
		interact(a);
	}
	if (a.body->get_position().y < -10) {
		damage(a.stats, 1000);
		a.body->set_position(arctic ? Vector3(140, 0, 238) : Vector3(4, 0, 4));
	}
	Vector3 displacement = a.body->get_global_position() - before;
	a.travelled += Vector2(displacement.x, displacement.z).length();
	if (a.move.length() > .1 && Vector2(displacement.x, displacement.z).length() < .01) {
		a.path_retry += dt;
	} else {
		a.path_retry = 0;
	}
	if (a.path_retry > 1.5) {
		a.nav->set_target_position(a.target);
		a.path_retry = 0;
	}
	animate(a, dt, displacement);
}
void LastLightGame::animate(Actor &a, float dt, const Vector3 &delta) {
	Vector3 actual = delta / dt;
	actual.y = 0;
	Basis facing(Vector3(0, 1, 0), a.yaw);
	Vector3 local = facing.inverse().xform(actual);
	a.blend.update(actual.length(), local.x, local.z, a.crouching, a.aiming, a.stats.busy() || a.stats.state == State::Fire, dt);
	if (autoplay && DisplayServer::get_singleton()->get_name() == "headless" && script_case != "assets") {
		a.body->set_collision_layer(a.stats.active() ? 2 : 0);
		a.body->set_collision_mask(a.stats.active() ? 3 : 1);
		return;
	}
	a.visual->set_rotation(Vector3(0, a.yaw + a.visual_yaw_offset, 0));
	if (a.animation_tree) {
		update_animation_graph(a, dt, local);
	} else if (a.animation) {
		Dictionary clips = assets.get("clips", Dictionary());
		String clip = clips.get(state_name(a.stats.state), String());
		if (!clip.is_empty() && a.animation->has_animation(clip) && a.last_clip != clip) {
			a.animation->play(clip, .16);
			a.last_clip = clip;
		}
	} else {
		a.visual->set_scale(Vector3(1, a.crouching ? .7 : 1, 1));
		float swing = Math::sin(a.blend.phase * Math::TAU) * .65f * (a.blend.walk + a.blend.run + a.blend.crouch);
		a.left_leg->set_rotation(Vector3(swing, 0, 0));
		a.right_leg->set_rotation(Vector3(-swing, 0, 0));
		a.left_arm->set_rotation(Vector3(a.aiming ? -1.35f : -swing, 0, 0));
		a.right_arm->set_rotation(Vector3(a.aiming ? -1.35f : swing, 0, 0));
		if (a.stats.state == State::Melee) {
			a.right_arm->set_rotation(Vector3(-1.8f + a.stats.timer * 4, 0, -.4));
		}
	}
	if (!a.stats.active()) {
		a.visual->set_rotation(Vector3(0, a.yaw + a.visual_yaw_offset, a.animation_tree ? 0 : Math::PI / 2));
		a.body->set_collision_layer(0);
		a.body->set_collision_mask(1);
	} else {
		a.body->set_collision_layer(2);
		a.body->set_collision_mask(3);
	}
	a.pistol->set_visible(a.stats.inventory.pistol && !a.use_sword);
	a.sword->set_visible(a.stats.inventory.sword && a.use_sword);
	if (a.gun_models.size() == 4) {
		for (int kind = 0; kind < 4; ++kind) {
			if (a.gun_models[kind]) {
				a.gun_models[kind]->set_visible(a.stats.inventory.pistol && !a.use_sword && kind == a.stats.inventory.weapon_kind && a.stats.state != State::Interact && a.stats.state != State::Heal && a.stats.state != State::Revive);
			}
		}
	} else if (a.pistol_model) {
		a.pistol_model->set_visible(a.stats.inventory.pistol && !a.use_sword);
	}
	if (a.held_item) {
		a.held_item->set_visible(a.stats.state == State::Heal || a.stats.state == State::Interact && (a.environment_action == "water" || a.environment_action == "food" || a.environment_action == "bandage"));
	}
	if (a.sword_model) {
		a.sword_model->set_visible(a.stats.inventory.sword && a.use_sword);
	}
}
void LastLightGame::shoot(Actor &a) {
	if (compound_world) {
		shoot_compound(a);
		return;
	}
	if (!a.stats.active() || a.stats.busy() || !a.stats.inventory.pistol || a.stats.inventory.rounds < 1 || a.stats.cooldown > 0) {
		return;
	}
	Vector3 from = a.body->get_global_position() + Vector3(0, 1.35, 0), direction;
	auto space = get_world_3d()->get_direct_space_state();
	if (a.stats.id == 1 && !autoplay && !smoke) {
		PhysicsDirectSpaceState3D::RayParameters aim;
		aim.from = camera->get_global_position();
		aim.to = aim.from - camera->get_global_basis().get_column(2) * 40;
		aim.collision_mask = 3;
		aim.exclude.insert(a.body->get_rid());
		PhysicsDirectSpaceState3D::RayResult hit;
		direction = ((space->intersect_ray(aim, hit) ? hit.position : aim.to) - from).normalized();
	} else {
		float height = 1.1f;
		if (arctic) {
			for (const auto &target : actors) {
				if (target.stats.active() && target.stats.id != a.stats.id && flat_distance(target.body->get_position(), a.target) < .7f) {
					height = target.crouching ? .65f : 1.f;
					break;
				}
			}
		}
		direction = (a.target + Vector3(0, height, 0) - from).normalized();
	}
	PhysicsDirectSpaceState3D::RayParameters query;
	query.from = from;
	query.to = from + direction * lastlight::weapon_spec(a.stats.inventory.weapon_kind).range;
	query.collision_mask = 3;
	query.exclude.insert(a.body->get_rid());
	PhysicsDirectSpaceState3D::RayResult result;
	const bool hit = space->intersect_ray(query, result);
	Actor *victim = nullptr;
	if (hit) {
		for (auto &other : actors) {
			if (result.collider == other.body) {
				victim = &other;
				break;
			}
		}
	}
	if (a.stats.survivor && (a.stats.id != 1 || autoplay || automated_run) && victim && victim->stats.survivor) {
		auto belief = a.memory.find(victim->stats.id);
		if (belief == a.memory.end() || belief->second.threat < .25) {
			a.reason = "Holding fire: neutral in the line of sight";
			if (arctic) {
				Vector3 side = direction.cross(Vector3(0, 1, 0)).normalized();
				for (int sign : { a.stats.id % 2 ? 1 : -1, a.stats.id % 2 ? -1 : 1 }) {
					Vector3 desired = a.body->get_position() + side * (3.f * sign);
					Vector3 walkable = NavigationServer3D::get_singleton()->map_get_closest_point(get_world_3d()->get_navigation_map(), desired);
					if (flat_distance(walkable, desired) < .7f) {
						a.tactical_position = walkable;
						a.reposition_until = elapsed + 2;
						break;
					}
				}
			}
			return;
		}
	}
	if (!fire(a.stats)) {
		return;
	}
	if (victim && victim->stats.active()) {
		if (a.stats.survivor && victim->stats.survivor) {
			auto belief = a.memory.find(victim->stats.id);
			if (belief == a.memory.end() || belief->second.threat < .25) {
				++harmful_attacks;
			}
		}
		damage(victim->stats, lastlight::weapon_spec(a.stats.inventory.weapon_kind).damage);
		++shots_hit;
		a.hit_feedback_time = elapsed;
		emit_event(a.stats.id, victim->stats.id, "attack", a.body->get_position());
	} else {
		if (hit) {
			++wall_blocks;
		}
		emit_event(a.stats.id, 0, "shot", a.body->get_position());
	}
	if (a.stats.id == 1 && !automated_run) {
		camera_pitch = CLAMP(camera_pitch + .035f, -1.1f, .5f);
	}
}
void LastLightGame::melee(Actor &a) {
	if (!begin(a.stats, State::Melee)) {
		return;
	}
	a.melee_pending = true;
	++a.melee_combo;
}
void LastLightGame::apply_melee(Actor &a) {
	a.melee_pending = false;
	Vector3 front = -Basis(Vector3(0, 1, 0), a.yaw).get_column(2);
	Ref<SphereShape3D> shape;
	shape.instantiate();
	shape->set_radius(a.stats.inventory.sword && a.use_sword ? 1.1f : .55f);
	PhysicsDirectSpaceState3D::ShapeParameters query;
	query.shape_rid = shape->get_rid();
	query.transform.origin = a.body->get_global_position() + Vector3(0, 1, 0) + front * .85;
	query.collision_mask = 2;
	query.exclude.insert(a.body->get_rid());
	PhysicsDirectSpaceState3D::ShapeResult hits[16];
	int count = get_world_3d()->get_direct_space_state()->intersect_shape(query, hits, 16);
	std::set<int> damaged;
	for (int n = 0; n < count; ++n) {
		for (auto &victim : actors) {
			if (hits[n].collider_id == victim.body->get_instance_id() && victim.stats.active() && damaged.insert(victim.stats.id).second && clear_line(a.body->get_position() + Vector3(0, 1, 0), victim.body->get_position() + Vector3(0, 1, 0))) {
				if (a.stats.survivor && victim.stats.survivor && a.memory[victim.stats.id].threat < .25) {
					++harmful_attacks;
				}
				damage(victim.stats, a.stats.inventory.sword && a.use_sword ? 35 : 12);
				a.hit_feedback_time = elapsed;
				emit_event(a.stats.id, victim.stats.id, "attack", a.body->get_position());
			}
		}
	}
}
void LastLightGame::interact(Actor &a) {
	Vector3 position = a.body->get_global_position();
	for (auto &other : actors) {
		if (other.stats.state == State::Downed && other.stats.survivor && flat_distance(position, other.body->get_position()) < 1.8 && clear_line(position + Vector3(0, 1, 0), other.body->get_position() + Vector3(0, 1, 0)) && a.stats.inventory.medkits > 0) {
			begin(a.stats, State::Revive, other.stats.id);
			return;
		}
	}
	if (arctic) {
		if (a.stats.id != 1 || autoplay || smoke) {
			for (auto &object : world_objects) {
				if (object.id == a.environment_target && flat_distance(position, object.position) < 3 && interact_environment(a)) {
					return;
				}
			}
		}
		for (auto &item : items) {
			if (item.available && flat_distance(position, item.position) < 1.8 && clear_line(position + Vector3(0, 1, 0), item.position) && acquire_item(a, item)) {
				return;
			}
		}
		interact_environment(a);
		return;
	}
	for (auto &item : items) {
		if (item.available && flat_distance(position, item.position) < 1.8 && clear_line(position + Vector3(0, 1, 0), item.position)) {
			auto &i = a.stats.inventory;
			if (item.kind == "key") {
				if (!a.stats.survivor) {
					continue;
				}
				key_found = true;
				mission = MAX(1, mission);
				notify("Yard key recovered. Activate the radio across the barrier.");
			} else if (item.kind == "pistol") {
				if (i.pistol) {
					if (i.reserve >= 120) {
						continue;
					}
					i.reserve = MIN(120, i.reserve + 6);
				} else {
					i.pistol = true;
					i.rounds = 6;
				}
			} else if (item.kind == "sword") {
				if (i.sword) {
					continue;
				}
				i.sword = true;
			} else if (item.kind == "ammo") {
				if (i.reserve >= 120) {
					continue;
				}
				i.reserve = MIN(120, i.reserve + 12);
			} else if (item.kind == "medkit") {
				if (i.medkits >= 3) {
					continue;
				}
				++i.medkits;
			} else if (item.kind == "food") {
				if (i.food >= 3) {
					continue;
				}
				++i.food;
			}
			item.available = false;
			item.visual->hide();
			++pickups;
			++a.stats.pickups;
			emit_event(a.stats.id, item.id, "pickup", position);
			return;
		}
	}
	if (mission == 1 && a.stats.survivor && flat_distance(position, radio_position) < 2 && clear_line(position + Vector3(0, 1, 0), radio_position)) {
		radio_done = true;
		mission = 2;
		notify("Radio active. Get the living team to the green extraction pad.");
		emit_event(a.stats.id, 0, "radio", position);
	}
}
void LastLightGame::update_objectives(float dt) {
	int first = 0, second = 0;
	for (auto &a : actors) {
		if (a.stats.active() && a.interacting && !a.stats.busy()) {
			if (flat_distance(a.body->get_position(), control_a) < .9) {
				first = a.stats.id;
			}
			if (flat_distance(a.body->get_position(), control_b) < .9) {
				second = a.stats.id;
			}
		}
	}
	if (!gate_open) {
		lastlight::GateState state{ float(gate_hold), gate_lift, gate_opening, gate_open };
		const bool was_opening = state.opening;
		const bool opened = lastlight::advance_gate(state, first, second, dt);
		if (!was_opening && (state.opening || state.open)) {
			gate_operator_a = first;
			gate_operator_b = second;
		}
		gate_hold = state.held;
		gate_lift = state.lift;
		gate_opening = state.opening;
		gate_open = state.open;
		gate->set_position(gate_closed_position + Vector3(0, gate_lift, 0));
		if (opened) {
			++cooperative_uses;
			gate->set_collision_layer(1);
			gate->set_collision_mask(2);
			gate_mesh->hide();
			region->bake_navigation_mesh(false);
			if (compound_world) {
				compound_paths_ready = false;
			}
			emit_event(gate_operator_a, gate_operator_b, "help", control_a);
			emit_event(gate_operator_b, gate_operator_a, "help", control_b);
			notify("Gate open: cooperative shortcut available.");
		}
	}
	int living = 0, extracted = 0;
	for (auto &a : actors) {
		if (a.stats.survivor) {
			bool landing_ready = !compound_world || ((flat_distance(a.body->get_position(), exit_position) < 4 && beacon_active[0] && elapsed >= beacon_times[0] && beacon_hold[0] > .75f && !beacon_contested[0]) || (flat_distance(a.body->get_position(), alternative_exit) < 4 && beacon_active[1] && elapsed >= beacon_times[1] && beacon_hold[1] > .75f && !beacon_contested[1]));
			if (a.stats.active() && mission == 2 && landing_ready && (flat_distance(a.body->get_position(), exit_position) < (arctic ? 3 : 1.5) || (arctic && flat_distance(a.body->get_position(), alternative_exit) < 3))) {
				a.arrived_health = a.stats.health;
				a.arrived_time = elapsed;
				a.stats.state = State::Extracted;
				a.body->set_velocity(Vector3());
				a.visual->hide();
				emit_event(a.stats.id, 0, "extracted", a.body->get_position());
				record_experience(a, "extracted", 0, 1);
				a.outcome_recorded = true;
				save_profiles();
			}
			if (a.stats.active()) {
				++living;
			}
			if (a.stats.state == State::Extracted) {
				++extracted;
			}
		}
	}
	if (living == 0) {
		if (extracted > 0) {
			victory = true;
			for (auto &a : actors) {
				if (a.stats.state == State::Downed) {
					a.stats.state = State::Dead;
					a.stats.bleed = 0;
				}
			}
		} else {
			defeat = true;
		}
	}
	if (elapsed > double(config.get("deadline_seconds", 600))) {
		defeat = true;
	}
	if (victory || defeat) {
		if (arctic) {
			for (auto &a : actors) {
				if (a.stats.survivor && !a.outcome_recorded) {
					emit_event(a.stats.id, 0, "death", a.body->get_position());
					record_experience(a, "died", 0, -1);
					a.outcome_recorded = true;
				}
			}
			save_profiles();
		}
		notify(victory ? "VICTORY - survivors extracted. Backspace restarts." : "DEFEAT - team lost or deadline expired. Backspace restarts.");
		frontend_mode = "results";
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		stop_python();
		if (automated_run) {
			finish_report();
		}
	}
}
void LastLightGame::physics(float dt) {
	if (!is_inside_tree() || !region || actors.empty()) {
		return;
	}
	if (physics_frames < 3) {
		++physics_frames;
		return;
	}
	if (paused || victory || defeat) {
		return;
	}
	if (python_server.is_valid() && (!python_ready || (autoplay && pending_request))) {
		OS::get_singleton()->delay_usec(1000);
		return;
	}
	++physics_frames;
	dt = MIN(dt, .1f);
	elapsed += dt;
	message_time = MAX(0., message_time - dt);
	for (auto &a : actors) {
		State completed = tick(a.stats, dt);
		if (completed == State::Interact && arctic) {
			complete_environment_action(a);
		}
		if (a.stats.state == State::Melee && a.melee_pending && a.stats.timer <= .3f) {
			apply_melee(a);
		}
		if (a.stats.state != State::Melee) {
			a.melee_pending = false;
		}
		if (completed == State::Revive) {
			auto victim = find_actor(a.stats.pending_target);
			if (victim && flat_distance(a.body->get_position(), victim->body->get_position()) < 1.8 && clear_line(a.body->get_position() + Vector3(0, 1, 0), victim->body->get_position() + Vector3(0, 1, 0)) && revive(a.stats, victim->stats)) {
				++revivals;
				emit_event(a.stats.id, victim->stats.id, "help", a.body->get_position());
			}
		}
		if (a.stats.state == State::Fire && a.stats.cooldown < .2) {
			a.stats.state = State::Idle;
		}
	}
	if (smoke) {
		integration_step();
		if (script_case == "python_exit" && physics_frames == 90 && python_pid) {
			OS::get_singleton()->kill(python_pid);
			python_pid = 0; // kill() already reaps the child; detect socket closure next.
		}
		if (physics_frames == 4 && script_case == "solo") {
			for (auto &other : actors) {
				if (other.stats.survivor && other.stats.id != 1) {
					other.stats.health = 0;
					other.stats.state = State::Dead;
				}
			}
		}
		if (physics_frames == 4 && script_case == "combat") {
			auto p = find_actor(1);
			auto t = find_actor(10);
			if (p && t) {
				p->body->set_position(Vector3(16, .1, 14));
				p->stats.inventory.pistol = true;
				p->stats.inventory.rounds = 6;
				t->body->set_position(Vector3(24, .1, 14));
				t->stats.inventory.rounds = 0;
			}
		}
		if (physics_frames == 4 && (script_case == "gate" || script_case == "gate_save")) {
			find_actor(2)->body->set_position(control_a + Vector3(0, .1, 0));
			find_actor(3)->body->set_position(control_b + Vector3(0, .1, 0));
		}
		if (script_case == "combat") {
			auto p = find_actor(1);
			if (find_actor(10)) {
				p->target = find_actor(10)->body->get_position();
			}
			if (physics_frames == 10 || physics_frames == 80) {
				shoot(*p);
			}
			if (physics_frames == 50) {
				gate_open = true;
				gate_lift = 3;
				gate_opening = false;
				gate->set_position(Vector3(20, 4.5, 14));
				gate->set_collision_layer(0);
				gate->set_collision_mask(0);
				gate_mesh->hide();
			}
			if (physics_frames == 100) {
				begin(p->stats, State::Reload);
			}
		}
		if (script_case == "gate" && physics_frames == 25 && gate_open) {
			integration_ok = false;
		}
		if (script_case == "save" && physics_frames == 120) {
			auto before = snapshot();
			save_game();
			find_actor(1)->stats.health = 50;
			load_game();
			Dictionary after = snapshot();
			integration_ok = int(after["mission"]) == int(before["mission"]);
			Array old_actors = before["actors"], new_actors = after["actors"];
			for (int n = 0; n < old_actors.size(); ++n) {
				Dictionary old = old_actors[n], current = new_actors[n];
				for (String key : { String("health"), String("state"), String("rounds"), String("reserve"), String("medkits"), String("food") }) {
					integration_ok = integration_ok && int(old[key]) == int(current[key]);
				}
			}

			Dictionary bad = before.duplicate(true);
			Array bad_actors = bad["actors"];
			Dictionary first = bad_actors[0];
			first["health"] = -10;
			integration_ok = integration_ok && !restore(bad);
		}
	}
	if (compound_world && physics_frames > 7 && (!compound_paths_ready || compound_nav_iteration != NavigationServer3D::get_singleton()->map_get_iteration_id(get_world_3d()->get_navigation_map()))) {
		refresh_compound_routes();
	}
	if (compound_world) {
		step_compound_ai();
	}
	ai_clock += dt;
	if (ai_clock >= .2) {
		ai_clock = 0;
		for (auto &a : actors) {
			if (compound_world) {
				continue;
			}
			if (controller == "nomemory" && a.stats.survivor) {
				a.memory.clear();
				a.heard_until = 0;
			}
			sense(a);
			if ((a.stats.id != 1 || autoplay || smoke) && !(smoke && ((script_case == "combat" && (a.stats.id == 1 || a.stats.id == 10)) || script_case == "gate" || script_case == "gate_save" || script_case == "assets" || script_case == "social" || script_case == "gate_negative" || script_case == "melee" || script_case == "lifecycle" || script_case == "arctic_animation" || script_case == "arctic_interactions" || script_case == "arctic_equipment" || script_case == "arctic_social" || script_case == "arctic_art" || script_case == "arctic_crossfire" || script_case == "profiles"))) {
				decide(a);
			}
		}
		if (trace_file.is_valid()) {
			for (const auto &actor : actors) {
				if (actor.stats.survivor && actor.stats.active()) {
					Dictionary entry = build_observation(actor);
					entry["tick"] = physics_frames;
					entry["time"] = elapsed;
					entry["intent"] = actor.intent;
					entry["reason"] = actor.reason;
					if (arctic) {
						entry["strategy"] = profile_info(actor);
					}
					trace_file->store_string(JSON::stringify(entry) + "\n");
				}
			}
		}
		request_policy();
	}
	for (auto &a : actors) {
		move_actor(a, dt);
	}
	if (arctic) {
		update_environment(dt);
	}
	if (compound_world) {
		update_compound(dt);
	}
	update_objectives(dt);
	hud_clock += dt;
	if (hud_clock >= .1) {
		hud_clock = 0;
		update_hud();
	}
	if (automated_run && physics_frames >= max_frames) {
		finish_report();
	}
}
void LastLightGame::update_hud() {
	auto p = find_actor(1);
	if (!p || !hud) {
		return;
	}
	String objective = mission == 0 ? "1/3 Recover the yellow yard key" : mission == 1 ? "2/3 Activate the green radio across the barrier"
																					   : "3/3 Extract the surviving team at the green pad";
	String text = "LAST LIGHT | " + objective + "\nHP " + itos(p->stats.health) + "  Ammo " + itos(p->stats.inventory.rounds) + "/" + itos(p->stats.inventory.reserve) + "  Stamina " + itos(int(p->stats.stamina)) + "  Hunger " + itos(int(p->stats.hunger)) + "\n";
	text += "WASD move | Shift sprint | Ctrl crouch | Space jump | Mouse aim | LMB attack | RMB aim\nE hold/interact/revive | R reload | H heal | F food | G offer | 1 pistol / 2 sword | X cancel\nEsc pause | F5 save | F9 load | F10 autoplay | Backspace restart | F1 debug\n";
	if (paused) {
		text += "PAUSED\n";
	}
	for (auto &a : actors) {
		if (a.stats.survivor) {
			text += itos(a.stats.id) + ": " + state_name(a.stats.state) + " HP " + itos(a.stats.health) + " " + a.reason + "\n";
			if (debug_overlay) {
				text += "  blend idle/walk/run/crouch " + rtos(a.blend.idle) + " / " + rtos(a.blend.walk) + " / " + rtos(a.blend.run) + " / " + rtos(a.blend.crouch) + "\n";
			}
		}
	}
	if (debug_overlay) {
		text += "FPS " + itos(Engine::get_singleton()->get_frames_per_second()) + " | nav polygons " + itos(region->get_navigation_mesh()->get_polygon_count()) + " | Python " + (python_ready ? "connected" : "authored fallback");
	}
	Vector3 objective_position = mission_target_for(*p);
	text += "Objective distance: " + itos(int(flat_distance(p->body->get_position(), objective_position))) + "m | Tab: spectator camera\n";
	for (auto &item : items) {
		if (item.available && flat_distance(p->body->get_position(), item.position) < 1.8) {
			text += "E: pick up " + item.kind + "\n";
			break;
		}
	}
	if (!gate_open && (flat_distance(p->body->get_position(), control_a) < 1 || flat_distance(p->body->get_position(), control_b) < 1)) {
		text += "Hold E; a second actor must hold the other control. " + itos(int(gate_hold * 100)) + "%\n";
	}
	interaction_hint = "";
	for (auto &item : items) {
		if (item.available && flat_distance(p->body->get_position(), item.position) < 1.8 && clear_line(p->body->get_position() + Vector3(0, 1, 0), item.position)) {
			interaction_hint = "E  Pick up " + item.kind;
			break;
		}
	}
	for (auto &other : actors) {
		if (other.stats.survivor && other.stats.state == State::Downed && flat_distance(p->body->get_position(), other.body->get_position()) < 1.8 && clear_line(p->body->get_position() + Vector3(0, 1, 0), other.body->get_position() + Vector3(0, 1, 0))) {
			interaction_hint = p->stats.inventory.medkits > 0 ? "E  Revive survivor" : "A medkit is needed to revive";
		}
	}
	if (!gate_open && (flat_distance(p->body->get_position(), control_a) < .9 || flat_distance(p->body->get_position(), control_b) < .9)) {
		interaction_hint = "Hold E  Gate control  " + itos(int(CLAMP(gate_hold, 0., 1.) * 100)) + "% - partner needed";
	}
	if (mission == 1 && flat_distance(p->body->get_position(), radio_position) < 2) {
		interaction_hint = "E  Activate the signal radio";
	}
	if (arctic && !p->stats.busy()) {
		for (auto &object : world_objects) {
			if (flat_distance(p->body->get_position(), object.position) < 3) {
				if (object.kind == "door") {
					interaction_hint = object.open ? "C Close door" : "E Open door";
				}
				if (object.kind == "crate" && !object.used) {
					interaction_hint = "E Search " + object.name + " (2s)";
				}
				if (object.kind == "generator" && !power_on) {
					interaction_hint = p->stats.inventory.fuel ? "E Refuel generator (3s)" : "E Hand-crank backup power (12s)";
				}
				if (object.kind == "relay" && !object.used) {
					interaction_hint = power_on ? "E Calibrate relay (2s)" : "Generator power required";
				}
				if (object.kind == "security" && !object.used) {
					interaction_hint = p->stats.inventory.tools ? "E Secure turbine panel against sabotage (4s)" : "Tool kit required to secure turbine controls";
				}
				if (compound_world && object.kind == "extraction") {
					int n = int(object.value);
					interaction_hint = !radio_done ? "Restore radio before calling an airlift" : !beacon_active[n] ? "E Activate landing beacon"
							: beacon_contested[n]																   ? "Clear raiders from the landing perimeter"
							: elapsed < beacon_times[n]															   ? "Hold the perimeter: " + itos(int(Math::ceil(beacon_times[n] - elapsed))) + "s"
																												   : "Airlift ready: move onto the landing pad";
				}
				if (object.kind == "radio" && !radio_done) {
					interaction_hint = "E Use radio (key, power and two relays required)";
				}
			}
		}
	}
	hud->set_text(text);
	message_label->set_text(message_time > 0 ? message : String());
}
void LastLightGame::restart() {
	save_profiles();
	stop_python();
	get_tree()->call_deferred("reload_current_scene");
	set_physics_process(false);
}
