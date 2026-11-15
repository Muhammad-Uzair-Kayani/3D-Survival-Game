#pragma once
#include "domain.h"
#include <array>
#include <vector>

namespace lastlight {
// Physics/rendering adapters supply these values. This header has no Godot API.
struct Point {
	float x = 0, z = 0;
};
inline float distance(Point a, Point b) {
	return std::hypot(a.x - b.x, a.z - b.z);
}
struct Contact {
	int id = 0;
	bool survivor = false, visible = false;
	Belief belief;
};
struct ObservationInput {
	Stats self;
	Point position, destination, control;
	std::vector<Contact> contacts;
	bool visible_loot = false, heard_noise = false, gate_open = false, key_found = false;
	double elapsed = 0;
};
struct Observation {
	std::array<float, 16> features{};
	std::array<int, 6> mask{};
};
struct WorldPort {
	virtual ~WorldPort() = default;
	virtual ObservationInput observe(int actor) const = 0;
};
inline Observation observe(const WorldPort &world, int actor) {
	auto in = world.observe(actor);
	float threat = 0, trust = 0;
	bool partner = false;
	unsigned seen = 0;
	for (const auto &peer : in.contacts) {
		if (peer.visible) {
			threat = std::max(threat, peer.belief.threat);
			trust = std::max(trust, peer.belief.trust);
			++seen;
		}
		if (peer.id != actor && peer.survivor && (peer.visible || in.elapsed - peer.belief.last_seen < 12) && peer.belief.threat < .6f) {
			partner = true;
		}
	}
	const bool visible_threat = threat > .25f;
	if (in.heard_noise) {
		threat = std::max(threat, .3f);
	}
	Observation out;
	const auto &s = in.self;
	out.features = { s.health / 100.f, s.inventory.rounds / 6.f, float(s.inventory.pistol || s.inventory.sword), float(in.visible_loot), threat, trust,
		1 - distance(in.position, in.destination) / 60, (in.destination.x - in.position.x + 42) / 84, (in.destination.z - in.position.z + 34) / 68, float(in.gate_open),
		1 - distance(in.position, in.control) / 60, seen / 5.f, float(in.elapsed / 600), s.pickups / 5.f, float(!in.contacts.empty() || in.heard_noise), float(in.key_found) };
	for (auto &value : out.features) {
		value = std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0;
	}
	out.mask = { 1, int(in.visible_loot), int(!in.gate_open && partner), int(threat > .25f), int(visible_threat && ((s.inventory.pistol && s.inventory.rounds > 0) || s.inventory.sword)), 1 };
	return out;
}
struct GateState {
	float held = 0, lift = 0;
	bool opening = false, open = false;
};
inline bool advance_gate(GateState &gate, int first, int second, float dt) {
	if (gate.open || !std::isfinite(dt) || dt <= 0 || dt > .25f) {
		return false;
	}
	gate.held = first > 0 && second > 0 && first != second ? std::min(2.f, gate.held + dt) : 0;
	if (gate.held >= 1) {
		gate.opening = true;
	}
	if (gate.opening) {
		gate.lift = std::min(3.f, gate.lift + 5 * dt);
	}
	if (gate.lift >= 3) {
		gate.open = true;
		gate.opening = false;
		return true;
	}
	return false;
}
inline double fitness(int arrived, int alive, double seconds, int harmful_attacks) {
	return arrived * 1000 + alive * 50 - seconds - 100 * harmful_attacks;
}
} // namespace lastlight
