#pragma once
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include <functional>
#include <map>
#include <string>
#include <vector>
// Shared static geometry keeps the richer level practical on integrated GPUs.
class CompoundArt {
	struct Batch {
		Ref<Mesh> mesh;
		Node3D *parent = nullptr;
		std::vector<Transform3D> transforms;
	};
	Node3D *root;
	NavigationRegion3D *nav;
	bool render;
	std::function<Ref<PackedScene>(const String &)> loader;
	std::map<std::string, Batch> batches;
	std::map<std::string, Ref<Material>> materials;
	Ref<Shader> shader;
	Ref<Mesh> cube, cylinder, sphere;
	Ref<Material> material(const Color &color, int surface);
	void instance(const String &key, const Ref<Mesh> &mesh, const Transform3D &transform, Node3D *parent);

public:
	CompoundArt(Node3D *p_root, NavigationRegion3D *p_nav, bool p_render, std::function<Ref<PackedScene>(const String &)> p_loader);
	StaticBody3D *box(const Vector3 &p, const Vector3 &size, const Color &color, bool collision = false, int surface = 0, const Basis &rotation = Basis(), Node3D *parent = nullptr);
	void tube(const Vector3 &a, const Vector3 &b, float radius, const Color &color, int surface = 1, Node3D *parent = nullptr);
	void disc(const Vector3 &p, float radius, float height, const Color &color, int surface = 1, Node3D *parent = nullptr);
	void asset(const String &path, const Vector3 &p, const Vector3 &size, float yaw = 0, Node3D *parent = nullptr);
	void label(const String &text, const Vector3 &p, const Color &color, float scale = .007f);
	void light(const Vector3 &p, const Color &color, float radius, float energy);
	void flush();
};
