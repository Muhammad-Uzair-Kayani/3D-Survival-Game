#include "last_light_game.h"
#include "core/crypto/crypto_core.h"
#include "core/io/json.h"

using namespace lastlight;

bool LastLightGame::load_native_policy(const String &path) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null() || file->get_length() != NeuralPolicy::file_size) {
		return false;
	}
	const auto bytes = file->get_buffer(NeuralPolicy::file_size);
	NeuralPolicy candidate;
	unsigned char digest[32];
	if (!candidate.load(bytes.ptr(), bytes.size()) ||
			CryptoCore::sha256(bytes.ptr(), bytes.size(), digest) != OK) {
		return false;
	}
	neural_policy = candidate;
	neural_checkpoint = String::hex_encode_buffer(digest, sizeof(digest));
	return true;
}

bool LastLightGame::neural_choices_enabled(const Actor &a) const {
	return a.stats.survivor && (controller == "native" || neural_trace.is_valid());
}

NeuralFeatures LastLightGame::neural_context(const Actor &a) const {
	NeuralFeatures x{};
	const auto &s = a.stats;
	const auto &i = s.inventory;
	const auto &p = a.personality;
	x.set(Feature::health, s.health, 100);
	x.set(Feature::stamina, s.stamina, 100);
	x.set(Feature::hunger, s.hunger, 100);
	x.set(Feature::hydration, i.hydration, 100);
	x.set(Feature::cold, i.cold, 100);
	x.set(Feature::armour, i.armour, 100);
	x.set(Feature::loaded, i.rounds, 30);
	x.set(Feature::reserve, i.reserve, 60);
	x.set(Feature::magazine, weapon_spec(i.weapon_kind).magazine, 30);
	x.set(Feature::weapon_range, weapon_spec(i.weapon_kind).range, 60);
	x.set(Feature::weapon_kind, i.pistol ? i.weapon_kind + 1 : 0, 4);
	x.set(Feature::owned_weapons, i.owned_weapons, 15);
	x.set(Feature::medkits, i.medkits, 3);
	x.set(Feature::bandages, i.bandages, 3);
	x.set(Feature::food, i.food, 3);
	x.set(Feature::water, i.water, 3);
	x.set(Feature::fuel, i.fuel, 3);
	x.set(Feature::smoke, i.smoke, 3);
	x.set(Feature::decoys, i.decoys, 3);
	x.set(Feature::tools, i.tools);
	x.set(Feature::coat, i.coat);
	x.set(Feature::backpack, i.backpack);
	x.set(Feature::weight, i.weight(), 24);
	x.set(Feature::capacity, i.capacity(), 24);
	x.set(Feature::caution, p.caution);
	x.set(Feature::confidence, p.confidence);
	x.set(Feature::aggression, p.aggression);
	x.set(Feature::loyalty, p.loyalty);
	x.set(Feature::generosity, p.generosity);
	x.set(Feature::curiosity, p.curiosity);
	x.set(Feature::role, a.role, 4);
	x.set(Feature::order, a.order, 4);
	x.set(Feature::order_time, a.order ? MAX(0., a.order_until - elapsed) : 0., 45);
	x.set(Feature::key, key_found);
	x.set(Feature::power, power_on);
	int relays = 0;
	for (const auto &o : world_objects) {
		relays += o.kind == "relay" && o.used;
	}
	x.set(Feature::relays, relays, 3);
	x.set(Feature::radio, radio_done);
	x.set(Feature::shortcut, gate_open);
	x.set(Feature::time_left, MAX(0., double(config.get("deadline_seconds", 600)) - elapsed), 600);
	x.set(Feature::north_active, beacon_active[0]);
	x.set(Feature::north_time, beacon_active[0] ? MAX(0., beacon_times[0] - elapsed) : 0., 32);
	x.set(Feature::west_active, beacon_active[1]);
	x.set(Feature::west_time, beacon_active[1] ? MAX(0., beacon_times[1] - elapsed) : 0., 32);
	x.set(Feature::known_items, a.known_items.size(), 30);
	x.set(Feature::known_caches, a.known_caches.size(), 12);
	x.set(Feature::known_contacts, a.contacts.size(), 12);
	x.set(Feature::suppression, a.suppression);
	return x;
}

void LastLightGame::neural_target(NeuralFeatures &x, const Actor &a, const Vector3 &target) const {
	const Vector3 relative = target - a.body->get_position();
	x.set(Feature::target_present, 1);
	x.set(Feature::target_dx, relative.x, 80);
	x.set(Feature::target_dy, relative.y, 12);
	x.set(Feature::target_dz, relative.z, 80);
	x.set(Feature::target_distance, relative.length(), 80);
}

