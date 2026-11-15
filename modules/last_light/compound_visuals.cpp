#include "last_light_game.h"
#include "scene/resources/material.h"
void LastLightGame::add_compound_gear(Actor &a) {
	if (!a.skeleton) {
		return;
	}
	auto attach = [&](const String &bone) -> Node3D * {int id=a.skeleton->find_bone(bone);if(id<0){return nullptr;
}auto socket=memnew(BoneAttachment3D);a.skeleton->add_child(socket);socket->set_bone_name(bone);auto root=memnew(Node3D);socket->add_child(root);Basis bone_world=a.skeleton->get_global_basis()*a.skeleton->get_bone_global_pose(id).basis;root->set_basis(bone_world.orthonormalized().inverse()*a.visual->get_global_basis().orthonormalized());return root; };
	auto chest = attach("spine_03");
	if (chest) {
		Color fabric = a.stats.survivor ? Color(.12, .2, .22) : Color(.23, .18, .13);
		make_box(chest, Vector3(0, -.07, 0), Vector3(.38, .33, .21), fabric);
		for (int side : { -1, 1 }) {
			make_box(chest, Vector3(side * .12, -.07, .13), Vector3(.09, .16, .05), fabric.lightened(.15));
			make_box(chest, Vector3(side * .13, .05, 0), Vector3(.055, .13, .22), Color(.1, .12, .13));
		}
		if (a.stats.id == 2) {
			make_box(chest, Vector3(0, .04, .135), Vector3(.08, .022, .012), Color(.9, .9, .85));
			make_box(chest, Vector3(0, .04, .14), Vector3(.022, .08, .012), Color(.9, .9, .85));
		}
		a.vest_model = make_box(chest, Vector3(0, -.065, .145), Vector3(.21, .23, .035), Color(.3, .34, .32));
		a.pack_model = make_box(chest, Vector3(0, -.06, -.2), Vector3(.31, .37, .2), Color(.2, .27, .24));
	}
	auto head = attach("Head");
	if (head) {
		make_box(head, Vector3(0, .12, 0), Vector3(.29, .13, .3), Color(.14, .19, .2));
		make_box(head, Vector3(0, .045, .145), Vector3(.25, .08, .035), Color(.1, .3, .35));
		for (int side : { -1, 1 }) {
			make_box(head, Vector3(side * .155, .045, 0), Vector3(.045, .105, .08), Color(.09, .12, .13));
		}
	}
}
void LastLightGame::update_equipment_pose(Actor &a) {
	if (a.vest_model) {
		a.vest_model->set_visible(a.stats.inventory.armour > 0);
	}
	if (a.pack_model) {
		a.pack_model->set_visible(a.stats.inventory.backpack > 0);
	}
	if (!a.skeleton || !a.aiming || (a.stats.busy() && a.stats.state != lastlight::State::Fire) || a.use_sword || a.stats.inventory.weapon_kind == 0 || !a.stats.active()) {
		return;
	}
	int weapon = a.stats.inventory.weapon_kind;
	if (size_t(weapon) >= a.gun_models.size() || !a.gun_models[weapon]) {
		return;
	}
	auto s = a.skeleton;
	int hand = s->find_bone("hand_l"), upper = s->find_bone("upperarm_l"), lower = s->find_bone("lowerarm_l");
	if (hand < 0 || upper < 0 || lower < 0) {
		return;
	}
	s->force_update_all_bone_transforms();
	Vector3 target = s->get_global_transform().affine_inverse().xform(a.gun_models[weapon]->get_global_transform().xform(Vector3(0, .015, weapon == 1 ? .19f : .29f)));
	for (int pass = 0; pass < 2; ++pass) {
		for (int joint : { lower, upper }) {
			Transform3D pose = s->get_bone_global_pose(joint);
			Vector3 tip = s->get_bone_global_pose(hand).origin;
			Vector3 from = tip - pose.origin, to = target - pose.origin;
			if (from.length() < .01 || to.length() < .01) {
				continue;
			}
			Quaternion correction(from.normalized(), to.normalized());
			correction = Quaternion().slerp(correction, .7f);
			int parent = s->get_bone_parent(joint);
			Basis basis = parent >= 0 ? s->get_bone_global_pose(parent).basis : Basis();
			Basis local = basis.inverse() * Basis(correction) * basis;
			s->set_bone_pose_rotation(joint, (local.get_rotation_quaternion() * s->get_bone_pose_rotation(joint)).normalized());
			s->force_update_all_bone_transforms();
		}
	}
}

Node3D *LastLightGame::make_compound_pickup(const String &kind, const Vector3 &position) {
	auto root = memnew(Node3D);
	level_root->add_child(root);
	root->set_position(position);
	int gun = kind == "pistol" ? 0 : kind == "smg" ? 1
			: kind == "rifle"					   ? 2
			: kind == "shotgun"					   ? 3
												   : -1;
	if (gun >= 0) {
		Array paths = assets.get("gun_scenes", Array());
		if (paths.size() > gun) {
			auto scene = load_asset_scene(paths[gun]);
			if (scene.is_valid()) {
				auto model = Object::cast_to<Node3D>(scene->instantiate());
				if (model) {
					root->add_child(model);
					model->set_rotation_degrees(Vector3(0, 90, 70));
					model->set_scale(Vector3(.65, .65, .65));
				}
			}
		}
	} else if (kind == "medkit" || kind == "bandage") {
		make_box(root, Vector3(), Vector3(.32, .18, .23), kind == "medkit" ? Color(.7, .16, .12) : Color(.68, .62, .5));
		make_box(root, Vector3(0, .095, 0), Vector3(.15, .012, .045), Color(.9, .9, .82));
		make_box(root, Vector3(0, .097, 0), Vector3(.045, .012, .15), Color(.9, .9, .82));
	} else if (kind == "water") {
		make_box(root, Vector3(), Vector3(.13, .3, .13), Color(.16, .45, .58));
		make_box(root, Vector3(0, .17, 0), Vector3(.085, .055, .085), Color(.75, .78, .74));
	} else if (kind == "key") {
		make_box(root, Vector3(), Vector3(.28, .025, .18), Color(.85, .58, .12));
		make_box(root, Vector3(0, .02, 0), Vector3(.2, .01, .025), Color(.12, .16, .18));
	} else {
		Color color = kind.contains("ammo") || kind == "shells" ? Color(.48, .39, .2) : kind == "food" ? Color(.58, .29, .12)
				: kind == "fuel"																	   ? Color(.52, .15, .1)
																									   : Color(.22, .32, .28);
		make_box(root, Vector3(), Vector3(.28, .17, .22), color);
		make_box(root, Vector3(0, .09, 0), Vector3(.28, .025, .04), color.lightened(.35));
	}
	auto label = memnew(Label3D);
	root->add_child(label);
	label->set_position(Vector3(0, .3, 0));
	label->set_text(kind.replace("_", " "));
	label->set_font_size(32);
	label->set_pixel_size(.002);
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	label->set_visibility_range_end(8);
	return root;
}
