#include "last_light_game.h"
namespace {
float flat(Vector3 a, Vector3 b) {
	a.y = b.y = 0;
	return a.distance_to(b);
}
} //namespace
bool LastLightGame::issue_order(int id, int order) {
	auto a = find_actor(id), human = find_actor(1);
	if (!compound_world || !a || !human || id < 2 || id > 5 || order < 0 || order > 4 || !a->stats.active() || !human->stats.active()) {
		return false;
	}
	bool voice = flat(a->body->get_position(), human->body->get_position()) < 18 && a->visible_actors.count(1);
	if (!voice && !power_on) {
		a->order_reason = "No voice contact; restore generator power for squad radio";
		notify(a->order_reason);
		return false;
	}
	if (order && (a->memory[1].threat > .35f || a->personality.loyalty + MAX(a->memory[1].trust, a->persistent_trust[1]) < .4f)) {
		a->order_reason = "Refused: distrust outweighs loyalty";
		notify(a->order_reason);
		return false;
	}
	a->order = order;
	a->order_peer = 1;
	a->order_position = human->body->get_position();
	a->order_until = elapsed + 45;
	const char *labels[] = { "Independent mission planning", "Follow last confirmed leader position", "Hold this position and observe", "Scavenge useful observed supplies", "Assist the closest unfinished mission task" };
	a->order_reason = String(labels[order]) + "; survival constraints remain active";
	a->next_plan = 0;
	a->task_claim = 0;
	for (auto it = mission_claims.begin(); it != mission_claims.end();) {
		if (it->second.owner == id) {
			it = mission_claims.erase(it);
		} else {
			++it;
		}
	}
	notify(a->order_reason);
	return true;
}
