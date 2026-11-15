#include "compound_art.h"
#include "last_light_game.h"
#include "scene/3d/light_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/display_server.h"
#include "servers/navigation_server_3d.h"
#include <cmath>
void LastLightGame::add_cover_box(const Vector3 &center, const Vector3 &size, const String &label, float yaw) {
	if (size.y < .65f) {
		return;
	}
	Basis rotate(Vector3(0, 1, 0), yaw);
	for (int face = 0; face < 4; ++face) {
		bool x = face < 2;
		float side = face % 2 ? 1.f : -1.f;
		Vector3 normal = x ? Vector3(side, 0, 0) : Vector3(0, 0, side);
		Vector3 tangent = x ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
		float width = x ? size.z : size.x;
		float depth = x ? size.x : size.z;
		int count = CLAMP(int(width / 3), 1, 6);
		for (int i = 0; i < count; ++i) {
			float along = count == 1 ? 0 : Math::lerp(-width * .5f + .8f, width * .5f - .8f, float(i) / (count - 1));
			Vector3 offset = normal * (depth * .5f + .85f) + tangent * along;
			CoverPoint point;
			point.id = cover_points.size();
			point.position = center + rotate.xform(offset);
			point.position.y = MAX(0.f, center.y - size.y * .5f + .02f);
			point.normal = rotate.xform(normal);
			point.height = size.y;
			point.label = label;
			point.peek_a = center + rotate.xform(normal * (depth * .5f + .85f) + tangent * (width * .5f + 1.05f));
			point.peek_b = center + rotate.xform(normal * (depth * .5f + .85f) - tangent * (width * .5f + 1.05f));
			point.peek_a.y = point.peek_b.y = point.position.y;
			cover_points.push_back(point);
		}
	}
}
void LastLightGame::create_compound_level() {
	config["deadline_seconds"] = 1800;
	config["variant"] = "frostworks-2";
	config["world"] = "arctic";
	level_root = memnew(Node3D);
	level_root->set_name("FrostworksIndustrialBase");
	add_child(level_root);
	region = memnew(NavigationRegion3D);
	level_root->add_child(region);
	bool visual = DisplayServer::get_singleton()->get_name() != "headless" || script_case == "compound_art";
	CompoundArt art(level_root, region, visual, [&](const String &path) { return load_asset_scene(path); });
	const Color snow(.4, .5, .57), concrete(.47, .48, .45), steel(.24, .31, .34), paint(.63, .57, .44), blue(.15, .27, .32), trim(.18, .22, .23), amber(.95, .65, .25), white(.83, .86, .84), red(.55, .22, .14), road(.26, .29, .28);
	const lastlight::StrategyNode nodes[] = { { { 128, 212 }, "Control post", true }, { { 82, 205 }, "West trail", false }, { { 48, 160 }, "Forest camp", true }, { { 89, 120 }, "Living quarters", true }, { { 96, 75 }, "Storage", true }, { { 122, 125 }, "Medical station", true }, { { 169, 152 }, "Generator hall", false }, { { 215, 164 }, "Ice bridge", false }, { { 221, 111 }, "East patrol road", false }, { { 185, 70 }, "Radio station", true }, { { 192, 32 }, "North extraction", false }, { { 42, 208 }, "Emergency landing", false }, { { 145, 42 }, "Watchtower", false }, { { 211, 212 }, "Supply shed", true }, { { 110, 180 }, "Maintenance hall", true }, { { 36, 105 }, "West ridge", false }, { { 130, 203 }, "Security checkpoint", false }, { { 155, 183 }, "Motor pool", false }, { { 105, 143 }, "Loading dock", false }, { { 164, 121 }, "Turbine floor", false }, { { 200, 133 }, "Pipe yard", false }, { { 141, 91 }, "North service lane", false }, { { 150, 75 }, "Command annex", true }, { { 70, 145 }, "West service lane", false }, { { 241, 177 }, "Pump house", true }, { { 62, 184 }, "West breach", false }, { { 127, 160 }, "West gantry", false, 4.2 }, { { 144, 160 }, "East gantry", false, 4.2 }, { { 127, 192 }, "Checkpoint bypass", false }, { { 121, 203 }, "West gate controls", false } };
	strategic_nodes.assign(std::begin(nodes), std::end(nodes));
	const lastlight::StrategyEdge links[] = { { 0, 16, .12, .25 }, { 0, 1, .08, .1 }, { 0, 13, .18, .3 }, { 1, 25, .08, .05 }, { 25, 2, .06, .04 }, { 25, 23, .1, .1 }, { 2, 23, .05, .03 }, { 2, 15, .08, .02 }, { 15, 4, .08, .03 }, { 23, 3, .08, .06 }, { 3, 4, .1, .1 }, { 3, 5, .12, .1 }, { 4, 21, .2, .2 }, { 21, 22, .15, .1 }, { 22, 9, .2, .1 }, { 9, 10, .17, .1 }, { 9, 8, .27, .2 }, { 8, 20, .35, .35 }, { 20, 6, .15, .2 }, { 6, 19, .12, .28 }, { 19, 21, .14, .18 }, { 5, 19, .35, .38 }, { 5, 18, .12, .1 }, { 18, 14, .15, .18 }, { 14, 28, .08, .1 }, { 29, 16, .06, .05, true }, { 1, 29, .05, .05 }, { 0, 29, .08, .1 }, { 16, 17, .5, .65 }, { 17, 6, .4, .55 }, { 17, 13, .35, .5 }, { 13, 24, .3, .35 }, { 24, 7, .4, .38 }, { 7, 20, .32, .32 }, { 1, 11, .05, .04 }, { 25, 11, .06, .02 }, { 18, 26, .15, .4 }, { 26, 27, .22, .45 }, { 27, 6, .1, .3 }, { 28, 17, .35, .4 }, { 14, 23, .1, .08 }, { 8, 12, .3, .2 }, { 12, 10, .18, .15 }, { 21, 12, .18, .14 } };
	strategic_edges.assign(std::begin(links), std::end(links));
	auto ellipse = [](float x, float z, float cx, float cz, float rx, float rz) { return (x - cx) * (x - cx) / (rx * rx) + (z - cz) * (z - cz) / (rz * rz) < 1; };
	Array rows;
	for (int z = 0; z < 151; ++z) {
		String line;
		for (int x = 0; x < 151; ++x) {
			float wx = x * 2, wz = z * 2;
			bool land = ellipse(wx, wz, 140, 126, 121, 109) || ellipse(wx, wz, 145, 215, 101, 71) || ellipse(wx, wz, 46, 190, 30, 42) || (wx >= 226 && wx <= 252 && wz >= 108 && wz <= 220);
			if (wx > 210 && wx < 224 && wz > 139 && wz < 202 && !(wz >= 159 && wz <= 169)) {
				land = false;
			}
			line += land ? '.' : '#';
		}
		map_rows.push_back(line);
		rows.push_back(line);
	}
	config["map"] = rows;
	// Snow land and cold channel share physical geometry with navigation.
	make_box(level_root, Vector3(150, -1.3, 150), Vector3(302, .4, 302), Color(.06, .2, .26), true);
	auto water = make_box(level_root, Vector3(150, -.08, 150), Vector3(302, .025, 302), Color(.12, .3, .36));
	if (auto mesh = Object::cast_to<MeshInstance3D>(water->get_child(0))) {
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_albedo(Color(.1, .28, .34, .8));
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		mat->set_roughness(.3);
		mesh->set_material_override(mat);
	}
	for (int z = 0; z < 151; ++z) {
		for (int x = 0; x < 151;) {
			if (map_rows[z][x] != '.') {
				++x;
				continue;
			}
			int start = x;
			while (x < 151 && map_rows[z][x] == '.') {
				++x;
			}
			art.box(Vector3(start + x - 1, -.15, z * 2), Vector3((x - start) * 2, .3, 2), snow, true, 4);
		}
	}
	art.box(Vector3(215, .02, 164), Vector3(24, .14, 10), steel, true, 1);
	std::vector<std::pair<Vector3, Vector3>> roads;
	auto paving = [&](Vector3 p, Vector3 size) { roads.push_back({ p, size }); };
	paving(Vector3(130, 0, 150), Vector3(12, 0, 140));
	paving(Vector3(155, 0, 182), Vector3(95, 0, 28));
	paving(Vector3(145, 0, 100), Vector3(102, 0, 14));
	paving(Vector3(205, 0, 145), Vector3(9, 0, 142));
	paving(Vector3(89, 0, 137), Vector3(13, 0, 105));
	paving(Vector3(155, 0, 138), Vector3(56, 0, 67));
	paving(Vector3(225, 0, 164), Vector3(38, 0, 8));
	paving(Vector3(129, 0, 215), Vector3(38, 0, 20));
	// Union overlapping road rectangles before rendering; coplanar patches cause
	// visible z-fighting even though their collision/navigation is correct.
	for (int z = 30; z < 250; z += 2) {
		for (int x = 20; x < 260;) {
			auto paved = [&](int px) {for(auto &[center,size]:roads){if(Math::abs(px+.5f-center.x)<size.x*.5f&&Math::abs(z+1.f-center.z)<size.z*.5f){return true;
}}return false; };
			if (!paved(x)) {
				++x;
				continue;
			}
			int start = x;
			while (x < 260 && paved(x)) {
				++x;
			}
			art.box(Vector3((start + x) * .5f, .014f, z + 1.f), Vector3(x - start, .025, 2), road, false, 9);
		}
	}
	for (int z = 90; z < 218; z += 10) {
		art.box(Vector3(130, .036, z), Vector3(.16, .018, 4), white);
	}
	for (int i = 0; i < 7; ++i) {
		float x = 137 + i * 8;
		art.box(Vector3(x, .04, 183), Vector3(.12, .018, 12), white);
		art.box(Vector3(x + 3.8, .04, 176.8), Vector3(7.6, .018, .12), white);
	}
	for (int z = 108; z < 192; z += 3) {
		art.box(Vector3(124, .04, z), Vector3(.14, .018, 1.7), amber);
		art.box(Vector3(136, .04, z), Vector3(.14, .018, 1.7), amber);
	}
	for (int track = 0; track < 48; ++track) {
		float x = 126 + (track % 4) * 2.1f, z = 105 + (track / 4) * 8.7f;
		art.box(Vector3(x, .041f, z), Vector3(.13, .016, 4.8), Color(.18, .21, .22), false, 3, Basis(Vector3(0, 1, 0), .08f * Math::sin(float(track))));
	}
	for (auto p : { Vector3(110, 0, 182), Vector3(92, 0, 137), Vector3(145, 0, 90), Vector3(207, 0, 180) }) {
		for (int k = 0; k < 8; ++k) {
			art.box(p + Vector3(k * 1.4, .08, 0), Vector3(.9, .13, .7), snow, false, 4);
		}
	}
	// Door openings are broad and multiple exits create real flanking routes.
	auto building = [&](Vector3 center, float w, float d, float h, int entrances, const String &name, int style) {
		art.box(center + Vector3(0, .012, 0), Vector3(w, .028, d), Color(.31, .34, .32), false, 2);
		auto wall = [&](Vector3 p, Vector3 size) {
			auto piece = [&](Vector3 q, Vector3 dim) {if(dim.x<.01||dim.y<.01||dim.z<.01){return;
}art.box(q,dim,paint,true,0);add_cover_box(q,dim,name+" wall"); };
			if (name.begins_with("TURBINE") && Math::abs(p.x - 150) < .1 && size.x < 1 && p.z + size.z * .5f > 157.6f && p.z - size.z * .5f < 162.4f) {
				float lo = p.z - size.z * .5f, hi = p.z + size.z * .5f;
				float left = MAX(0.f, 157.6f - lo), right = MAX(0.f, hi - 162.4f);
				piece(Vector3(p.x, p.y, lo + left * .5f), Vector3(size.x, size.y, left));
				piece(Vector3(p.x, p.y, hi - right * .5f), Vector3(size.x, size.y, right));
				float a = MAX(lo, 157.6f), b = MIN(hi, 162.4f);
				piece(Vector3(p.x, 2.1f, (a + b) * .5f), Vector3(size.x, 4.2f, b - a));
				piece(Vector3(p.x, (6.4f + h) * .5f, (a + b) * .5f), Vector3(size.x, h - 6.4f, b - a));
			} else {
				piece(p, size);
			}
		};
		for (int side = 0; side < 4; ++side) {
			bool along_x = side == 0 || side == 2;
			float sign = side < 2 ? -1.f : 1.f;
			if (side == 1) {
				sign = 1;
			}
			if (side == 3) {
				sign = -1;
			}
			float length = along_x ? w : d;
			float opening = style == 1 ? 5.4f : 4.2f;
			float opening_height = name.begins_with("MAINTENANCE") && side == 1 ? 6.4f : 3.9f;
			Vector3 base = center + (along_x ? Vector3(0, h * .5f, sign * d * .5f) : Vector3(sign * w * .5f, h * .5f, 0));
			auto size = [&](float l, float height) { return along_x ? Vector3(l, height, .42) : Vector3(.42, height, l); };
			if (entrances & (1 << side)) {
				float panel = (length - opening) * .5f;
				for (int s : { -1, 1 }) {
					wall(base + (along_x ? Vector3(s * (opening * .5f + panel * .5f), 0, 0) : Vector3(0, 0, s * (opening * .5f + panel * .5f))), size(panel, h));
				}
				wall(base + Vector3(0, opening_height * .5f, 0), size(opening, MAX(.1f, h - opening_height)));
				Vector3 entry = base;
				entry.y = 0;
				int id = add_world_object("door", name + " access", entry);
				auto &door = world_objects.back();
				Vector3 door_size = along_x ? Vector3(opening - .3f, 3.65, .2) : Vector3(.2, 3.65, opening - .3f);
				door.body = Object::cast_to<StaticBody3D>(make_box(level_root, entry + Vector3(0, 1.825, 0), door_size, blue, true));
				door.value = along_x ? 0 : 2;
				for (int s : { -1, 1 }) {
					Vector3 p = entry + (along_x ? Vector3(s * (opening * .5f + .13f), opening_height * .5f, 0) : Vector3(0, opening_height * .5f, s * (opening * .5f + .13f)));
					art.box(p, along_x ? Vector3(.22, opening_height + .2f, .7) : Vector3(.7, opening_height + .2f, .22), trim, false, 1);
				}
				art.box(entry + Vector3(0, opening_height + .1f, 0), along_x ? Vector3(opening + .7, .18, .75) : Vector3(.75, .18, opening + .7), steel, false, 1);
				art.light(entry + Vector3(0, 3.1, along_x ? sign * 1.1f : 0), amber, 9, 1.2);
			} else {
				wall(base, size(length, h));
			}
			// Pilasters, lower protective panels, framed windows and exterior piping.
			int divisions = MAX(2, int(length / 4));
			for (int i = 0; i <= divisions; ++i) {
				float offset = -length * .5f + i * length / divisions;
				if ((entrances & (1 << side)) && Math::abs(offset) < opening * .6f) {
					continue;
				}
				Vector3 p = base + (along_x ? Vector3(offset, 0, sign * .26f) : Vector3(sign * .26f, 0, offset));
				art.box(p, size(.18, h + .25), trim, false, 1);
				if (i < divisions) {
					Vector3 q = p + (along_x ? Vector3(length / divisions * .5f, 0, 0) : Vector3(0, 0, length / divisions * .5f));
					q.y = MIN(h - 1.2f, style == 2 ? 4.9f : 3.f);
					art.box(q, size(length / divisions - .45f, style == 2 ? 1.55f : .85f), Color(.19, .34, .39), false, 5);
					art.box(q + Vector3(0, -.55, 0), size(length / divisions - .3f, .1), white, false, 1);
				}
			}
			if (h > 6) {
				Vector3 band = base;
				band.y = h - 1.15f;
				art.box(band, size(length - .7f, .78f), Color(.24, .36, .38), false, 5);
				for (int column = 0; column <= int(length / 2.5f); ++column) {
					float off = -length * .5f + column * 2.5f;
					art.box(band + (along_x ? Vector3(off, 0, 0) : Vector3(0, 0, off)), size(.09, .85), trim, false, 1);
				}
			}
			art.box(base + Vector3(0, -h * .5f + .55f, 0), size(length, .24f), blue);
			art.box(base + Vector3(0, h * .5f - .15f, 0), size(length, .22f), trim, false, 1);
		}
		Node3D *roof = memnew(Node3D);
		level_root->add_child(roof);
		roofs.push_back({ roof, center + Vector3(0, h, 0), Vector3(w, h, d) });
		art.box(center + Vector3(0, h + .12f, 0), Vector3(w + .75, .25, d + .75), steel, false, 1, Basis(), roof);
		for (int side : { -1, 1 }) {
			art.box(center + Vector3(side * (w * .5f - .45f), h + .27f, 0), Vector3(.8, .07, d + .65f), snow, false, 4, Basis(), roof);
			art.box(center + Vector3(0, h + .27f, side * (d * .5f - .4f)), Vector3(w + .6f, .07, .75), snow, false, 4, Basis(), roof);
		}
		for (int patch = 0; patch < 4; ++patch) {
			art.box(center + Vector3((patch % 2 ? 1.f : -1.f) * w * .3f, h + .27f, (patch / 2 ? 1.f : -1.f) * d * .3f), Vector3(w * .14f, .07, d * .16f), snow, false, 4, Basis(), roof);
		}
		for (int s : { -1, 1 }) {
			art.box(center + Vector3(s * w * .5f, h + .3f, 0), Vector3(.16, .42, d + .6), white, false, 1, Basis(), roof);
			art.box(center + Vector3(0, h + .3f, s * d * .5f), Vector3(w + .6, .42, .16), white, false, 1, Basis(), roof);
		}
		if (style == 1) {
			for (int row = 0; row < int(d / 7); ++row) {
				float z = -d * .35f + row * 7;
				for (int band = 0; band < 8; ++band) {
					float x0 = -w * .36f + band * w * .72f / 8, x1 = x0 + w * .72f / 8;
					float y0 = h + .4f + Math::sin((float(band) / 8) * Math::PI) * 1.45f, y1 = h + .4f + Math::sin((float(band + 1) / 8) * Math::PI) * 1.45f;
					float angle = Math::atan2(y1 - y0, x1 - x0);
					Vector3 mid = center + Vector3((x0 + x1) * .5f, (y0 + y1) * .5f, z);
					float length = Vector2(x1 - x0, y1 - y0).length();
					art.box(mid, Vector3(length, .1, 4.7), Color(.26, .4, .43), false, 5, Basis(Vector3(0, 0, 1), angle), roof);
					for (int edge : { -1, 1 }) {
						art.box(mid + Vector3(0, .06, edge * 2.35f), Vector3(length, .16, .14), white, false, 1, Basis(Vector3(0, 0, 1), angle), roof);
					}
				}
			}
		} else if (style == 0) {
			for (int side : { -1, 1 }) {
				for (int j = 0; j < MAX(2, int(d / 8)); ++j) {
					Vector3 p = center + Vector3(side * w * .27f, h + .65f, -d * .33f + j * 7);
					art.box(p, Vector3(4.5, 1.1, 4.8), Color(.44, .47, .45), false, 1, Basis(), roof);
					for (int q = 0; q < 2; ++q) {
						Vector3 v = p + Vector3((q ? 1 : -1) * 1.1f, .6, 0);
						art.disc(v, 1, .18, trim, 1, roof);
						for (int k = 0; k < 5; ++k) {
							art.box(v + Vector3(0, .1, 0), Vector3(1.7, .04, .17), steel, false, 1, Basis(Vector3(0, 1, 0), k * Math::TAU / 5), roof);
						}
					}
				}
			}
			art.tube(center + Vector3(0, h + .6f, -d * .44f), center + Vector3(0, h + .6f, d * .44f), .2f, Color(.58, .42, .23), 1, roof);
		} else {
			Vector3 p = center + Vector3(w * .22f, h + .85f, 0);
			art.box(p, Vector3(w * .3f, 1.5, d * .35f), trim, false, 1, Basis(), roof);
			art.box(p + Vector3(0, .05, -d * .18f), Vector3(w * .27f, .7, .08), Color(.21, .38, .43), false, 5, Basis(), roof);
			art.tube(center + Vector3(-w * .28f, h + .3f, d * .22f), center + Vector3(-w * .28f, h + 5, d * .22f), .06, white, 1, roof);
			art.tube(center + Vector3(-w * .28f - 1.2f, h + 4, d * .22f), center + Vector3(-w * .28f + 1.2f, h + 4, d * .22f), .04, steel, 1, roof);
			art.disc(center + Vector3(w * .25f, h + 1.75f, -d * .25f), 1.2, .18, white, 1, roof);
		}
		art.label(name, center + Vector3(0, h + 1, -d * .5f), white, .006);
		art.light(center + Vector3(0, 3.2, 0), Color(1, .78, .5), MAX(w, d) * .65f, 1.6);
		for (int s : { -1, 1 }) {
			art.box(center + Vector3(s * w * .38f, 2.8, d * .38f), Vector3(1.9, .2, .3), amber, false, 6);
		}
	};
	building(Vector3(128, 0, 222), 24, 18, 4.8, 1 | 4, "CONTROL / 01", 2);
	building(Vector3(105, 0, 161), 28, 34, 6.5, 1 | 2 | 4 | 8, "MAINTENANCE / 02", 0);
	building(Vector3(166, 0, 137), 32, 52, 8.2, 1 | 2 | 4 | 8, "TURBINE HALL / 03", 1);
	building(Vector3(145, 0, 73), 38, 22, 7.3, 2 | 4 | 8, "COMMAND / 04", 2);
	building(Vector3(96, 0, 75), 28, 24, 6.2, 2 | 4, "STORES / 05", 0);
	building(Vector3(88, 0, 118), 22, 18, 4.4, 2 | 4 | 8, "BARRACKS / 06", 0);
	building(Vector3(122, 0, 125), 16, 16, 4.4, 1 | 4, "MEDICAL / 07", 2);
	building(Vector3(185, 0, 70), 16, 18, 5.2, 4 | 8, "SIGNAL / 08", 2);
	building(Vector3(211, 0, 212), 18, 16, 4.6, 1 | 8, "SUPPLY / 09", 0);
	building(Vector3(241, 0, 177), 14, 12, 4.6, 1 | 8, "PUMP CONTROL", 1);
	// Furniture is placed against walls; clear routes remain through every room.
	auto prop = [&](const String &pack, const String &name, Vector3 p, Vector3 size, float yaw = 0) { art.asset("res://assets/arctic/" + pack + "/" + name + ".glb", p, size, yaw); };
	auto solid_prop = [&](const String &pack, const String &name, Vector3 p, Vector3 size, float yaw = 0) {prop(pack,name,p,size,yaw);art.box(p+Vector3(0,size.y*.5f,0),Vector3(size.x*.9f,size.y,size.z*.9f),trim,true,1,Basis(Vector3(0,1,0),yaw));add_cover_box(p+Vector3(0,size.y*.5f,0),size,name,yaw); };
	for (Vector3 c : { Vector3(128, 0, 222), Vector3(145, 0, 73), Vector3(185, 0, 70) }) {
		solid_prop("furniture-kit", "desk", c + Vector3(-4, 0, -2), Vector3(2, .9, 1));
		prop("furniture-kit", "computerScreen", c + Vector3(-4, 1, -2), Vector3(.65, .45, .2));
		prop("furniture-kit", "radio", c + Vector3(-3.2, 1, -2), Vector3(.35, .3, .25));
		prop("furniture-kit", "chair", c + Vector3(-4, 0, 0), Vector3(.7, 1, .7));
		prop("furniture-kit", "table", c + Vector3(3, 0, 3), Vector3(2, .85, 1));
	}
	for (int i = 0; i < 4; ++i) {
		prop("furniture-kit", "bedBunk", Vector3(80 + (i % 2) * 15, 0, 113 + (i / 2) * 10), Vector3(2.1, 1.9, 1));
		prop("furniture-kit", "bookcaseClosed", Vector3(81 + i * 4, 0, 125), Vector3(2, 2, .45));
	}
	for (int i = 0; i < 3; ++i) {
		prop("furniture-kit", "bedSingle", Vector3(117 + i * 4.5f, 0, 129), Vector3(1.7, .75, 2.3));
		prop("furniture-kit", "kitchenCabinet", Vector3(117 + i * 4.5f, 0, 120), Vector3(1.8, 1.2, .6));
	}
	for (int i = 0; i < 5; ++i) {
		Vector3 p(94 + (i % 2) * 21, 0, 149 + (i / 2) * 8);
		solid_prop("survival-kit", "workbench", p, Vector3(2.7, 1.2, 1));
		prop("survival-kit", "tool-hammer", p + Vector3(.3, 1.25, 0), Vector3(.45, .2, .25));
	}
	for (int i = 0; i < 4; ++i) {
		Vector3 p(157 + (i % 2) * 18, 0, 123 + (i / 2) * 25);
		art.box(p + Vector3(0, 1.5, 0), Vector3(4, 3, 8), steel, true, 1);
		add_cover_box(p + Vector3(0, 1.5, 0), Vector3(4, 3, 8), "Turbine housing");
		art.tube(p + Vector3(0, 2.5, -2.5), p + Vector3(0, 2.5, 2.5), 1.5, Color(.48, .5, .48));
		for (int k = 0; k < 5; ++k) {
			art.disc(p + Vector3(0, 3.15, -2 + k), .7, .2, trim);
		}
	}
	// Working yard: parked vehicles, loading bays and discontinuous cover lines.
	for (int i = 0; i < 6; ++i) {
		float x = 139 + i * 8;
		Vector3 p(x, 0, i % 2 ? 185 : 181);
		String model = i == 0 ? "ambulance" : i == 3 ? "delivery"
													 : "truck-flat";
		Vector3 size = i == 0 ? Vector3(2.5, 2.5, 5.2) : Vector3(2.6, 2.8, 6.2);
		solid_prop("car-kit", model, p, size, Math::PI);
	}
	auto barrier = [&](Vector3 p, float yaw, const String &name) {Basis rot(Vector3(0,1,0),yaw);art.box(p+Vector3(0,.52,0),Vector3(3.8,1.04,.7),concrete,true,2,rot);art.box(p+Vector3(0,.12,0),Vector3(4,.24,1),concrete,false,2,rot);for(int i=0;i<5;++i){art.box(p+rot.xform(Vector3(-1.5+i*.7f,1.08,0)),Vector3(.35,.06,.72),amber,false,0,rot);
}add_cover_box(p+Vector3(0,.52,0),Vector3(3.8,1.04,.7),name,yaw); };
	for (auto p : { Vector3(123, 0, 193), Vector3(137, 0, 193), Vector3(138, 0, 165), Vector3(130, 0, 145), Vector3(136, 0, 119), Vector3(145, 0, 102), Vector3(181, 0, 93), Vector3(206, 0, 175), Vector3(230, 0, 164) }) {
		barrier(p, 0, "Concrete cover");
	}
	for (auto p : { Vector3(119, 0, 187), Vector3(123, 0, 157), Vector3(144, 0, 156), Vector3(143, 0, 132), Vector3(200, 0, 146), Vector3(202, 0, 118), Vector3(82, 0, 153), Vector3(74, 0, 177) }) {
		barrier(p, Math::PI / 2, "Flank cover");
	}
	for (int i = 0; i < 8; ++i) {
		Vector3 p(194 + (i % 2) * 8, 0, 119 + (i / 2) * 11);
		solid_prop("city-kit-industrial", i % 2 ? "shipping-container-b" : "shipping-container-a", p, Vector3(6, 2.8, 3));
		if (i % 3 == 0) {
			prop("city-kit-industrial", "shipping-container-c", p + Vector3(0, 2.85, 0), Vector3(6, 2.8, 3));
		}
	}
	for (int i = 0; i < 12; ++i) {
		Vector3 p(84 + (i % 3) * 8, 0, 60 + (i / 3) * 3);
		if (i < 6) {
			prop("survival-kit", "box-large", p, Vector3(1.4, 1.3, 1.2));
		}
	}
	for (int i = 0; i < 9; ++i) {
		Vector3 p(197 + (i % 3) * 2, 0, 145 + (i / 3) * 2);
		solid_prop("survival-kit", "barrel", p, Vector3(.85, 1.2, .85));
	}
	// Pipe bridges, valves, water tower, rooftop machinery and a traversable gantry.
	for (int s : { -1, 1 }) {
		art.tube(Vector3(119, 4.8, 150 + s * .45f), Vector3(194, 4.8, 150 + s * .45f), .27, Color(.54, .36, .19));
		art.tube(Vector3(194, 1.8, 132 + s * .5f), Vector3(194, 4.8, 150 + s * .45f), .27, steel);
	}
	for (float x : { 121.f, 139.f, 158.f, 178.f, 195.f }) {
		art.box(Vector3(x, 2.1, 150), Vector3(.25, 4.2, 1.6), steel, true, 1);
		art.box(Vector3(x, 4.5, 150), Vector3(1, .3, 2.8), steel, false, 1);
	}
	solid_prop("city-kit-industrial", "water-tower", Vector3(193, 0, 104), Vector3(7, 14, 7));
	prop("city-kit-industrial", "chimney-large", Vector3(174, 0, 109), Vector3(3, 17, 3));
	prop("city-kit-industrial", "solar-panel-landscape-group", Vector3(231, 0, 192), Vector3(8, 3, 5));
	art.box(Vector3(146, 4.1, 160), Vector3(60, .22, 3.6), steel, true, 1);
	for (float z : { 158.2f, 161.8f }) {
		std::vector<std::pair<float, float>> pieces = z > 160 ? std::vector<std::pair<float, float>>{ { 116, 125 }, { 129, 142 }, { 146, 176 } } : std::vector<std::pair<float, float>>{ { 116, 170 }, { 174, 176 } };
		for (auto [lo, hi] : pieces) {
			art.box(Vector3((lo + hi) * .5f, 4.65, z), Vector3(hi - lo, .85, .14), steel, true, 1);
			add_cover_box(Vector3((lo + hi) * .5f, 4.65, z), Vector3(hi - lo, .85, .14), "Gantry railing");
			for (float x = lo + .2f; x < hi; x += 3.8f) {
				art.box(Vector3(x, 4.6, z), Vector3(.09, 1.15, .09), trim);
			}
		}
	}
	auto ramp = [&](Vector3 from, Vector3 to) {Vector3 delta=to-from;float flat=Vector2(delta.x,delta.z).length();Basis basis=Basis(Vector3(0,1,0),Math::atan2(delta.x,delta.z))*Basis(Vector3(1,0,0),-Math::atan2(delta.y,flat));art.box((from+to)*.5f-Vector3(0,.1,0),Vector3(3.6,.22,delta.length()+.5f),steel,true,1,basis);for(int side:{-1,1}){art.box((from+to)*.5f+Vector3(side*1.75f,.55,0),Vector3(.12,.9,delta.length()),steel,true,1,basis);
} };
	ramp(Vector3(127, 0, 178), Vector3(127, 4.21, 161.8));
	ramp(Vector3(144, 0, 178), Vector3(144, 4.21, 161.8));
	ramp(Vector3(172, 0, 141), Vector3(172, 4.21, 158.2));
	art.box(Vector3(115, 4.1, 161), Vector3(8, .22, 6), steel, true, 1);
	// Perimeter security and distinct approaches. Fence contains traversable gaps.
	for (auto p : { Vector3(75, 0, 92), Vector3(75, 0, 198), Vector3(207, 0, 95), Vector3(207, 0, 199) }) {
		for (int k = 0; k < 7; ++k) {
			art.box(p + Vector3(k * 4, 1.2, 0), Vector3(.12, 2.4, .12), steel, true, 1);
		}
		art.box(p + Vector3(12, 1.1, 0), Vector3(24, .08, .08), steel);
		art.box(p + Vector3(12, 2.1, 0), Vector3(24, .08, .08), steel);
	}
	for (auto p : { Vector3(121, 0, 207), Vector3(134, 0, 207), Vector3(127, 0, 187), Vector3(142, 0, 111), Vector3(181, 0, 82), Vector3(207, 0, 157), Vector3(87, 0, 139) }) {
		art.tube(p, p + Vector3(0, 6.5, 0), .095, trim);
		art.tube(p + Vector3(0, 6.4, 0), p + Vector3(1.8, 6.4, 0), .075, trim);
		art.box(p + Vector3(1.8, 6.35, 0), Vector3(.7, .14, .45), amber, false, 6);
		art.light(p + Vector3(1.8, 6.1, 0), Color(1, .82, .55), 13, 1.2);
	}
	// Functional mission stations are placed in the authored spaces.
	key_position = Vector3(92, .6, 75);
	generator_position = Vector3(169, 0, 152);
	radio_position = Vector3(185, .7, 70);
	exit_position = Vector3(192, 0, 32);
	alternative_exit = Vector3(42, 0, 208);
	add_item("key", key_position);
	add_world_object("generator", "Emergency turbine controls", generator_position);
	world_objects.back().body = art.box(generator_position + Vector3(0, .85, 0), Vector3(1.4, 1.7, .7), blue, true, 1);
	add_world_object("security", "Secure turbine service panel", generator_position + Vector3(2.5, 0, 0));
	world_objects.back().body = art.box(generator_position + Vector3(2.5, .9, 0), Vector3(.5, 1.8, .5), amber, true, 1);
	add_world_object("radio", "Signal console", radio_position);
	add_world_object("relay", "Storage relay", Vector3(98, 0, 78));
	add_world_object("relay", "Supply relay", Vector3(211, 0, 214));
	add_world_object("relay", "Ridge relay", Vector3(36, 0, 105));
	for (auto p : { Vector3(48, 0, 160), Vector3(128, 0, 222), Vector3(88, 0, 118) }) {
		add_world_object("shelter", "Heated shelter", p);
		prop("survival-kit", "campfire-pit", p, Vector3(1, .25, 1));
		art.light(p + Vector3(0, .8, 0), amber, 8, 1.5);
	}
	for (int i = 0; i < 2; ++i) {
		Vector3 p = i ? alternative_exit : exit_position;
		add_world_object("extraction", i ? "West landing beacon" : "North landing beacon", p);
		world_objects.back().value = i;
		art.box(p + Vector3(0, .025, 0), Vector3(10, .04, 10), road, false, 3);
		for (int s : { -1, 1 }) {
			art.box(p + Vector3(s * 4, .055, 0), Vector3(.16, .03, 8), white);
			art.box(p + Vector3(0, .055, s * 4), Vector3(8, .03, .16), white);
		}
		art.box(p + Vector3(0, .06, 0), Vector3(4, .03, .3), amber);
		art.box(p + Vector3(-1.8, .06, 0), Vector3(.3, .03, 3), amber);
		art.box(p + Vector3(1.8, .06, 0), Vector3(.3, .03, 3), amber);
		art.label(i ? "WEST AIRLIFT" : "NORTH AIRLIFT", p + Vector3(0, 2.5, 0), amber);
	}
	const char *bundles[][4] = { { "medkit", "bandage", "water", "food" }, { "rifle", "rifle_ammo", "armour", "bandage" }, { "food", "water", "coat", "backpack" }, { "fuel", "tools", "smoke", "water" }, { "smg", "smg_ammo", "decoy", "bandage" }, { "shotgun", "shells", "smoke", "food" } };
	const Vector3 caches[] = { { 121, 0, 216 }, { 135, 0, 225 }, { 104, 0, 174 }, { 94, 0, 158 }, { 113, 0, 149 }, { 158, 0, 154 }, { 176, 0, 127 }, { 153, 0, 116 }, { 92, 0, 69 }, { 104, 0, 81 }, { 82, 0, 115 }, { 95, 0, 123 }, { 118, 0, 120 }, { 127, 0, 130 }, { 140, 0, 76 }, { 156, 0, 68 }, { 181, 0, 66 }, { 190, 0, 74 }, { 208, 0, 208 }, { 217, 0, 216 }, { 235, 0, 177 }, { 239, 0, 182 }, { 48, 0, 164 }, { 35, 0, 105 }, { 63, 0, 181 }, { 190, 0, 129 }, { 201, 0, 151 }, { 145, 4.25, 160 }, { 169, 4.25, 160 }, { 154, 0, 191 }, { 177, 0, 188 }, { 196, 0, 101 } };
	for (int i = 0; i < int(std::size(caches)); ++i) {
		int type = (i + int(rng() % 6)) % 6;
		int id = add_world_object("crate", "Supply cache " + itos(i + 1), caches[i]);
		for (int j = 0; j < 4; ++j) {
			add_item(bundles[type][j], caches[i] + Vector3((j % 2) * .7 - .35, .35, (j / 2) * .6 + .8), id);
		}
		prop("survival-kit", "chest", caches[i], Vector3(1.1, .75, .7));
	}
	for (auto &o : world_objects) {
		if (o.kind == "relay") {
			art.box(o.position + Vector3(0, .8, 0), Vector3(.6, 1.6, .5), blue);
			art.tube(o.position + Vector3(0, 1.5, 0), o.position + Vector3(0, 3, 0), .035, white);
		}
		if (o.kind == "generator" || o.kind == "radio" || o.kind == "relay") {
			art.label(o.name, o.position + Vector3(0, 2, 0), amber, .0045);
		}
	}
	for (auto p : { Vector3(126, .35, 207), Vector3(131, .35, 208) }) {
		add_item("water", p);
		add_item("bandage", p + Vector3(.6, 0, 0));
	}
	// Curated tree clusters and rocks frame routes rather than scattering noise.
	const Vector3 groves[] = { { 36, 0, 143 }, { 52, 0, 121 }, { 47, 0, 188 }, { 61, 0, 221 }, { 28, 0, 171 }, { 46, 0, 75 }, { 63, 0, 61 }, { 232, 0, 86 }, { 239, 0, 222 }, { 202, 0, 246 }, { 175, 0, 252 }, { 104, 0, 245 }, { 115, 0, 38 } };
	for (int g = 0; g < int(std::size(groves)); ++g) {
		for (int k = 0; k < 9; ++k) {
			float angle = k * 2.4f + g;
			Vector3 p = groves[g] + Vector3(Math::cos(angle) * (3 + k * .65f), 0, Math::sin(angle) * (3 + k * .65f));
			int x = int(p.x / 2), z = int(p.z / 2);
			if (x < 1 || x >= 150 || z < 1 || z >= 150 || map_rows[z][x] != '.') {
				continue;
			}
			float h = 5 + (k % 4) * 1.1;
			prop("nature-kit", k % 2 ? "tree_pineTallA" : "tree_pineRoundC", p, Vector3(3, h, 3));
			art.box(p + Vector3(0, 1, 0), Vector3(.45, 2, .45), trim, true, 1);
			if (k % 4 == 0) {
				Vector3 r = p + Vector3(1.8, 0, 1);
				solid_prop("nature-kit", "rock_largeA", r, Vector3(2.4, 1.3, 1.8));
			}
		}
	}
	prop("nature-kit", "tent_detailedOpen", Vector3(49, 0, 155), Vector3(5, 2.7, 4));
	prop("nature-kit", "log_stack", Vector3(45, 0, 164), Vector3(3, 1.1, 1.5));
	// Background silhouettes are outside the playable service yards.
	prop("city-kit-industrial", "building-h", Vector3(62, 0, 36), Vector3(24, 15, 18));
	prop("city-kit-industrial", "building-e", Vector3(229, 0, 61), Vector3(21, 12, 17));
	prop("city-kit-industrial", "building-r", Vector3(159, 0, 17), Vector3(20, 14, 15));
	gate_closed_position = Vector3(123, 1.5, 203);
	gate = Object::cast_to<StaticBody3D>(make_box(region, gate_closed_position, Vector3(.35, 3, 4.5), blue, true));
	gate_mesh = Object::cast_to<MeshInstance3D>(gate->get_child(0));
	art.box(Vector3(123, 1.5, 195), Vector3(.35, 3, 11), blue, true);
	art.box(Vector3(123, 1.5, 210), Vector3(.35, 3, 9), blue, true);
	control_a = Vector3(121, .04, 200);
	control_b = Vector3(121, .04, 206);
	for (Vector3 p : { control_a, control_b }) {
		art.box(p, Vector3(1.4, .08, 1.4), amber);
	}
	art.flush();
	Ref<NavigationMesh> mesh;
	mesh.instantiate();
	mesh->set_parsed_geometry_type(NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS);
	mesh->set_collision_mask(1);
	mesh->set_cell_size(.45);
	mesh->set_cell_height(.15);
	mesh->set_agent_radius(.45);
	mesh->set_agent_height(1.8);
	mesh->set_agent_max_climb(.3);
	auto server = NavigationServer3D::get_singleton();
	server->map_set_cell_size(get_world_3d()->get_navigation_map(), .45);
	server->map_set_cell_height(get_world_3d()->get_navigation_map(), .15);
	region->set_navigation_mesh(mesh);
	region->bake_navigation_mesh(false);
	for (int id = 1; id <= 5; ++id) {
		Vector3 p = id == 1 ? Vector3(129, .1, 207) : Vector3(123 + (id - 2) * 3, .1, 218);
		create_actor(id, p, true, Color(.3, .5, .6));
		auto &a = actors.back();
		int weapon = id == 3 ? 1 : id == 4 ? 2
										   : 0;
		a.stats.inventory.ammunition.fill(0);
		a.stats.inventory.magazines.fill(0);
		a.yaw = 0;
		a.stats.inventory.weapon_kind = weapon;
		a.stats.inventory.pistol = true;
		a.stats.inventory.owned_weapons = 1 << weapon;
		a.stats.inventory.rounds = lastlight::weapon_spec(weapon).magazine;
		a.stats.inventory.reserve = id == 4 ? 20 : 12;
		a.stats.inventory.smoke = id == 1 || id == 5 ? 1 : 0;
		a.stats.inventory.bandages = id == 2 ? 2 : 1;
		a.stats.inventory.medkits = id == 2 ? 2 : 1;
		a.stats.inventory.tools = id == 5 ? 1 : 0;
		a.stats.inventory.armour = id == 4 ? 40 : 0;
		a.role = id == 2 ? 1 : id == 3 ? 2
				: id == 4			   ? 3
				: id == 5			   ? 4
									   : 0;
	}
	const Vector3 spawns[] = { { 165, .1, 112 }, { 93, .1, 85 }, { 199, .1, 153 }, { 235, .1, 176 }, { 180, .1, 84 }, { 210, .1, 112 }, { 51, .1, 184 }, { 146, .1, 92 } };
	for (int i = 0; i < 8; ++i) {
		create_actor(10 + i, spawns[i], false, Color(.55, .29, .18), i % 5);
		auto &a = actors.back();
		a.bot_role = i % 5;
		a.stats.inventory.weapon_kind = i % 3;
	}
	configure_bots(spawn_count);
	camera_yaw = 0;
	camera_pitch = -.26;
}
