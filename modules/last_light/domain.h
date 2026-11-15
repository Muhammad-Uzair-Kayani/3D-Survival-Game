#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
namespace lastlight {
enum class State { Idle,
	Walk,
	Run,
	Crouch,
	Airborne,
	Aim,
	Fire,
	Reload,
	Melee,
	Heal,
	Revive,
	Stunned,
	Downed,
	Dead,
	Extracted,
	Interact };
inline const char *state_name(State s) {
	static const char *names[] = { "Idle", "Walk", "Run", "Crouch", "Airborne", "Aim", "Fire", "Reload", "Melee", "Heal", "Revive", "Stunned", "Downed", "Dead", "Extracted", "Interact" };
	int n = int(s);
	return n >= 0 && n < 16 ? names[n] : "Invalid";
}
struct WeaponSpec {
	const char *name;
	int magazine, damage;
	float cooldown, reload, range, noise;
};
inline WeaponSpec weapon_spec(int kind) {
	const WeaponSpec specs[] = { { "Pistol", 6, 25, .35f, 1.7f, 30, 25 }, { "SMG", 24, 15, .11f, 2.2f, 24, 32 }, { "Rifle", 20, 32, .24f, 2.5f, 55, 45 }, { "Shotgun", 5, 52, .9f, 2.8f, 14, 38 } };
	return specs[std::clamp(kind, 0, 3)];
}
struct Inventory {
	bool pistol = false, sword = false;
	int rounds = 0, reserve = 12, medkits = 1, food = 1;
	int weapon_kind = 0, owned_weapons = 0;
	std::array<int, 4> magazines{}, ammunition{ { 12, 0, 0, 0 } };
	int bandages = 0, water = 1, fuel = 0, smoke = 0, decoys = 0, armour = 0, backpack = 0, tools = 0, coat = 0;
	float hydration = 100, cold = 0;
	float weight() const {
		float guns = 0;
		const float masses[] = { 1.5f, 2.5f, 3.5f, 3.2f };
		for (int k = 0; k < 4; ++k) {
			if (owned_weapons & (1 << k)) {
				guns += masses[k];
			}
			guns += (k == weapon_kind ? reserve : ammunition[k]) * .015f;
		}
		if (pistol && !owned_weapons) {
			guns += 2;
		}
		return guns + (sword ? 1.5f : 0) + medkits * .5f + food * .35f + water * .5f + fuel * 1.5f + smoke * .3f + decoys * .3f + bandages * .1f;
	}
	float capacity() const { return backpack ? 24.f : 14.f; }
};
inline bool switch_weapon(Inventory &i, int kind) {
	if (kind < 0 || kind > 3 || !(i.owned_weapons & (1 << kind))) {
		return false;
	}
	i.magazines[i.weapon_kind] = i.rounds;
	i.ammunition[i.weapon_kind] = i.reserve;
	i.weapon_kind = kind;
	i.rounds = i.magazines[kind];
	i.reserve = i.ammunition[kind];
	i.pistol = true;
	return true;
}

struct Stats {
	int id = 0, health = 100;
	bool survivor = true;
	State state = State::Idle;
	Inventory inventory;
	float stamina = 100, hunger = 100, timer = 0, cooldown = 0, bleed = 0, hunger_clock = 0;
	int pending_target = 0, damage_received = 0, shots = 0, kills = 0, revives = 0, pickups = 0;
	bool active() const { return health > 0 && state != State::Extracted && state != State::Dead && state != State::Downed; }
	bool busy() const { return state == State::Reload || state == State::Heal || state == State::Revive || state == State::Melee || state == State::Stunned || state == State::Interact; }
};
inline bool valid(const Stats &s) {
	auto &i = s.inventory;
	return s.id > 0 && s.health >= 0 && s.health <= 100 && int(s.state) >= 0 && int(s.state) < 16 && i.rounds >= 0 && i.rounds <= weapon_spec(i.weapon_kind).magazine && i.reserve >= 0 && i.reserve <= 10000 && i.medkits >= 0 && i.medkits <= 100 && i.food >= 0 && i.food <= 100 && std::isfinite(s.stamina) && s.stamina >= 0 && s.stamina <= 100 && std::isfinite(s.hunger) && s.hunger >= 0 && s.hunger <= 100 && std::isfinite(s.timer) && s.timer >= 0 && s.timer <= 120 && std::isfinite(s.cooldown) && s.cooldown >= 0 && s.cooldown <= 120 && std::isfinite(s.bleed) && s.bleed >= 0 && s.bleed <= 60 && std::isfinite(s.hunger_clock) && s.hunger_clock >= 0 && s.hunger_clock <= 10;
}
inline void cancel(Stats &s) {
	if (s.active() && s.busy()) {
		s.state = State::Idle;
		s.timer = 0;
		s.pending_target = 0;
	}
}
inline bool begin(Stats &s, State action, int target = 0) {
	if (!s.active() || s.busy()) {
		return false;
	}
	if (action == State::Reload && s.inventory.pistol && s.inventory.rounds < weapon_spec(s.inventory.weapon_kind).magazine && s.inventory.reserve > 0) {
		s.timer = weapon_spec(s.inventory.weapon_kind).reload;
	} else if (action == State::Heal && s.inventory.medkits > 0 && s.health < 100) {
		s.timer = 1.5f;
	} else if (action == State::Revive && s.inventory.medkits > 0 && target > 0 && target != s.id) {
		s.timer = 2;
	} else if (action == State::Melee && (s.inventory.sword || !s.inventory.pistol) && s.cooldown <= 0) {
		s.timer = .55f;
		s.cooldown = .8f;
	} else if (action == State::Interact) {
		s.timer = 1;
	} else {
		return false;
	}
	s.state = action;
	s.pending_target = target;
	return true;
}
inline bool fire(Stats &s) {
	if (!s.active() || s.busy() || !s.inventory.pistol || s.inventory.rounds < 1 || s.cooldown > 0) {
		return false;
	}
	--s.inventory.rounds;
	++s.shots;
	s.cooldown = weapon_spec(s.inventory.weapon_kind).cooldown;
	s.state = State::Fire;
	return true;
}
inline void damage(Stats &s, int amount) {
	if (!s.active() || amount <= 0) {
		return;
	}
	if (s.inventory.armour > 0) {
		int absorbed = std::min(s.inventory.armour, std::max(1, amount / 3));
		s.inventory.armour -= absorbed;
		amount -= absorbed;
	}
	s.health = std::max(0, s.health - amount);
	s.damage_received += amount;
	s.timer = 0;
	s.pending_target = 0;
	if (s.health == 0) {
		s.state = s.survivor ? State::Downed : State::Dead;
		s.bleed = s.survivor ? 18.f : 0.f;
	} else {
		s.state = State::Stunned;
		s.timer = .15f;
	}
}
inline bool revive(Stats &helper, Stats &victim) {
	if (helper.timer > 0 || !helper.active() || helper.inventory.medkits < 1 || victim.state != State::Downed || helper.pending_target != victim.id) {
		return false;
	}
	--helper.inventory.medkits;
	victim.health = 40;
	victim.state = State::Idle;
	victim.bleed = 0;
	++helper.revives;
	return true;
}
// Returns the completed action, allowing the spatial adapter to recheck revival range.
inline State tick(Stats &s, float dt) {
	if (!std::isfinite(dt) || dt <= 0 || dt > .25f) {
		return State::Idle;
	}
	s.cooldown = std::max(0.f, s.cooldown - dt);
	if (s.state == State::Downed) {
		s.bleed = std::max(0.f, s.bleed - dt);
		if (s.bleed == 0) {
			s.state = State::Dead;
		}
		return State::Idle;
	}
	if (!s.active()) {
		return State::Idle;
	}
	s.hunger_clock += dt;
	if (s.hunger_clock >= 10) {
		s.hunger_clock -= 10;
		s.hunger = std::max(0.f, s.hunger - 3);
		if (s.hunger == 0) {
			damage(s, 2);
		}
	}
	if (s.state != State::Run) {
		s.stamina = std::min(100.f, s.stamina + dt * 10);
	}
	if (s.timer > 0) {
		s.timer = std::max(0.f, s.timer - dt);
		if (s.timer == 0) {
			State completed = s.state;
			if (completed == State::Reload) {
				int n = std::min(weapon_spec(s.inventory.weapon_kind).magazine - s.inventory.rounds, s.inventory.reserve);
				s.inventory.rounds += n;
				s.inventory.reserve -= n;
			}
			if (completed == State::Heal && s.inventory.medkits > 0) {
				--s.inventory.medkits;
				s.health = std::min(100, s.health + 45);
			}
			s.state = State::Idle;
			return completed;
		}
	}
	return State::Idle;
}
struct Belief {
	float trust = 0, threat = 0;
	double last_seen = -100, last_attack = -100;
};
inline void witness(Belief &b, const std::string &event, double time) {
	b.last_seen = time;
	if (event == "help" || event == "offer") {
		b.trust = std::min(1.f, b.trust + .3f);
	}
	if (event == "attack") {
		b.threat = std::min(1.f, b.threat + .6f);
		b.last_attack = time;
	}
	if (event == "aim") {
		b.threat = std::min(1.f, b.threat + .1f);
	}
}
struct Blend {
	float idle = 1, walk = 0, run = 0, crouch = 0, aim = 0, action = 0, phase = 0, forward = 0, backward = 0, left = 0, right = 0;
	void update(float speed, float local_x, float local_z, bool crouching, bool aiming, bool acting, float dt) {
		float desired_run = std::clamp((speed - 3.f) / 2.f, 0.f, 1.f), desired_idle = speed < .08f ? 1.f : 0.f;
		float desired_crouch = crouching && desired_idle == 0 ? 1.f : 0.f;
		if (desired_crouch > 0) {
			desired_run = 0;
		}
		float desired_walk = std::max(0.f, 1 - desired_idle - desired_run - desired_crouch), alpha = std::min(1.f, dt * 10);
		idle += (desired_idle - idle) * alpha;
		walk += (desired_walk - walk) * alpha;
		run += (desired_run - run) * alpha;
		crouch += (desired_crouch - crouch) * alpha;
		float sum = idle + walk + run + crouch;
		idle /= sum;
		walk /= sum;
		run /= sum;
		crouch /= sum;
		aim += (float(aiming) - aim) * alpha;
		action += (float(acting) - action) * alpha;
		phase = std::fmod(phase + speed * dt / 2, 1.f);
		float direction = std::abs(local_x) + std::abs(local_z);
		forward = direction > 0 ? std::max(0.f, -local_z) / direction : 0;
		backward = direction > 0 ? std::max(0.f, local_z) / direction : 0;
		right = direction > 0 ? std::max(0.f, local_x) / direction : 0;
		left = direction > 0 ? std::max(0.f, -local_x) / direction : 0;
	}
};
} //namespace lastlight
