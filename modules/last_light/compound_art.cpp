#include "compound_art.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/shader.h"
#include <cmath>
CompoundArt::CompoundArt(Node3D *r, NavigationRegion3D *n, bool visual, std::function<Ref<PackedScene>(const String &)> l) : root(r), nav(n), render(visual), loader(l) {
	if (!render) {
		return;
	}
	Ref<BoxMesh> b;
	b.instantiate();
	b->set_size(Vector3(1, 1, 1));
	cube = b;
	Ref<CylinderMesh> c;
	c.instantiate();
	c->set_top_radius(.5);
	c->set_bottom_radius(.5);
	c->set_height(1);
	c->set_radial_segments(12);
	cylinder = c;
	shader.instantiate();
	shader->set_code(R"SHADER(shader_type spatial;
uniform vec4 paint : source_color=vec4(1.0);
uniform int surface=0;
uniform sampler2D palette : source_color,filter_nearest_mipmap;
uniform bool use_palette=false;
varying vec3 local;varying vec2 extent;
varying vec3 world;varying vec3 world_normal;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void vertex(){local=VERTEX;extent=vec2(length(MODEL_MATRIX[0].xyz),length(MODEL_MATRIX[2].xyz));world=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;world_normal=normalize(MODEL_NORMAL_MATRIX*NORMAL);}
void fragment(){
 vec3 n=normalize(world_normal);vec2 p=abs(n.y)>.7?world.xz:(abs(n.x)>.7?world.zy:world.xy);
 float grain=hash(floor(p*18.0));float broad=hash(floor(p*.48));vec3 c=paint.rgb;if(use_palette)c*=texture(palette,UV).rgb;
 if(surface==0){float seam=max(step(.985,fract(p.x*.42)),step(.986,fract(p.y*.32)));c*=.91+grain*.06+broad*.09;c=mix(c,c*.52,seam*.65);}
 if(surface==1){c*=.86+grain*.10;ROUGHNESS=.55;METALLIC=.35;}else{ROUGHNESS=.9;}
 if(surface==2){c*=.88+grain*.1+broad*.1;}
 if(surface==3||surface==9){c*=.78+grain*.16+broad*.08;float edge=step(.98,fract(p.x*.18))*step(.98,fract(p.y*.18));c*=1.0-edge*.45;float border=min((.5-abs(local.x))*extent.x,(.5-abs(local.z))*extent.y);if(surface==3)c=mix(vec3(.4,.5,.57),c,smoothstep(.02,.65+hash(floor(p*2.0))*.4,border));}
 if(surface==4){c*=.94+grain*.05+sin(world.x*.14+sin(world.z*.21))*.02;}
 if(surface==5){c*=.7+step(.5,fract(p.x*.5))*.2;ROUGHNESS=.25;METALLIC=.45;EMISSION=c*.12;}
 if(surface==6){EMISSION=c*1.4;ROUGHNESS=.6;}if(surface==8){float cap=smoothstep(.15,.65,n.y);c=mix(c*vec3(.8,.9,1.),vec3(.48,.6,.68),cap*.85);}
 ALBEDO=c;
}
)SHADER");
}
Ref<Material> CompoundArt::material(const Color &c, int surface) {
	String name = c.to_html() + itos(surface);
	std::string key = name.utf8().get_data();
	auto found = materials.find(key);
	if (found != materials.end()) {
		return found->second;
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("paint", c);
	m->set_shader_parameter("surface", surface);
	materials[key] = m;
	return m;
}
void CompoundArt::instance(const String &key, const Ref<Mesh> &mesh, const Transform3D &t, Node3D *parent) {
	if (!render || mesh.is_null()) {
		return;
	}
	String name = key + ":" + itos((parent ? parent : root)->get_instance_id());
	auto &b = batches[std::string(name.utf8().get_data())];
	b.mesh = mesh;
	b.parent = parent ? parent : root;
	b.transforms.push_back(t);
}
StaticBody3D *CompoundArt::box(const Vector3 &p, const Vector3 &size, const Color &color, bool collision, int surface, const Basis &rotation, Node3D *parent) {
	if (render) {
		String key = "box:" + color.to_html() + itos(surface);
		std::string id = key.utf8().get_data();
		Ref<Mesh> mesh;
		auto found = batches.find(id + ":" + std::to_string(uint64_t((parent ? parent : root)->get_instance_id())));
		if (found != batches.end()) {
			mesh = found->second.mesh;
		} else {
			mesh = cube->duplicate();
			mesh->surface_set_material(0, material(color, surface));
		}
		instance(key, mesh, Transform3D(rotation.scaled_local(size), p), parent);
	}
	if (!collision) {
		return nullptr;
	}
	auto body = memnew(StaticBody3D);
	nav->add_child(body);
	body->set_position(p);
	body->set_basis(rotation);
	body->set_collision_layer(1);
	body->set_collision_mask(2);
	auto child = memnew(CollisionShape3D);
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(size);
	child->set_shape(shape);
	body->add_child(child);
	return body;
}
void CompoundArt::tube(const Vector3 &a, const Vector3 &b, float radius, const Color &c, int surface, Node3D *parent) {
	if (!render) {
		return;
	}
	Vector3 axis = b - a;
	if (axis.length() < .001) {
		return;
	}
	Vector3 up = axis.normalized();
	Vector3 tangent = up.cross(Math::abs(up.y) < .95 ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).normalized();
	Basis basis(tangent, up, tangent.cross(up));
	basis = basis.scaled_local(Vector3(radius * 2, axis.length(), radius * 2));
	String key = "tube:" + c.to_html() + itos(surface);
	Ref<Mesh> mesh = cylinder->duplicate();
	mesh->surface_set_material(0, material(c, surface));
	instance(key, mesh, Transform3D(basis, (a + b) * .5f), parent);
}
void CompoundArt::disc(const Vector3 &p, float radius, float height, const Color &c, int surface, Node3D *parent) {
	tube(p - Vector3(0, height * .5, 0), p + Vector3(0, height * .5, 0), radius, c, surface, parent);
}
void CompoundArt::asset(const String &path, const Vector3 &p, const Vector3 &size, float yaw, Node3D *parent) {
	if (!render) {
		return;
	}
	auto packed = loader(path);
	if (packed.is_null()) {
		return;
	}
	Node *node = packed->instantiate();
	struct Part {
		Ref<Mesh> mesh;
		Transform3D transform;
	};
	std::vector<Part> parts;
	AABB bounds;
	bool first = true;
	auto collect = [&](auto &&self, Node *n, Transform3D t) -> void {if(auto spatial=Object::cast_to<Node3D>(n)){t=t*spatial->get_transform();}if(auto m=Object::cast_to<MeshInstance3D>(n)){if(m->get_mesh().is_valid()){parts.push_back({m->get_mesh(),t});auto box=t.xform(m->get_aabb());if(first){bounds=box;first=false;}else{bounds.merge_with(box);}}}for(int i=0;i<n->get_child_count();++i){self(self,n->get_child(i),t);
} };
	collect(collect, node, Transform3D());
	if (!first) {
		Vector3 scale;
		for (int i = 0; i < 3; ++i) {
			scale[i] = bounds.size[i] > .0001 ? size[i] / bounds.size[i] : 1;
		}
		Transform3D norm(Basis().scaled(scale), -Vector3(bounds.position.x + bounds.size.x * .5, bounds.position.y, bounds.position.z + bounds.size.z * .5) * scale);
		Transform3D placement(Basis(Vector3(0, 1, 0), yaw), p);
		for (size_t i = 0; i < parts.size(); ++i) {
			Ref<Mesh> mesh = parts[i].mesh;
			if (path.contains("nature-kit/tree_")) {
				mesh = mesh->duplicate();
				for (int face = 0; face < mesh->get_surface_count(); ++face) {
					Ref<BaseMaterial3D> old = parts[i].mesh->surface_get_material(face);
					Ref<ShaderMaterial> m;
					m.instantiate();
					m->set_shader(shader);
					m->set_shader_parameter("paint", old.is_valid() ? old->get_albedo() : Color(1, 1, 1));
					m->set_shader_parameter("surface", 8);
					if (old.is_valid()) {
						auto texture = old->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
						m->set_shader_parameter("use_palette", texture.is_valid());
						if (texture.is_valid()) {
							m->set_shader_parameter("palette", texture);
						}
					}
					mesh->surface_set_material(face, m);
				}
			}
			instance("asset:" + path + ":" + itos(i), mesh, placement * norm * parts[i].transform, parent);
		}
	}
	memdelete(node);
}
void CompoundArt::label(const String &text, const Vector3 &p, const Color &c, float scale) {
	if (!render) {
		return;
	}
	auto label = memnew(Label3D);
	root->add_child(label);
	label->set_position(p);
	label->set_text(text);
	label->set_font_size(48);
	label->set_pixel_size(scale);
	label->set_modulate(c);
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	label->set_visibility_range_end(65);
}
void CompoundArt::light(const Vector3 &p, const Color &c, float radius, float energy) {
	if (!render) {
		return;
	}
	auto l = memnew(OmniLight3D);
	root->add_child(l);
	l->set_position(p);
	l->set_color(c);
	l->set_param(Light3D::PARAM_RANGE, radius);
	l->set_param(Light3D::PARAM_ENERGY, energy);
	l->set_shadow(false);
}
void CompoundArt::flush() {
	for (auto &[key, b] : batches) {
		if (b.transforms.empty()) {
			continue;
		}
		Ref<MultiMesh> mesh;
		mesh.instantiate();
		mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
		mesh->set_mesh(b.mesh);
		mesh->set_instance_count(b.transforms.size());
		for (size_t i = 0; i < b.transforms.size(); ++i) {
			mesh->set_instance_transform(i, b.transforms[i]);
		}
		auto visual = memnew(MultiMeshInstance3D);
		b.parent->add_child(visual);
		visual->set_multimesh(mesh);
		visual->set_visibility_range_end(220);
	}
	batches.clear();
}