void LastLightGame::neural_item(NeuralFeatures &x, const String &kind) const {
	static const char *const kinds[] = { "pistol", "smg", "rifle", "shotgun", "ammo", "smg_ammo", "rifle_ammo", "shells", "medkit", "bandage", "food", "water", "fuel", "smoke", "armour", "coat", "backpack", "tools" };
	static_assert(std::size(kinds) == int(Feature::i_tools) - int(Feature::i_pistol) + 1);
	for (size_t n = 0; n < std::size(kinds); ++n) {
		if (kind == kinds[n]) {
			x.set(Feature(int(Feature::i_pistol) + n), 1);
			break;
		}
	}
}

int LastLightGame::neural_choose(const Actor &a, std::vector<NeuralChoice> &choices, int teacher, int stage) {
	if (choices.empty() || teacher < 0 || size_t(teacher) >= choices.size()) {
		++neural_fallbacks[stage];
		return teacher;
	}
	int choice = teacher;
	float probability = 1.f;
	const float teacher_score = choices[teacher].teacher;
	for (auto &row : choices) {
		row.features.set(Feature::teacher_prior, row.teacher - teacher_score);
	}
	if (controller == "native" && neural_policy.ready()) {
		++neural_decisions[stage];
		if (choices.size() > 1) {
			std::vector<float> logits;
			logits.reserve(choices.size());
			for (const auto &row : choices) {
				logits.push_back(neural_policy.score(row.features));
			}
			choice = std::max_element(logits.begin(), logits.end()) - logits.begin();
			if (neural_temperature > 0) {
				const float high = logits[choice];
				double total = 0;
				for (float &logit : logits) {
					logit = std::exp((logit - high) / neural_temperature);
					total += logit;
				}
				std::discrete_distribution<int> sample(logits.begin(), logits.end());
				choice = sample(neural_rng);
				probability = logits[choice] / total;
			}
		}
	}
	if (neural_trace.is_valid() && !neural_trace_failed) {
		Dictionary record;
		record["type"] = "decision";
		record["stage"] = stage;
		record["tick"] = physics_frames;
		record["actor"] = a.stats.id;
		record["teacher"] = teacher;
		record["choice"] = choice;
		record["probability"] = probability;
		record["temperature"] = neural_temperature;
		Array rows;
		Array ids;
		for (const auto &row : choices) {
			Array values;
			values.resize(FEATURE_COUNT);
			for (int n = 0; n < FEATURE_COUNT; ++n) {
				values[n] = row.features[n];
			}
			rows.push_back(values);
			Array id;
			id.push_back(row.kind);
			id.push_back(row.target);
			ids.push_back(id);
		}
		record["features"] = rows;
		record["ids"] = ids;
		neural_trace_failed = !neural_trace->store_line(JSON::stringify(record));
	}
	return choice;
}

void LastLightGame::neural_outcome() {
	if (neural_trace.is_null() || neural_trace_failed) {
		return;
	}
	Array states;
	for (const auto &actor : actors) {
		if (actor.stats.survivor) {
			Array row;
			row.push_back(actor.stats.id);
			row.push_back(actor.stats.health);
			row.push_back(int(actor.stats.state));
			row.push_back(actor.stats.revives);
			row.push_back(actor.stats.damage_received);
			states.push_back(row);
		}
	}
	int relays = 0;
	for (const auto &o : world_objects) {
		relays += o.kind == "relay" && o.used;
	}
	Dictionary record;
	record["type"] = "outcome";
	record["tick"] = physics_frames;
	record["elapsed"] = elapsed;
	record["progress"] = int(key_found) + int(power_on) + MIN(relays, 2) + int(radio_done);
	record["actors"] = states;
	neural_trace_failed = !neural_trace->store_line(JSON::stringify(record));
}

void LastLightGame::neural_status(Dictionary &data) const {
	data["controller"] = controller;
	data["checkpoint"] = neural_checkpoint;
	data["fixed_profiles"] = fixed_profiles;
	data["neural_trace_failed"] = neural_trace_failed;
	Array decisions, fallbacks;
	for (int n = 0; n < 3; ++n) {
		decisions.push_back(neural_decisions[n]);
		fallbacks.push_back(neural_fallbacks[n]);
	}
	data["neural_decisions"] = decisions;
	data["neural_fallbacks"] = fallbacks;
}
