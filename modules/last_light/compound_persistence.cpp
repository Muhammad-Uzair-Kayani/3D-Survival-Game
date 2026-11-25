#include "last_light_game.h"
#include <cmath>
namespace {
Array vec(const Vector3 &p) {
	Array a;
	a.push_back(p.x);
	a.push_back(p.y);
	a.push_back(p.z);
	return a;
}
bool finite(const Variant &v, double lo, double hi) {
	return (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) && std::isfinite(double(v)) && double(v) >= lo && double(v) <= hi;
}
bool read_vec(const Variant &v, Vector3 &p) {
	if (v.get_type() != Variant::ARRAY) {
		return false;
	}
	Array a = v;
	if (a.size() != 3) {
		return false;
	}
	for (int i = 0; i < 3; ++i) {
		if (!finite(a[i], -1000, 1000)) {
			return false;
		}
	}
	p = Vector3(double(a[0]), double(a[1]), double(a[2]));
	return true;
}
} //namespace
void LastLightGame::snapshot_compound(Dictionary &data) const {
	Array sites;
	for (int i = 0; i < 2; ++i) {
		Dictionary site;
		site["active"] = beacon_active[i];
		site["arrival"] = beacon_times[i];
		site["hold"] = beacon_hold[i];
		site["contested"] = beacon_contested[i];
		sites.push_back(site);
	}
	data["beacons"] = sites;
	data["wave_stage"] = wave_stage;
	data["next_wave"] = next_wave;
	Array smoke;
	for (auto &e : effects) {
		if (e.kind != "smoke" && e.kind != "decoy") {
			continue;
		}
		Dictionary row;
		row["kind"] = e.kind;
		row["position"] = vec(e.position);
		row["until"] = e.until;
		row["owner"] = e.owner;
		smoke.push_back(row);
	}
	data["effects"] = smoke;
	Array states = data["actors"];
	for (int i = 0; i < states.size(); ++i) {
		Dictionary state = states[i];
		auto &a = actors[i];
		Dictionary detail;
		detail["next_investigate"] = a.next_investigate;
		detail["suppression"] = a.suppression;
		detail["damage_at"] = a.last_damage_at;
		detail["noise_at"] = a.last_noise_at;
		detail["noise_position"] = vec(a.last_noise);
		detail["noise_danger"] = a.noise_danger;
		detail["react_at"] = a.react_at;
		detail["order"] = a.order;
		detail["order_peer"] = a.order_peer;
		detail["order_until"] = a.order_until;
		detail["order_position"] = vec(a.order_position);
		detail["order_reason"] = a.order_reason;
		Array contacts;
		for (auto &[id, c] : a.contacts) {
			if (c.confidence <= 0) {
				continue;
			}
			Dictionary row;
			row["id"] = id;
			row["position"] = vec(c.position);
			row["velocity"] = vec(c.velocity);
			row["seen"] = c.seen;
			row["confidence"] = c.confidence;
			row["hostile"] = c.hostile;
			row["health_band"] = c.health_band;
			contacts.push_back(row);
		}
		detail["contacts"] = contacts;
		Array caches, empty;
		for (int id : a.known_caches) {
			caches.push_back(id);
		}
		for (int id : a.known_empty_crates) {
			empty.push_back(id);
		}
		detail["caches"] = caches;
		detail["empty_caches"] = empty;
		state["tactical_state"] = detail;
		states[i] = state;
	}
	data["actors"] = states;
}
bool LastLightGame::validate_compound_save(const Dictionary &data, std::vector<Actor> &candidate, double time) const {
	if (data.get("beacons", Variant()).get_type() != Variant::ARRAY || data.get("effects", Variant()).get_type() != Variant::ARRAY || !finite(data.get("wave_stage", Variant()), 0, 2) || std::floor(double(data["wave_stage"])) != double(data["wave_stage"]) || !finite(data.get("next_wave", Variant()), 0, 36060)) {
		return false;
	}
	Array sites = data["beacons"];
	if (sites.size() != 2) {
		return false;
	}
	for (int i = 0; i < 2; ++i) {
		if (sites[i].get_type() != Variant::DICTIONARY) {
			return false;
		}
		Dictionary site = sites[i];
		if (site.get("active", Variant()).get_type() != Variant::BOOL || site.get("contested", Variant()).get_type() != Variant::BOOL || !finite(site.get("arrival", Variant()), -1, 36060) || !finite(site.get("hold", Variant()), 0, 36000)) {
			return false;
		}
		if (bool(site["active"]) && (!bool(data["radio"]) || double(site["arrival"]) < 0)) {
			return false;
		}
	}
	Array effects = data["effects"];
	if (effects.size() > 64) {
		return false;
	}
	for (int i = 0; i < effects.size(); ++i) {
		if (effects[i].get_type() != Variant::DICTIONARY) {
			return false;
		}
		Dictionary e = effects[i];
		String kind = e.get("kind", "");
		Vector3 p;
		if ((kind != "smoke" && kind != "decoy") || !read_vec(e.get("position", Variant()), p) || (!finite(e.get("owner", Variant()), 1, 17) || std::floor(double(e["owner"])) != double(e["owner"]) || !find_actor(int(e["owner"]))) || !finite(e.get("until", Variant()), time, time + 13)) {
			return false;
		}
	}
	Array states = data["actors"];
	for (int n = 0; n < states.size(); ++n) {
		Dictionary state = states[n];
		if (state.get("tactical_state", Variant()).get_type() != Variant::DICTIONARY) {
			return false;
		}
		Dictionary d = state["tactical_state"];
		auto &a = candidate[n];
		for (const char *field : { "suppression", "damage_at", "noise_at", "react_at", "order", "order_peer", "order_until", "next_investigate" }) {
			if (!finite(d.get(field, Variant()), -100, 36060)) {
				return false;
			}
		}
		if (double(d["suppression"]) < 0 || double(d["suppression"]) > 1 || double(d["damage_at"]) > time || double(d["noise_at"]) > time || int(d["order"]) < 0 || int(d["order"]) > 4 || double(d["order"]) != int(d["order"]) || int(d["order_peer"]) < 0 || int(d["order_peer"]) > 5 || d.get("noise_danger", Variant()).get_type() != Variant::BOOL || d.get("order_reason", Variant()).get_type() != Variant::STRING || String(d["order_reason"]).length() > 256) {
			return false;
		}
		if (!read_vec(d.get("noise_position", Variant()), a.last_noise) || !read_vec(d.get("order_position", Variant()), a.order_position)) {
			return false;
		}
		a.suppression = d["suppression"];
		a.last_damage_at = d["damage_at"];
		a.recent_attack_at = -100;
		a.recent_attacker = 0;
		a.retreat_clear = false;
		a.next_burst_at = 0;
		a.last_noise_at = d["noise_at"];
		a.react_at = d["react_at"];
		a.noise_danger = d["noise_danger"];
		a.next_investigate = d["next_investigate"];
		a.order = d["order"];
		a.order_peer = d["order_peer"];
		a.order_until = d["order_until"];
		a.order_reason = d["order_reason"];
		a.contacts.clear();
		a.known_caches.clear();
		a.known_empty_crates.clear();
		a.failed_jobs.clear();
		a.cover_id = -1;
		a.cover_manual = false;
		a.task_claim = 0;
		a.next_decision = 0;
		a.next_tactical_scan = 0;
		a.tactical_until = 0;
		a.learned_context = -1;
		a.last_learning = time;
		a.previous_health = a.stats.health;
		a.previous_pickups = a.stats.pickups;
		a.previous_mission = data["mission"];
		a.heard_until = 0;
		a.outcome_recorded = a.stats.state == lastlight::State::Dead || a.stats.state == lastlight::State::Extracted;
		if (d.get("contacts", Variant()).get_type() != Variant::ARRAY) {
			return false;
		}
		Array contacts = d["contacts"];
		if (contacts.size() > 16) {
			return false;
		}
		for (int k = 0; k < contacts.size(); ++k) {
			if (contacts[k].get_type() != Variant::DICTIONARY) {
				return false;
			}
			Dictionary row = contacts[k];
			int id = row.get("id", 0);
			Contact c;
			if (!finite(row.get("id", Variant()), 1, 17) || std::floor(double(row["id"])) != double(row["id"]) || !find_actor(id) || a.contacts.count(id) || id == a.stats.id || !read_vec(row.get("position", Variant()), c.position) || !read_vec(row.get("velocity", Variant()), c.velocity) || !finite(row.get("seen", Variant()), -100, time) || !finite(row.get("confidence", Variant()), 0, 1) || !finite(row.get("health_band", Variant()), 0, 100) || row.get("hostile", Variant()).get_type() != Variant::BOOL) {
				return false;
			}
			c.seen = row["seen"];
			c.confidence = row["confidence"];
			c.health_band = row["health_band"];
			c.hostile = row["hostile"];
			c.visible = false;
			a.contacts[id] = c;
		}
		for (const char *field : { "caches", "empty_caches" }) {
			if (d.get(field, Variant()).get_type() != Variant::ARRAY) {
				return false;
			}
			Array known = d[field];
			if (known.size() > 64) {
				return false;
			}
			for (int k = 0; k < known.size(); ++k) {
				if (!finite(known[k], 1, 100000)) {
					return false;
				}
				int id = known[k];
				bool found = false;
				for (auto &o : world_objects) {
					if (o.id == id && o.kind == "crate") {
						found = true;
					}
				}
				if (!found) {
					return false;
				}
				if (String(field) == "caches") {
					a.known_caches.insert(id);
				} else {
					a.known_empty_crates.insert(id);
				}
			}
		}
		if (int(state.get("profile_revision", -1)) != a.personality.revision) {
			a.contacts.clear();
			a.order = 0;
			a.order_until = 0;
			a.known_caches.clear();
			a.known_empty_crates.clear();
		}
	}
	return true;
}
void LastLightGame::restore_compound(const Dictionary &data) {
	mission_claims.clear();
	for (auto &p : cover_points) {
		p.owner = 0;
		p.reserved_until = 0;
	}
	Array sites = data["beacons"];
	for (int i = 0; i < 2; ++i) {
		Dictionary site = sites[i];
		beacon_active[i] = site["active"];
		beacon_times[i] = site["arrival"];
		beacon_hold[i] = site["hold"];
		beacon_contested[i] = site["contested"];
	}
	wave_stage = data["wave_stage"];
	next_wave = data["next_wave"];
	for (auto &e : effects) {
		if (e.visual) {
			e.visual->queue_free();
		}
	}
	effects.clear();
	Array clouds = data["effects"];
	for (int i = 0; i < clouds.size(); ++i) {
		Dictionary e = clouds[i];
		Vector3 p;
		read_vec(e["position"], p);
		spawn_gameplay_effect(e["kind"], p, e["owner"], e["until"]);
	}
}
