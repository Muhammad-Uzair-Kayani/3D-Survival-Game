#include "core/io/image.h"
#include "last_light_game.h"
#include "scene/3d/label_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

namespace {
void bounds(Node *node, const Transform3D &parent, AABB &box, bool &found) {
	auto spatial = Object::cast_to<Node3D>(node);
	Transform3D transform = spatial ? parent * spatial->get_transform() : parent;
	if (auto mesh = Object::cast_to<MeshInstance3D>(node); mesh && mesh->get_mesh().is_valid()) {
		AABB current = transform.xform(mesh->get_aabb());
		if (found) {
			box.merge_with(current);
		} else {
			box = current;
			found = true;
		}
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		bounds(node->get_child(i), transform, box, found);
	}
}
void faces(Node *node, Vector<Vector3> &vertices, const Vector3 &origin) {
	if (auto mesh = Object::cast_to<MeshInstance3D>(node); mesh && mesh->get_mesh().is_valid()) {
		Vector<Face3> data = mesh->get_mesh()->get_faces();
		Transform3D transform = mesh->get_global_transform();
		for (const Face3 &face : data) {
			for (const Vector3 &point : face.vertex) {
				vertices.push_back(transform.xform(point) - origin);
			}
		}
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		faces(node->get_child(i), vertices, origin);
	}
}
Ref<ImageTexture> image_texture(const String &path) {
	Ref<Image> image = Image::load_from_file(path);
	if (image.is_null() || image->is_empty()) {
		return Ref<ImageTexture>();
	}
	image->generate_mipmaps();
	return ImageTexture::create_from_image(image);
}
} //namespace
Ref<Material> LastLightGame::level_material(bool floor) {
	if (arctic && floor) {
		String key = "arctic:snow";
		if (scene_cache.has(key)) {
			return scene_cache[key];
		}
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code("shader_type spatial; varying vec3 world; void vertex(){world=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;} void fragment(){float grain=fract(sin(dot(floor(world.xz*8.0),vec2(12.9898,78.233)))*43758.5453);float drift=sin(world.x*.11+sin(world.z*.16))*.025;ALBEDO=vec3(.68,.79,.86)*(0.94+grain*.06+drift);ROUGHNESS=.97;}");
		Ref<ShaderMaterial> material;
		material.instantiate();
		material->set_shader(shader);
		scene_cache[key] = material;
		return material;
	}
	String key = floor ? "material:floor" : "material:wall";
	if (scene_cache.has(key)) {
		return scene_cache[key];
	}
	String base = "res://assets/environment/";
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_roughness(.9);
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, image_texture(base + (floor ? "SandStoneDEEP_Albedo_A.png" : "StoneTileADEP_ST_Albedo.png")));
	material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, image_texture(base + (floor ? "SandStoneDEEP_Normal.png" : "StoneTileADEP_ST_Normal.png")));
	material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
	material->set_flag(BaseMaterial3D::FLAG_UV1_USE_TRIPLANAR, true);
	material->set_flag(BaseMaterial3D::FLAG_UV1_USE_WORLD_TRIPLANAR, true);
	material->set_uv1_scale(Vector3(.25, .25, .25));
	scene_cache[key] = material;
	return material;
}
Node3D *LastLightGame::place_asset(const String &path, const Vector3 &position, const Vector3 &size, bool collision) {
	Ref<PackedScene> packed = load_asset_scene(path);
	if (packed.is_null()) {
		return nullptr;
	}
	auto model = Object::cast_to<Node3D>(packed->instantiate());
	if (!model) {
		return nullptr;
	}
	AABB box;
	bool found = false;
	bounds(model, Transform3D(), box, found);
	if (!found) {
		memdelete(model);
		return nullptr;
	}
	Vector3 scale;
	for (int i = 0; i < 3; ++i) {
		scale[i] = box.size[i] > .0001 ? size[i] / box.size[i] : 1;
	}
	auto wrapper = memnew(Node3D);
	region->add_child(wrapper);
	wrapper->set_position(position);
	wrapper->set_scale(scale);
	wrapper->add_child(model);
	model->set_position(model->get_position() - Vector3(box.position.x + box.size.x * .5, box.position.y, box.position.z + box.size.z * .5));
	if (collision) {
		Vector<Vector3> geometry;
		faces(model, geometry, position);
		if (!geometry.is_empty()) {
			auto body = memnew(StaticBody3D);
			region->add_child(body);
			body->set_position(position);
			body->set_collision_layer(1);
			body->set_collision_mask(2);
			Ref<ConcavePolygonShape3D> shape;
			shape.instantiate();
			shape->set_faces(geometry);
			auto collider = memnew(CollisionShape3D);
			collider->set_shape(shape);
			body->add_child(collider);
		}
	}
	return wrapper;
}
void LastLightGame::decorate_level() {
	const String folder = "res://assets/environment/";
	gate_model = place_asset(folder + "DungeonGateDEP_ST.glb", Vector3(20, 0, 14), Vector3(.3, 3, 2));
	if (gate_model) {
		gate_model->reparent(gate, true);
		gate_mesh->hide();
	}
	place_asset(folder + "StoneArchDEP_ST.glb", Vector3(20, 0, 14), Vector3(.8, 4, 3.8), true);
	for (const Vector3 &position : { Vector3(10, 0, 4), Vector3(10, 0, 28), Vector3(30, 0, 4), Vector3(36, 0, 28) }) {
		place_asset(folder + "PillarADEP_ST.glb", position, Vector3(1.1, 3.6, 1.1), true);
	}
	// The two towers sit beyond the enclosing walls; they do not conceal
	// unrelated gameplay collision or block a playable route.
	place_asset(folder + "DungeonStructureTowerDEP_ST.glb", Vector3(3, 0, 36), Vector3(6, 11, 3));
	place_asset(folder + "DungeonStructureTowerDEP_ST.glb", Vector3(37, 0, 36), Vector3(6, 11, 3));
	place_asset(folder + "DungeonStructureHeadDEP_ST.glb", Vector3(20, 3.6, 14), Vector3(1.6, 4, 4.8));
	// Real weapon meshes are used for pickups as well as bone attachments.
	for (auto &item : items) {
		if (item.kind != "pistol" && item.kind != "sword") {
			continue;
		}
		Ref<PackedScene> packed = load_asset_scene(assets.get(item.kind + "_scene", String()));
		if (packed.is_null()) {
			continue;
		}
		Node *weapon = packed->instantiate();
		item.visual->add_child(weapon);
		for (int i = 0; i < item.visual->get_child_count() - 1; ++i) {
			if (auto old = Object::cast_to<Node3D>(item.visual->get_child(i))) {
				old->hide();
			}
		}
	}
	for (auto &item : items) {
		auto tag = memnew(Label3D);
		item.visual->add_child(tag);
		tag->set_text(item.kind.to_upper());
		tag->set_font_size(40);
		tag->set_pixel_size(.004);
		tag->set_position(Vector3(0, .6, 0));
		tag->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		tag->set_modulate(item.kind == "key" ? Color(.95, .73, .35) : Color(.42, .8, .76));
	}
}
