#include "../modules/last_light/domain.h"
#include <iostream>
#include <stdexcept>
using namespace lastlight;
void check(bool b, const char *text) {
	if (!b) {
		throw std::runtime_error(text);
	}
}
int main() {
	try {
		Stats s;
		s.id = 1;
		s.inventory.pistol = true;
		s.inventory.rounds = 2;
		check(valid(s), "initial state");
		check(fire(s) && s.inventory.rounds == 1, "ammo consumed");
		check(!fire(s), "cooldown enforced");
		for (int i = 0; i < 30; ++i) {
			tick(s, 1.f / 60);
		}
		check(begin(s, State::Reload), "reload begins");
		cancel(s);
		check(s.inventory.rounds == 1 && s.inventory.reserve == 12, "cancel does not consume ammo");
		check(begin(s, State::Reload), "reload restart");
		for (int i = 0; i < 110; ++i) {
			tick(s, 1.f / 60);
		}
		check(s.inventory.rounds == 6 && s.inventory.reserve == 7, "reload commits once");
		damage(s, 60);
		for (int i = 0; i < 10; ++i) {
			tick(s, 1.f / 60);
		}
		check(begin(s, State::Heal), "heal begins");
		for (int i = 0; i < 100; ++i) {
			tick(s, 1.f / 60);
		}
		check(s.health == 85 && s.inventory.medkits == 0, "heal consumes one kit");
		Stats victim;
		victim.id = 2;
		damage(victim, 100);
		check(victim.state == State::Downed, "downed state");
		s.inventory.medkits = 1;
		check(begin(s, State::Revive, 2), "revive begins");
		check(!revive(s, victim), "revive waits for timer");
		for (int i = 0; i < 130; ++i) {
			tick(s, 1.f / 60);
		}
		check(revive(s, victim) && victim.health == 40, "revive commits");
		check(!revive(s, victim) && s.inventory.medkits == 0, "duplicate revive rejected");
		damage(victim, 100);
		for (int i = 0; i < 1200; ++i) {
			tick(victim, 1.f / 60);
		}
		check(victim.state == State::Dead, "bleed out");
		damage(s, 1000);
		check(!fire(s), "dead/downed cannot shoot");
		Blend b;
		for (int i = 0; i < 600; ++i) {
			b.update(5, 0, -5, false, true, false, 1.f / 60);
			check(std::abs(b.idle + b.walk + b.run + b.crouch - 1) < .0001, "normalized weights");
		}
		check(b.run > .99 && b.forward == 1, "run and direction blend");
		Belief belief;
		witness(belief, "help", 1);
		check(belief.trust > .2f && belief.threat == 0, "help evidence");
		witness(belief, "attack", 2);
		check(belief.threat > .5f && belief.last_attack == 2, "attack evidence");
		s.inventory.reserve = -1;
		check(!valid(s), "invalid inventory rejected");
		std::cout << "Domain action, inventory, timing, social, lifecycle and blend checks passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
