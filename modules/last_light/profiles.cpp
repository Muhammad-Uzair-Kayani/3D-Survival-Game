#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "last_light_game.h"
#include <cmath>
namespace {
bool finite_number(const Variant &v, double lo, double hi) {
	return (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) && std::isfinite(double(v)) && double(v) >= lo && double(v) <= hi;
}
const char *trait_names[] = { "caution", "aggression", "generosity", "loyalty", "curiosity", "confidence" };
} //namespace
Dictionary LastLightGame::profile_info(const Actor &actor) const {
	Dictionary row;
	row["id"] = actor.stats.id;
	const auto &p = actor.personality;
	row["type"] = lastlight::personality_type(p);
	row["revision"] = p.revision;
	row["experiences"] = p.experiences;
	const float values[] = { p.caution, p.aggression, p.generosity, p.loyalty, p.curiosity, p.confidence };
	Dictionary traits;
	for (int i = 0; i < 6; ++i) {
		traits[trait_names[i]] = values[i] * 100;
	}
	row["traits"] = traits;
	row["help_received"] = p.helped;
	row["harm_received"] = p.harmed;
	row["help_given"] = p.rescued;
	row["extractions"] = p.extractions;
	row["deaths"] = p.deaths;
	Array preferences, risk;
	for (float value : p.preferences) {
		preferences.push_back(value);
	}
	for (float value : p.region_risk) {
		risk.push_back(value);
	}
	row["preferences"] = preferences;
	Array tactical;
	for (float value : actor.tactical_learning) {
		tactical.push_back(value);
	}
	row["tactical_biases"] = tactical;
	Array learned_rows;
	for (auto &[context, weights] : actor.contextual_values) {
		Dictionary entry;
		entry["context"] = context;
		Array q;
		for (float value : weights) {
			q.push_back(value);
		}
		entry["values"] = q;
		learned_rows.push_back(entry);
	}
	row["contextual_values"] = learned_rows;
	row["context"] = actor.tactical_context;
	row["learned_contexts"] = int(actor.contextual_values.size());
	const char *roles[] = { "Independent", "Medic", "Scout", "Guard", "Engineer" };
	row["role"] = roles[CLAMP(actor.role, 0, 4)];
	row["cover_id"] = actor.cover_id;
	row["protected"] = actor.cover_protected;
	row["peek_available"] = actor.peek_clear;
	row["tactic"] = lastlight::tactic_name(actor.tactic);
	row["task_claim"] = actor.task_claim;
	row["suppression"] = actor.suppression;
	row["order_reason"] = actor.order_reason;
	row["plan_complete"] = actor.plan.complete;
	row["expected_ammo"] = actor.plan.expected_ammo;
	row["expected_health"] = actor.plan.expected_health;
	Array choices;
	for (int k = 0; k < lastlight::TACTIC_COUNT; ++k) {
		Dictionary option;
		option["name"] = lastlight::tactic_name(lastlight::Tactic(k));
		option["legal"] = actor.tactical_decision.legal[k];
		option["score"] = actor.tactical_decision.score[k];
		option["reason"] = actor.tactical_decision.reason[k].c_str();
		choices.push_back(option);
	}
	row["tactics"] = choices;
	Array contacts;
	for (auto &[id, c] : actor.contacts) {
		if (c.confidence <= 0) {
			continue;
		}
		Dictionary evidence;
		evidence["id"] = id;
		evidence["age"] = MAX(0., elapsed - c.seen);
		evidence["confidence"] = c.confidence;
		evidence["visible"] = c.visible;
		evidence["hostile"] = c.hostile;
		contacts.push_back(evidence);
	}
	row["contacts"] = contacts;
	row["region_risk"] = risk;
	Array relationships;
	for (auto &[id, trust] : actor.persistent_trust) {
		Dictionary peer;
		peer["id"] = id;
		peer["trust"] = trust * 100;
		auto fear = actor.persistent_fear.find(id);
		peer["fear"] = fear == actor.persistent_fear.end() ? 0 : fear->second * 100;
		relationships.push_back(peer);
	}
	row["relationships"] = relationships;
	row["history"] = actor.experience_log;
	row["health"] = actor.stats.health;
	row["hunger"] = actor.stats.hunger;
	row["cold"] = actor.stats.inventory.cold;
	row["hydration"] = actor.stats.inventory.hydration;
	row["weapon"] = actor.stats.inventory.pistol ? String(lastlight::weapon_spec(actor.stats.inventory.weapon_kind).name) : String("Unarmed");
	row["rounds"] = actor.stats.inventory.rounds;
	row["reserve"] = actor.stats.inventory.reserve;
	row["teammate"] = actor.teammate_id;
	row["goal"] = String(actor.plan.label.c_str());
	row["reason"] = actor.reason;
	row["expanded"] = actor.plan.expanded;
	row["neural"] = python_ready;
	row["plan_step"] = int(actor.plan_step);
	Array steps;
	for (size_t i = actor.plan_step; i < actor.plan.steps.size() && i < actor.plan_step + 18; ++i) {
		steps.push_back(String(actor.plan.steps[i].label.c_str()));
	}
	row["steps"] = steps;
	Array alternatives;
	for (const auto &plan : actor.alternatives) {
		Dictionary candidate;
		candidate["goal"] = String(plan.label.c_str());
		candidate["score"] = plan.score;
		candidate["cost"] = plan.cost;
		candidate["risk"] = plan.risk;
		candidate["reason"] = String(plan.reason.c_str());
		alternatives.push_back(candidate);
	}
	row["alternatives"] = alternatives;
	return row;
}
bool LastLightGame::save_profiles() {
	if (!arctic || automated_run || fixed_profiles) {
		return true;
	}
	Dictionary file;
	file["version"] = 2;
	Array profiles;
	for (const auto &a : actors) {
		if (a.stats.id >= 2 && a.stats.id <= 5) {
			Dictionary row = profile_info(a);
			Array sessions;
			for (auto &[id, until] : a.profile_sessions) {
				Array session;
				session.push_back(id);
				session.push_back(until);
				sessions.push_back(session);
			}
			row["sessions"] = sessions;
			profiles.push_back(row);
		}
	}
	file["survivors"] = profiles;
	String path = "user://survivors-v2.json", temp = path + ".tmp";
	auto output = FileAccess::open(temp, FileAccess::WRITE);
	if (output.is_null()) {
		return false;
	}
	output->store_string(JSON::stringify(file, "", true, true));
	output->flush();
	output.unref();
	String actual = ProjectSettings::get_singleton()->globalize_path(path);
	String backup = actual + ".bak";
	DirAccess::remove_absolute(backup);
	if (FileAccess::exists(path) && DirAccess::rename_absolute(actual, backup) != OK) {
		return false;
	}
	if (DirAccess::rename_absolute(ProjectSettings::get_singleton()->globalize_path(temp), actual) != OK) {
		DirAccess::rename_absolute(backup, actual);
		return false;
	}
	DirAccess::remove_absolute(backup);
	return true;
}
void LastLightGame::load_profiles() {
	if (!arctic || automated_run || fixed_profiles) {
		return;
	}
	String profile_path = FileAccess::exists("user://survivors-v2.json") ? "user://survivors-v2.json" : "user://survivors-v1.json";
	auto input = FileAccess::open(profile_path, FileAccess::READ);
	if (input.is_null()) {
		return;
	}
	if (input->get_length() > 2 * 1024 * 1024) {
		notify("Survivor profiles exceeded the file limit; defaults loaded.");
		return;
	}
	Variant parsed = JSON::parse_string(input->get_as_text());
	if (parsed.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary file = parsed;
	if ((int(file.get("version", 0)) != 1 && int(file.get("version", 0)) != 2) || file.get("survivors", Variant()).get_type() != Variant::ARRAY) {
		return;
	}
	Array rows = file["survivors"];
	if (rows.size() != 4) {
		return;
	}
	auto candidate = actors;
	for (int i = 0; i < 4; ++i) {
		if (rows[i].get_type() != Variant::DICTIONARY) {
			return;
		}
		Dictionary row = rows[i];
		int id = int(row.get("id", 0));
		if (id != i + 2) {
			return;
		}
		auto &a = candidate[id - 1];
		auto &p = a.personality;
		if (row.get("traits", Variant()).get_type() != Variant::DICTIONARY) {
			return;
		}
		Dictionary traits = row["traits"];
		float *values[] = { &p.caution, &p.aggression, &p.generosity, &p.loyalty, &p.curiosity, &p.confidence };
		for (int k = 0; k < 6; ++k) {
			Variant value = traits.get(trait_names[k], Variant());
			if (!finite_number(value, 0, 100)) {
				return;
			}
			*values[k] = double(value) / 100;
		}
		const char *names[] = { "revision", "experiences", "help_received", "harm_received", "help_given", "extractions", "deaths" };
		int *counts[] = { &p.revision, &p.experiences, &p.helped, &p.harmed, &p.rescued, &p.extractions, &p.deaths };
		for (int k = 0; k < 7; ++k) {
			Variant value = row.get(names[k], Variant());
			if (!finite_number(value, 0, 10000000) || std::floor(double(value)) != double(value)) {
				return;
			}
			*counts[k] = int(value);
		}
		if (row.get("preferences", Variant()).get_type() != Variant::ARRAY || row.get("region_risk", Variant()).get_type() != Variant::ARRAY) {
			return;
		}
		Array preferences = row["preferences"], risk = row["region_risk"];
		if (preferences.size() != 6 || (risk.size() != 16 && risk.size() != 32)) {
			return;
		}
		for (int k = 0; k < 6; ++k) {
			if (!finite_number(preferences[k], -2, 2)) {
				return;
			}
			p.preferences[k] = preferences[k];
		}
		for (int k = 0; k < risk.size(); ++k) {
			if (!finite_number(risk[k], 0, 1)) {
				return;
			}
			p.region_risk[k] = risk[k];
		}
		for (const String field : { String("relationships"), String("history"), String("sessions") }) {
			if (row.get(field, Variant()).get_type() != Variant::ARRAY) {
				return;
			}
		}
		a.contextual_values.clear();
		if (row.has("contextual_values")) {
			if (row["contextual_values"].get_type() != Variant::ARRAY) {
				return;
			}
			Array entries = row["contextual_values"];
			if (entries.size() > 216) {
				return;
			}
			for (int j = 0; j < entries.size(); ++j) {
				if (entries[j].get_type() != Variant::DICTIONARY) {
					return;
				}
				Dictionary entry = entries[j];
				if (!finite_number(entry.get("context", Variant()), 0, 215) || std::floor(double(entry["context"])) != double(entry["context"]) || entry.get("values", Variant()).get_type() != Variant::ARRAY) {
					return;
				}
				int context = entry["context"];
				if (a.contextual_values.count(context)) {
					return;
				}
				Array q = entry["values"];
				if (q.size() != lastlight::TACTIC_COUNT) {
					return;
				}
				std::array<float, lastlight::TACTIC_COUNT> values{};
				for (int k = 0; k < q.size(); ++k) {
					if (!finite_number(q[k], -2, 2)) {
						return;
					}
					values[k] = q[k];
				}
				a.contextual_values[context] = values;
			}
		}
		a.persistent_trust.clear();
		a.persistent_fear.clear();
		a.profile_sessions.clear();
		a.memory.clear();
		if (row.has("tactical_biases")) {
			if (row["tactical_biases"].get_type() != Variant::ARRAY) {
				return;
			}
			Array biases = row["tactical_biases"];
			if (biases.size() != lastlight::TACTIC_COUNT) {
				return;
			}
			for (int k = 0; k < biases.size(); ++k) {
				if (!finite_number(biases[k], -2, 2)) {
					return;
				}
				a.tactical_learning[k] = biases[k];
			}
		}
		Array peers = row.get("relationships", Array());
		if (peers.size() > 16) {
			return;
		}
		for (int k = 0; k < peers.size(); ++k) {
			if (peers[k].get_type() != Variant::DICTIONARY) {
				return;
			}
			Dictionary peer = peers[k];
			int other = peer.get("id", 0);
			if (!find_actor(other) || !finite_number(peer.get("trust", Variant()), 0, 100) || !finite_number(peer.get("fear", Variant()), 0, 100)) {
				return;
			}
			a.persistent_trust[other] = double(peer["trust"]) / 100;
			a.persistent_fear[other] = double(peer["fear"]) / 100;
		}
		Array history = row.get("history", Array());
		if (history.size() > 48) {
			return;
		}
		for (int k = 0; k < history.size(); ++k) {
			if (history[k].get_type() != Variant::DICTIONARY) {
				return;
			}
		}
		a.experience_log = history;
		Array sessions = row.get("sessions", Array());
		if (sessions.size() > 64) {
			return;
		}
		for (int k = 0; k < sessions.size(); ++k) {
			if (sessions[k].get_type() != Variant::ARRAY) {
				return;
			}
			Array session = sessions[k];
			if (session.size() != 2 || !finite_number(session[0], -2147483647, 2147483647) || !finite_number(session[1], 0, 10000000)) {
				return;
			}
			a.profile_sessions[int(session[0])] = session[1];
		}
		for (auto &[other, trust] : a.persistent_trust) {
			a.memory[other].trust = trust * .7f;
			a.memory[other].threat = a.persistent_fear[other] * .35f;
		}
	}
	actors = candidate;
}
bool LastLightGame::reset_profile(int id) {
	if (fixed_profiles) {
		return false;
	}
	auto a = find_actor(id);
	if (!a || id < 2 || id > 5) {
		return false;
	}
	int revision = a->personality.revision + 1;
	a->personality = lastlight::baseline_personality(id);
	a->personality.revision = revision;
	a->contextual_values.clear();
	a->learned_context = -1;
	a->failed_jobs.clear();
	for (auto it = mission_claims.begin(); it != mission_claims.end();) {
		if (it->second.owner == id) {
			it = mission_claims.erase(it);
		} else {
			++it;
		}
	}
	a->tactical_learning.fill(0);
	a->contacts.clear();
	a->known_caches.clear();
	a->cover_id = -1;
	a->order = 0;
	a->task_claim = 0;
	a->persistent_trust.clear();
	a->persistent_fear.clear();
	a->memory.clear();
	a->teammate_id = 0;
	for (auto &other : actors) {
		if (other.teammate_id == id) {
			other.teammate_id = 0;
			other.next_plan = 0;
		}
	}
	a->known_items.clear();
	a->known_item_kinds.clear();
	a->profile_sessions.clear();
	a->experience_log.clear();
	a->plan.steps.clear();
	a->alternatives.clear();
	a->next_plan = 0;
	a->reposition_until = 0;
	a->last_learning = elapsed;
	a->known_empty_crates.clear();
	a->heard_until = 0;
	a->previous_health = a->stats.health;
	a->previous_pickups = a->stats.pickups;
	a->previous_mission = mission;
	bool saved = save_profiles();
	notify(saved ? "Survivor learning and social memory reset. Inventory and health preserved." : "Reset applied in this session, but the profile file could not be written.");
	return saved;
}
void LastLightGame::record_experience(Actor &a, const String &kind, int peer, float reward) {
	if (fixed_profiles || !arctic || a.stats.id < 2 || a.stats.id > 5 || controller == "nomemory") {
		return;
	}
	int key = int(session_id.hash() % 1000000000) + 1;
	int intent = 5;
	const char *labels[] = { "Explore", "Acquire", "Cooperate", "Avoid", "Defend", "Progress" };
	for (int i = 0; i < 6; ++i) {
		if (a.intent == labels[i]) {
			intent = i;
		}
	}
	// Event sequence is monotonic within a session. Rewinding a mission save
	// cannot teach the same event again or resurrect a reset profile.
	if (kind != "progress" && a.profile_sessions[-key] >= event_sequence) {
		return;
	}
	if (kind != "progress") {
		a.profile_sessions[-key] = event_sequence;
	}
	lastlight::experience(a.personality, kind.utf8().get_data(), nearest_region(a.body->get_position()), reward, intent);
	if (peer > 0 && find_actor(peer)) {
		float &trust = a.persistent_trust[peer], &fear = a.persistent_fear[peer];
		if (kind == "help_witnessed") {
			trust = MIN(1.f, trust + .04f);
			fear = MAX(0.f, fear - .01f);
		}
		if (kind == "aggression_witnessed") {
			fear = MIN(1.f, fear + .04f);
			trust = MAX(0.f, trust - .03f);
		}
		if (kind == "help_received") {
			trust = MIN(1.f, trust + .12f);
			fear = MAX(0.f, fear - .05f);
		}
		if (kind == "attacked") {
			fear = MIN(1.f, fear + .15f);
			trust = MAX(0.f, trust - .12f);
		}
	}
	if (kind != "progress") {
		Dictionary event;
		event["kind"] = kind;
		event["peer"] = peer;
		event["time"] = elapsed;
		event["mission"] = session_id;
		event["reward"] = reward;
		a.experience_log.push_back(event);
		while (a.experience_log.size() > 48) {
			a.experience_log.pop_front();
		}
	}
}
void LastLightGame::learn_tick(Actor &a) {
	if (fixed_profiles || !arctic || a.stats.id < 2 || a.stats.id > 5 || elapsed - a.last_learning < 4) {
		return;
	}
	int key = int(session_id.hash() % 1000000000) + 1;
	if (elapsed > a.profile_sessions[key] + .01) {
		float reward = (a.stats.health - a.previous_health) * .015f + (a.stats.pickups - a.previous_pickups) * .1f + (mission - a.previous_mission) * .5f - .015f;
		record_experience(a, "progress", 0, reward);
		if (compound_world && controller != "nomemory") {
			float next = 0;
			auto found = a.contextual_values.find(a.tactical_context);
			if (found != a.contextual_values.end()) {
				for (int k = 0; k < lastlight::TACTIC_COUNT; ++k) {
					if (a.tactical_decision.legal[k]) {
						next = MAX(next, found->second[k]);
					}
				}
			}
			if (a.learned_context >= 0) {
				lastlight::learn_tactical_value(a.contextual_values[a.learned_context], a.learned_action, reward - a.suppression * .08f, next);
			}
			a.learned_context = a.tactical_context;
			a.learned_action = int(a.tactic);
		}
		a.profile_sessions[key] = elapsed;
		while (a.profile_sessions.size() > 60) {
			a.profile_sessions.erase(a.profile_sessions.begin());
		}
	}
	a.previous_health = a.stats.health;
	a.previous_pickups = a.stats.pickups;
	a.previous_mission = mission;
	a.last_learning = elapsed;
}
