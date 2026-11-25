#include "last_light_game.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/shader.h"
#include "servers/display_server.h"
#include "servers/navigation_server_3d.h"
#include <cmath>
namespace {
void tag(Node3D *parent, const String &text, const Vector3 &position, const Color &color = Color(.65, .85, 1)) {
	auto label = memnew(Label3D);
	parent->add_child(label);
	label->set_position(position);
	label->set_text(text);
	label->set_font_size(40);
	label->set_pixel_size(.008);
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	label->set_modulate(color);
	label->set_visibility_range_end(42);
}
void warm_light(Node3D *parent, const Vector3 &position, float radius = 12) {
	auto light = memnew(OmniLight3D);
	parent->add_child(light);
	light->set_position(position);
	light->set_color(Color(1, .65, .28));
	light->set_param(Light3D::PARAM_RANGE, radius);
	light->set_param(Light3D::PARAM_ENERGY, 2);
	light->set_shadow(false);
}
} //namespace
int LastLightGame::nearest_region(const Vector3 &position) const {
	int result = 0;
	float nearest = 1e9;
	for (size_t i = 0; i < strategic_nodes.size(); ++i) {
		auto p = strategic_nodes[i].position;
		float d = Vector2(position.x - p.x, position.z - p.z).length_squared();
		if (d < nearest) {
			nearest = d;
			result = int(i);
		}
	}
	return result;
}
int LastLightGame::add_world_object(const String &kind, const String &name, const Vector3 &position) {
	WorldObject object;
	object.id = 2001 + int(world_objects.size());
	object.kind = kind;
	object.name = name;
	object.position = position;
	object.visual = memnew(Node3D);
	level_root->add_child(object.visual);
	object.visual->set_position(position);
	world_objects.push_back(object);
	return object.id;
}
void LastLightGame::add_item(const String &kind, const Vector3 &position, int container) {
	Item item{ 101 + int(items.size()), kind, position };
	item.container = container;
	item.available = container == 0;
	if (compound_world) {
		item.visual = make_compound_pickup(kind, position);
	} else {
		item.visual = make_box(level_root, position, Vector3(.32, .25, .32), kind == "key" ? Color(1, .7, .15) : kind == "medkit" || kind == "bandage" ? Color(.85, .25, .22)
																																					   : Color(.3, .65, .8));
	}
	item.visual->set_visible(item.available);
	items.push_back(item);
}
void LastLightGame::create_arctic_level() {
	if (compound_world) {
		create_compound_level();
		return;
	}
	create_classic_level();
}
void LastLightGame::create_classic_level() {
	config["deadline_seconds"] = 1500;
	spawn_count = CLAMP(spawn_count, 0, 8);
	level_root = memnew(Node3D);
	level_root->set_name("ArcticBase");
	add_child(level_root);
	region = memnew(NavigationRegion3D);
	level_root->add_child(region);
	const lastlight::StrategyNode nodes[] = { { { 140, 238 }, "Control post", true }, { { 88, 202 }, "West trail", false }, { { 60, 146 }, "Forest camp", true }, { { 78, 96 }, "Living quarters", true }, { { 63, 55 }, "Storage", true }, { { 115, 112 }, "Medical station", true }, { { 150, 146 }, "Generator yard", false }, { { 183, 150 }, "Ice bridge", false }, { { 214, 104 }, "East patrol road", false }, { { 182, 66 }, "Radio station", true }, { { 208, 35 }, "North extraction", false }, { { 48, 183 }, "Emergency landing", false }, { { 130, 40 }, "Watchtower", false }, { { 200, 224 }, "Supply shed", true }, { { 100, 170 }, "Workshop", true }, { { 30, 100 }, "West ridge", false } };
	strategic_nodes.assign(std::begin(nodes), std::end(nodes));
	const lastlight::StrategyEdge edges[] = { { 0, 1, .1, .1 }, { 0, 13, .3, .4 }, { 1, 2, .08, .05 }, { 1, 14, .2, .2 }, { 14, 6, .4, .4 }, { 1, 6, .12, .1, true }, { 2, 3, .08, .05 }, { 3, 4, .15, .2 }, { 3, 5, .1, .15 }, { 5, 6, .3, .4 }, { 6, 7, .6, .5 }, { 7, 8, .5, .5 }, { 8, 9, .35, .4 }, { 9, 10, .2, .2 }, { 4, 12, .5, .5 }, { 12, 9, .4, .4 }, { 2, 15, .03, .02 }, { 15, 4, .08, .05 }, { 2, 11, .12, .15 }, { 11, 1, .05, .05 }, { 13, 8, .4, .3 }, { 14, 3, .2, .15 } };
	strategic_edges.assign(std::begin(edges), std::end(edges));
	auto ellipse = [](float x, float z, float cx, float cz, float rx, float rz) { return ((x - cx) * (x - cx) / (rx * rx) + (z - cz) * (z - cz) / (rz * rz)) < 1; };
	Array rows;
	for (int z = 0; z < 151; ++z) {
		String row;
		for (int x = 0; x < 151; ++x) {
			float wx = x * 2, wz = z * 2;
			bool land = ellipse(wx, wz, 135, 95, 110, 90) || ellipse(wx, wz, 142, 232, 70, 60) || ellipse(wx, wz, 50, 175, 30, 22) || (wx > 74 && wx < 106 && wz > 140 && wz < 230) || (wx > 198 && wx < 222 && wz > 110 && wz < 228);
			if (wx > 174 && wx < 190 && wz > 112 && wz < 200 && !(wz >= 146 && wz <= 154)) {
				land = false;
			}
			row += land ? "." : "#";
		}
		map_rows.push_back(row);
		rows.push_back(row);
	}
	config["map"] = rows;
	config["world"] = "arctic";
	// Only walkable island strips enter navigation; the cold-water floor is
	// physical but deliberately excluded from the AI navigation region.
	make_box(level_root, Vector3(150, -1.3, 150), Vector3(302, .4, 302), Color(.045, .22, .3), true);
	auto surface = make_box(level_root, Vector3(150, -.08, 150), Vector3(302, .025, 302), Color(.07, .28, .38));
	if (auto mesh = Object::cast_to<MeshInstance3D>(surface->get_child(0))) {
		Ref<StandardMaterial3D> water;
		water.instantiate();
		water->set_albedo(Color(.055, .26, .36, .76));
		water->set_roughness(.2);
		water->set_metallic(.25);
		water->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		mesh->set_material_override(water);
	}
	std::vector<Transform3D> ground_instances;
	for (int z = 0; z < 151; ++z) {
		for (int x = 0; x < 151;) {
			if (map_rows[z][x] != '.') {
				++x;
				continue;
			}
			int begin = x;
			while (x < 151 && map_rows[z][x] == '.') {
				++x;
			}
			auto floor = make_box(region, Vector3(begin + x - 1, -.15, z * 2), Vector3((x - begin) * 2, .3, 2), Color(.68, .79, .84), true);
			floor->get_child(0)->set("visible", false);
			ground_instances.push_back(Transform3D(Basis().scaled(Vector3((x - begin) * 2, .3, 2)), Vector3(begin + x - 1, -.15, z * 2)));
		}
	}
	Ref<BoxMesh> snow_box;
	snow_box.instantiate();
	snow_box->set_size(Vector3(1, 1, 1));
	snow_box->set_material(level_material(true));
	Ref<MultiMesh> snow_batch;
	snow_batch.instantiate();
	snow_batch->set_transform_format(MultiMesh::TRANSFORM_3D);
	snow_batch->set_mesh(snow_box);
	snow_batch->set_instance_count(ground_instances.size());
	for (size_t i = 0; i < ground_instances.size(); ++i) {
		snow_batch->set_instance_transform(i, ground_instances[i]);
	}
	auto snow_node = memnew(MultiMeshInstance3D);
	level_root->add_child(snow_node);
	snow_node->set_multimesh(snow_batch);
	// An open bridge links the shore; a cooperative gate offers a second route.
	make_box(region, Vector3(183, .04, 150), Vector3(18, .15, 7), Color(.27, .36, .4), true);
	make_box(region, Vector3(110, 1.3, 155.5), Vector3(.5, 2.6, 27), Color(.22, .32, .38), true);
	make_box(region, Vector3(110, 1.3, 180.5), Vector3(.5, 2.6, 19), Color(.22, .32, .38), true);
	gate_closed_position = Vector3(110, 1.5, 170);
	gate = Object::cast_to<StaticBody3D>(make_box(region, gate_closed_position, Vector3(.4, 3, 2), Color(.28, .5, .58), true));
	gate_mesh = Object::cast_to<MeshInstance3D>(gate->get_child(0));
	control_a = Vector3(107, .05, 168);
	control_b = Vector3(107, .05, 172);
	make_box(level_root, control_a, Vector3(1.4, .1, 1.4), Color(.2, .7, .95));
	make_box(level_root, control_b, Vector3(1.4, .1, 1.4), Color(.2, .7, .95));
	key_position = Vector3(77, .45, 95);
	generator_position = Vector3(150, 0, 146);
	radio_position = Vector3(182, .7, 66);
	exit_position = Vector3(208, 0, 35);
	alternative_exit = Vector3(48, 0, 183);
	add_item("key", key_position);
	add_world_object("generator", "Emergency generator", generator_position);
	add_world_object("radio", "Radio console", radio_position);
	add_world_object("relay", "Storage relay", Vector3(64, 0, 58));
	add_world_object("relay", "Supply relay", Vector3(198, 0, 222));
	add_world_object("relay", "Ridge relay", Vector3(31, 0, 100));
	add_world_object("shelter", "Forest campfire", Vector3(60, 0, 146));
	add_world_object("shelter", "Control post heater", Vector3(139, 0, 250));
	const char *contents[][3] = { { "medkit", "bandage", "water" }, { "rifle", "rifle_ammo", "armour" }, { "food", "water", "coat" }, { "fuel", "tools", "smoke" }, { "smg", "smg_ammo", "decoy" }, { "shotgun", "shells", "backpack" } };
	for (int i = 0; i < 22; ++i) {
		int zone = (i * 5 + 3) % 16;
		auto p = strategic_nodes[zone].position;
		Vector3 pos(p.x + float(int(rng() % 5) - 2) * 1.5f, 0, p.z + float(int(rng() % 5) - 2) * 1.5f);
		if (zone == 0) {
			pos = Vector3(146 + (i % 2) * 3, 0, 234);
		}
		int type = (i + int(rng() % 6)) % 6;
		int crate = add_world_object("crate", String(contents[type][0]) + " supply crate", pos);
		for (int k = 0; k < 3; ++k) {
			add_item(contents[type][k], pos + Vector3((k - 1) * .55f, .35, .8), crate);
		}
	}
	const char *loose[] = { "pistol", "ammo", "bandage", "food", "water", "fuel", "smoke", "decoy", "armour", "coat", "rifle", "rifle_ammo", "smg", "smg_ammo", "shotgun", "shells", "tools", "medkit", "backpack", "sword" };
	for (int i = 0; i < 54; ++i) {
		int zone = (i * 7 + 1) % 16;
		auto p = strategic_nodes[zone].position;
		add_item(loose[i % 20], Vector3(p.x + int(rng() % 9) - 4, .35, p.z + int(rng() % 9) - 4));
	}
	add_item("pistol", Vector3(134, .35, 246));
	add_item("water", Vector3(135, .35, 246));
	add_item("medkit", Vector3(136, .35, 246));
	create_arctic_art();
	Ref<NavigationMesh> nav;
	nav.instantiate();
	nav->set_parsed_geometry_type(NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS);
	nav->set_collision_mask(1);
	nav->set_cell_size(.6);
	nav->set_cell_height(.2);
	nav->set_agent_radius(.6);
	nav->set_agent_height(1.8);
	nav->set_agent_max_climb(.4);
	NavigationServer3D::get_singleton()->map_set_cell_height(get_world_3d()->get_navigation_map(), .2);
	NavigationServer3D::get_singleton()->map_set_cell_size(get_world_3d()->get_navigation_map(), .6);
	region->set_navigation_mesh(nav);
	region->bake_navigation_mesh(false);
	for (int id = 1; id <= 5; ++id) {
		create_actor(id, Vector3(136 + (id - 1) * 2, .1, 248), true, Color(.25 + (id % 3) * .15, .55, .7));
	}
	for (int i = 0; i < 8; ++i) {
		auto p = strategic_nodes[2 + (i * 3) % 14].position;
		create_actor(10 + i, Vector3(p.x + 3, .1, p.z + 3), false, Color(.7, .35, .2));
		actors.back().bot_role = i % 5;
		actors.back().stats.inventory.weapon_kind = i % 4;
	}
	configure_bots(spawn_count);
	camera_yaw = .3;
	camera_pitch = -.3;
}
void LastLightGame::create_arctic_art() {
	String root = "res://assets/arctic/";
	auto prop = [&](const String &pack, const String &model, Vector3 p, Vector3 size, bool collision = false) { return place_asset(root + pack + "/" + model + ".glb", p, size, collision); };
	auto building = [&](const Vector3 &center, float width, float depth, const String &name) {
		auto floor = make_box(level_root, center + Vector3(0, .015, 0), Vector3(width, .03, depth), Color(.22, .3, .36));
		Ref<Shader> floor_shader;
		floor_shader.instantiate();
		floor_shader->set_code("shader_type spatial;varying vec3 p;void vertex(){p=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;}void fragment(){vec2 t=fract(p.xz*.5);float seam=step(.025,t.x)*step(.025,t.y);float stripe=step(.92,t.x)*step(.96,fract(p.z*.08));ALBEDO=mix(vec3(.075,.11,.14),vec3(.23,.30,.32),seam);ALBEDO=mix(ALBEDO,vec3(.75,.48,.14),stripe*.65);ROUGHNESS=.87;}");
		Ref<ShaderMaterial> floor_material;
		floor_material.instantiate();
		floor_material->set_shader(floor_shader);
		Object::cast_to<MeshInstance3D>(floor->get_child(0))->set_material_override(floor_material);
		// Visible metal framing, skirting and fittings have no hidden collision.
		for (int side : { -1, 1 }) {
			for (float z = -depth * .5f; z <= depth * .5f; z += 3.f) {
				make_box(level_root, center + Vector3(side * (width * .5f - .2f), 1.7, z), Vector3(.14, 3.4, .14), Color(.14, .25, .3));
			}
			make_box(level_root, center + Vector3(side * (width * .5f - .18f), .3, 0), Vector3(.08, .12, depth), Color(.72, .54, .22));
			make_box(level_root, center + Vector3(side * (width * .5f - .18f), 2.7, 0), Vector3(.08, .1, depth), Color(.17, .28, .34));
		}
		make_box(level_root, center + Vector3(0, .25, depth * .5f - .2f), Vector3(width, .25, .08), Color(.13, .22, .28));
		make_box(region, center + Vector3(-width * .5, 1.65, 0), Vector3(.35, 3.3, depth), Color(.38, .49, .57), true);
		make_box(region, center + Vector3(width * .5, 1.65, 0), Vector3(.35, 3.3, depth), Color(.38, .49, .57), true);
		make_box(region, center + Vector3(0, 1.65, depth * .5), Vector3(width, 3.3, .35), Color(.28, .4, .48), true);
		float panel = (width - 3.6f) * .5f;
		for (int side : { -1, 1 }) {
			make_box(region, center + Vector3(side * (1.8f + panel * .5f), 1.65, -depth * .5f), Vector3(panel, 3.3, .35), Color(.38, .49, .57), true);
		}
		// Roof strips keep interiors visible from the third-person camera.
		make_box(level_root, center + Vector3(0, 3.5, depth * .38), Vector3(width + .6, .25, depth * .28), Color(.8, .88, .92));
		Vector3 entry = center + Vector3(0, 0, -depth * .5);
		int id = add_world_object("door", name + " door", entry);
		auto &door = world_objects.back();
		door.body = Object::cast_to<StaticBody3D>(make_box(level_root, entry + Vector3(0, 1.35, 0), Vector3(3.4, 2.7, .25), Color(.2, .36, .43), true));
		door.visual->add_child(memnew(Node3D));
		for (int side : { -1, 1 }) {
			make_box(level_root, entry + Vector3(side * width * .32, 2.1, -.2), Vector3(1.8, .8, .08), Color(1, .65, .28));
		}
		for (int side : { -1, 1 }) {
			auto panel = make_box(level_root, center + Vector3(side * (width * .5f - .25f), 2.3, depth * .25f), Vector3(.08, .28, 2.4), Color(1, .77, .42));
			Ref<StandardMaterial3D> glow;
			glow.instantiate();
			glow->set_albedo(Color(1, .77, .42));
			glow->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
			glow->set_emission(Color(1, .5, .16));
			glow->set_emission_energy_multiplier(1.6);
			Object::cast_to<MeshInstance3D>(panel->get_child(0))->set_material_override(glow);
		}
		warm_light(level_root, center + Vector3(0, 2.6, 0), 14);
		warm_light(level_root, entry + Vector3(0, 2, -2), 9);
		tag(level_root, name, entry + Vector3(0, 3.8, 0));
		prop("furniture-kit", "desk", center + Vector3(-width * .3, 0, 1), Vector3(2, .9, 1), true);
		prop("furniture-kit", "computerScreen", center + Vector3(-width * .3, 1, 1), Vector3(.65, .5, .3));
		prop("furniture-kit", "bedBunk", center + Vector3(width * .3, 0, depth * .2), Vector3(2, 1.8, 1), true);
		prop("furniture-kit", "bookcaseClosed", center + Vector3(width * .3, 0, depth * .4), Vector3(2, 2, .45), true);
		prop("furniture-kit", "chair", center + Vector3(-width * .3, 0, -.2), Vector3(.65, 1, .65));
		prop("furniture-kit", "lampWall", center + Vector3(-width * .5f + .35f, 2.4, depth * .22f), Vector3(.25, .45, .3));
		prop("furniture-kit", "radio", center + Vector3(-width * .3f + .5f, .95, 1), Vector3(.35, .28, .2));
		if (name == "LIVING QUARTERS" || name == "CONTROL POST") {
			prop("furniture-kit", "loungeSofa", center + Vector3(width * .25f, 0, -depth * .25f), Vector3(2.4, 1, 1));
			prop("furniture-kit", "table", center + Vector3(width * .25f, 0, -depth * .05f), Vector3(1.8, .75, .9));
			prop("furniture-kit", "kitchenFridge", center + Vector3(-width * .35f, 0, depth * .35f), Vector3(.85, 1.9, .8));
		}
		if (name == "MEDICAL") {
			for (int bed = 0; bed < 3; ++bed) {
				prop("furniture-kit", "bedSingle", center + Vector3(-2.5f + bed * 2.5f, 0, 3), Vector3(1.5, .7, 2.2));
			}
		}
		if (name == "WORKSHOP" || name == "STORAGE") {
			prop("survival-kit", "workbench", center + Vector3(0, 0, depth * .3f), Vector3(3, 1.2, 1));
			prop("survival-kit", "tool-hammer", center + Vector3(0, 1.25, depth * .3f), Vector3(.4, .15, .25));
		}
		return id;
	};
	building(Vector3(140, 0, 250), 20, 16, "CONTROL POST");
	building(Vector3(78, 0, 96), 24, 18, "LIVING QUARTERS");
	building(Vector3(63, 0, 55), 28, 20, "STORAGE");
	building(Vector3(115, 0, 112), 18, 14, "MEDICAL");
	building(Vector3(182, 0, 66), 18, 14, "RADIO");
	building(Vector3(100, 0, 170), 20, 14, "WORKSHOP");
	building(Vector3(200, 0, 224), 16, 14, "SUPPLY SHED");
	prop("city-kit-industrial", "shipping-container-b", Vector3(153, 0, 228), Vector3(8, 2.8, 3.2), true);
	prop("nature-kit", "tent_detailedOpen", Vector3(120, 0, 236), Vector3(5, 2.8, 4));
	prop("nature-kit", "log_stack", Vector3(127, 0, 232), Vector3(3, 1.2, 2), true);
	for (int i = 0; i < 5; ++i) {
		prop("survival-kit", "barrel", Vector3(129 + (i % 3) * 1.2, 0, 235 + (i / 3) * 1.2), Vector3(.8, 1.2, .8), true);
	}
	prop("city-kit-industrial", "water-tower", Vector3(156, 0, 140), Vector3(7, 14, 7), true);
	prop("city-kit-industrial", "detail-tank", generator_position, Vector3(3, 2, 2));
	for (auto &object : world_objects) {
		if (object.kind == "generator") {
			object.body = Object::cast_to<StaticBody3D>(make_box(region, generator_position + Vector3(0, 1, 0), Vector3(2.5, 2, 1.7), Color(.3, .4, .45), true));
		}
	}
	prop("city-kit-industrial", "windmill-low", Vector3(168, 0, 80), Vector3(6, 16, 6), true);
	prop("furniture-kit", "radio", radio_position, Vector3(.8, .6, .5));
	for (int i = 0; i < 12; ++i) {
		Vector3 p(130 + (i % 4) * 7, 0, 100 + (i / 4) * 9);
		prop("city-kit-industrial", "shipping-container-a", p, Vector3(5, 2.6, 2.6), true);
	}
	for (const Vector3 &p : { exit_position, alternative_exit }) {
		make_box(level_root, p + Vector3(0, .05, 0), Vector3(5, .1, 5), Color(.2, .8, .55));
		tag(level_root, "EXTRACTION", p + Vector3(0, 2.6, 0), Color(.35, 1, .7));
	}
	for (auto &object : world_objects) {
		if (object.kind == "crate") {
			prop("survival-kit", "chest", object.position, Vector3(1, .7, .65));
			object.body = Object::cast_to<StaticBody3D>(make_box(level_root, object.position + Vector3(0, .3, 0), Vector3(.9, .6, .6), Color(.3, .4, .45), true));
			object.body->get_child(0)->set("visible", false);
		}
		if (object.kind == "relay") {
			make_box(level_root, object.position + Vector3(0, .8, 0), Vector3(.7, 1.6, .5), Color(.25, .6, .72));
			make_box(level_root, object.position + Vector3(0, 2.4, 0), Vector3(.08, 2, .08), Color(.65, .75, .8));
		}
		if (object.kind == "shelter") {
			prop("survival-kit", "campfire-pit", object.position, Vector3(1, .25, 1));
			warm_light(level_root, object.position + Vector3(0, .7, 0), 8);
		}
		if (object.kind != "door") {
			tag(level_root, object.name, object.position + Vector3(0, 1.8, 0));
		}
	}
	for (auto &item : items) {
		String path;
		Vector3 size(.5, .5, .5);
		if (item.kind == "pistol") {
			path = assets.get("pistol_scene", String());
		}
		if (item.kind == "sword") {
			path = assets.get("sword_scene", String());
		}
		if (item.kind == "smg" || item.kind == "rifle" || item.kind == "shotgun") {
			path = root + item.kind + ".glb";
		}
		if (item.kind == "ammo" || item.kind.ends_with("_ammo") || item.kind == "shells") {
			path = root + "blaster-kit/clip-small.glb";
		}
		if (item.kind == "fuel") {
			path = root + "survival-kit/bottle-large.glb";
		}
		if (item.kind == "food" || item.kind == "backpack") {
			path = root + "survival-kit/box.glb";
		}
		if (item.kind == "tools") {
			path = root + "survival-kit/tool-hammer.glb";
		}
		if (item.kind == "medkit") {
			make_box(item.visual, Vector3(0, .14, -.17), Vector3(.22, .065, .025), Color(1, .96, .9));
			make_box(item.visual, Vector3(0, .14, -.18), Vector3(.065, .22, .025), Color(1, .96, .9));
		}
		if (item.kind == "water") {
			path = root + "survival-kit/bottle.glb";
		}
		if (item.kind == "smoke" || item.kind == "decoy") {
			path = root + "blaster-kit/grenade-a.glb";
		}
		if (!path.is_empty()) {
			auto packed = load_asset_scene(path);
			if (packed.is_valid()) {
				for (int k = 0; k < item.visual->get_child_count(); ++k) {
					if (auto old = Object::cast_to<Node3D>(item.visual->get_child(k))) {
						old->hide();
					}
				}
				item.visual->add_child(packed->instantiate());
			}
		}
		tag(item.visual, item.kind.to_upper(), Vector3(0, .6, 0), Color(.7, .9, 1));
		auto label = Object::cast_to<Label3D>(item.visual->get_child(item.visual->get_child_count() - 1));
		label->set_pixel_size(.0045);
		label->set_visibility_range_end(18);
	}
	// Batched vegetation shares geometry/materials. Individual trunks remain
	// in the navigation/collision world; no transparent forest barriers.
	Ref<Shader> frost_shader;
	frost_shader.instantiate();
	frost_shader->set_code(R"SHADER(shader_type spatial;
uniform vec4 base_color : source_color = vec4(1.0);
uniform sampler2D palette : source_color, filter_nearest_mipmap;
uniform bool use_palette=false;
varying vec3 world_normal;
void vertex(){VERTEX.x+=sin(TIME*0.7+MODEL_MATRIX[3].x)*max(VERTEX.y,0.0)*0.008;world_normal=(MODEL_MATRIX*vec4(NORMAL,0.0)).xyz;}
void fragment(){vec3 c=base_color.rgb;if(use_palette){c*=texture(palette,UV).rgb;}float snow=smoothstep(0.12,0.75,normalize(world_normal).y);ALBEDO=mix(c*vec3(0.72,0.84,1.0),vec3(0.78,0.86,0.94),snow*0.88);ROUGHNESS=0.95;}
)SHADER");
	for (int variant = 0; variant < 3; ++variant) {
		String name = variant == 0 ? "tree_pineTallA" : variant == 1 ? "tree_pineTallB"
																	 : "tree_pineRoundC";
		Ref<PackedScene> scene = load_asset_scene(root + "nature-kit/" + name + ".glb");
		if (scene.is_null()) {
			continue;
		}
		Node *sample = scene->instantiate();
		struct Part {
			Ref<Mesh> mesh;
			Transform3D transform;
		};
		std::vector<Part> parts;
		auto collect = [&](auto &&self, Node *node, Transform3D parent) -> void {auto spatial=Object::cast_to<Node3D>(node);Transform3D t=spatial?parent*spatial->get_transform():parent;if(auto mesh=Object::cast_to<MeshInstance3D>(node)){parts.push_back({mesh->get_mesh(),t});
}for(int c=0;c<node->get_child_count();++c){self(self,node->get_child(c),t);
} };
		collect(collect, sample, Transform3D());
		std::vector<Transform3D> positions;
		for (int i = 0; i < 240; ++i) {
			float x = 20 + rng() % 230, z = 18 + rng() % 265;
			int gx = int(x / 2), gz = int(z / 2);
			if (gz >= 151 || gx >= 151 || map_rows[gz][gx] != '.') {
				continue;
			}
			bool near_existing_obstacle = false;
			for (const auto &node : strategic_nodes) {
				if (lastlight::distance({ x, z }, node.position) < 13) {
					near_existing_obstacle = true;
				}
			}
			if (near_existing_obstacle) {
				continue;
			}
			float scale = 2.2f + (rng() % 100) * .012f;
			Transform3D t(Basis(Vector3(0, 1, 0), (rng() % 628) * .01f).scaled(Vector3(scale, scale, scale)), Vector3(x, 0, z));
			positions.push_back(t);
			make_box(region, Vector3(x, 1.5, z), Vector3(.55, 3, .55), Color(.25, .3, .35), true)->get_child(0)->set("visible", false);
		}
		for (const auto &part : parts) {
			if (part.mesh.is_valid()) {
				Ref<MultiMesh> batch;
				batch.instantiate();
				batch->set_transform_format(MultiMesh::TRANSFORM_3D);
				Ref<Mesh> winter_mesh = part.mesh->duplicate();
				for (int surface = 0; surface < winter_mesh->get_surface_count(); ++surface) {
					Ref<BaseMaterial3D> original = part.mesh->surface_get_material(surface);
					Ref<ShaderMaterial> material;
					material.instantiate();
					material->set_shader(frost_shader);
					if (original.is_valid()) {
						material->set_shader_parameter("base_color", original->get_albedo());
						auto palette = original->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
						material->set_shader_parameter("use_palette", palette.is_valid());
						if (palette.is_valid()) {
							material->set_shader_parameter("palette", palette);
						}
					}
					winter_mesh->surface_set_material(surface, material);
				}
				batch->set_mesh(winter_mesh);
				batch->set_instance_count(positions.size());
				for (size_t i = 0; i < positions.size(); ++i) {
					batch->set_instance_transform(i, positions[i] * part.transform);
				}
				auto node = memnew(MultiMeshInstance3D);
				level_root->add_child(node);
				node->set_multimesh(batch);
			}
		}
		memdelete(sample);
	}
	for (int i = 0; i < 55; ++i) {
		float x = 20 + rng() % 235, z = 25 + rng() % 250;
		if (map_rows[int(z / 2)][int(x / 2)] != '.') {
			continue;
		}
		bool near_existing_obstacle = false;
		for (auto &node : strategic_nodes) {
			if (lastlight::distance({ x, z }, node.position) < 14) {
				near_existing_obstacle = true;
			}
		}
		if (near_existing_obstacle) {
			continue;
		}
		prop("nature-kit", i % 2 ? "rock_largeA" : "rock_largeB", Vector3(x, 0, z), Vector3(2 + (i % 3), 1.3, 2), true);
	}
}
