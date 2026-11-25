#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
namespace lastlight {
enum class Tactic { Progress,
	Acquire,
	Cover,
	Peek,
	Flank,
	Withdraw,
	Reload,
	Treat,
	Rescue,
	Investigate,
	Suppress,
	Hold,
	Smoke,
	Count };
constexpr int TACTIC_COUNT = int(Tactic::Count);
inline const char *tactic_name(Tactic a) {
	static const char *names[] = { "Advance mission", "Acquire supplies", "Move to protected cover", "Peek and engage", "Change firing angle", "Break contact", "Reload safely", "Treat in safety", "Rescue teammate", "Investigate evidence", "Suppress threat", "Hold and observe", "Deploy smoke" };
	return int(a) >= 0 && int(a) < TACTIC_COUNT ? names[int(a)] : "Invalid";
}
struct TacticalInput {
	float health = 100, suppression = 0, caution = .6f, confidence = .5f, loyalty = .5f, aggression = .25f;
	int rounds = 0, reserve = 0, magazine = 6, enemies = 0, allies = 0, smoke = 0;
	float cover_distance = 0, distance = 100, range = 30, contact_age = 100, time_pressure = 0;
	bool danger = false, visible = false, hostile = false, firing_lane = false, ready = false;
	bool protected_here = false, cover_available = false, peek_available = false, flank_available = false, retreat_available = true;
	bool finishing_peek = false;
	bool needs_loot = false, has_medical = false, downed_ally = false, rescue_safe = false, heard = false, waiting = false;
	std::array<float, TACTIC_COUNT> learned{};
};
struct TacticalDecision {
	Tactic action = Tactic::Hold;
	std::array<float, TACTIC_COUNT> score{};
	std::array<bool, TACTIC_COUNT> legal{};
	std::array<std::string, TACTIC_COUNT> reason{};
};
inline int tactical_context(const TacticalInput &c) {
	int h = c.health < 35 ? 0 : c.health < 75 ? 1
											  : 2;
	int ammo = c.rounds == 0 ? 0 : c.rounds <= 2 ? 1
												 : 2;
	int threats = c.enemies == 0 ? 0 : c.enemies == 1 ? 1
													  : 2;
	return (((((h * 3 + ammo) * 3 + threats) * 2 + int(c.protected_here)) * 2 + int(c.allies > 0)) * 2 + int(c.suppression > .45f));
}
inline void learn_tactical_value(std::array<float, TACTIC_COUNT> &values, int action, float reward, float next) {
	if (action < 0 || action >= TACTIC_COUNT || !std::isfinite(reward) || !std::isfinite(next)) {
		return;
	}
	values[action] = std::clamp(values[action] + .12f * (std::clamp(reward, -2.f, 2.f) + .7f * std::clamp(next, -2.f, 2.f) - values[action]), -2.f, 2.f);
}
// Safety constraints are authoritative. Learned preferences can rank legal
// choices but cannot authorize blind fire, friendly fire, imaginary ammunition
// or exposed medical actions. Hysteresis prevents intention flicker.
inline TacticalDecision choose_tactic(const TacticalInput &c, Tactic previous = Tactic::Hold, bool committed = false) {
	TacticalDecision out;
	out.score.fill(-100000);
	auto option = [&](Tactic a, bool valid, float score, const char *why, const char *veto) {int i=int(a);out.legal[i]=valid;out.score[i]=valid?score+std::clamp(c.learned[i],-2.f,2.f)*3.f:-100000;out.reason[i]=valid?why:veto; };
	const bool critical = c.health < 32;
	const bool outgunned = c.enemies > c.allies + 1 || c.rounds < 2 || critical;
	const bool recovering_safe = !c.danger || (c.protected_here && c.suppression < .4f && (!c.visible || c.distance > 10));
	const bool fire_safe = c.hostile && c.visible && c.firing_lane && c.ready && c.rounds > 0 && c.distance <= c.range;
	const bool can_peek = c.hostile && c.peek_available && c.cover_available && c.contact_age < 4 &&
			c.ready && c.rounds > 0 && c.distance <= c.range;
	const bool close_shot = fire_safe && c.distance <= std::min(18.f, c.range * .7f);
	const bool close_cover = c.cover_available && !c.protected_here && c.cover_distance < 4;
	const float engagement_bonus = c.finishing_peek && !outgunned && c.suppression < .5f ? 32.f :
			close_cover ? 0.f : close_shot ? 35.f : fire_safe ? 16.f : 0.f;
	const bool sheltered_route = c.protected_here && !c.visible && c.suppression < .22f;
	option(Tactic::Progress, (!c.danger || sheltered_route) && !c.waiting, (sheltered_route ? 49 : 36) + c.time_pressure * 12, "Continue the committed mission route", "Current danger must be resolved before ordinary travel");
	option(Tactic::Acquire, c.needs_loot && !c.danger, 48, "Acquire a real known supply that improves readiness", "No useful capacity-compatible supply or exposed under threat");
	option(Tactic::Cover, c.danger && c.cover_available && !c.protected_here, 68 + c.suppression * 30 + c.caution * 12 + (outgunned ? 12 : 0) - std::min(30.f, c.cover_distance) * .85f, "Reach physically verified cover before spending health", "No available reachable protection or already sheltered");
	option(Tactic::Peek, (fire_safe || can_peek) && (!critical || c.protected_here) && (c.protected_here || c.enemies <= c.allies + 1), 48 + c.confidence * 16 + c.aggression * 9 - c.suppression * 18 + (c.protected_here ? 10 : 0) + engagement_bonus, "Expose briefly for a confirmed hostile and clear firing lane", "Requires observed hostile, ammunition, reaction time, range and safe firing lane");
	option(Tactic::Flank, c.hostile && c.contact_age < 4 && c.flank_available && c.rounds >= 2 && !critical && c.suppression < .65f, 51 + c.confidence * 12 + (c.firing_lane ? 0 : 21) + (c.enemies <= c.allies ? 8 : 0), "Change angle while respecting cover and friendly positions", "Flank is unavailable, stale, pinned or unaffordable in health/ammunition");
	option(Tactic::Withdraw, c.danger && c.retreat_available, 47 + c.caution * 20 + (outgunned ? 32 : 0) + (critical ? 32 : 0) + c.suppression * 20 - (close_shot && !outgunned ? 15 : 0), "Preserve the survivor when the engagement budget is poor", "No present danger or safe retreat candidate");
	option(Tactic::Reload, c.rounds < c.magazine && c.reserve > 0 && recovering_safe, 32 + (c.rounds == 0 ? 57 : 0) + (c.rounds < c.magazine * .3f ? 20 : 0), "Reload with compatible reserve while protected", "No compatible reserve or reload would expose the survivor");
	option(Tactic::Treat, c.has_medical && c.health < 85 && recovering_safe, 30 + (100 - c.health) * .8f, "Treat injury after reaching safety", "Medical supply missing, unnecessary or unsafe under fire");
	option(Tactic::Rescue, c.downed_ally && c.has_medical && c.rescue_safe, 44 + c.loyalty * 36, "Rescue with a safe approach and adequate medical supplies", "A casualty needs a protected approach or smoke/support first");
	option(Tactic::Investigate, c.heard && !c.visible && !critical && c.suppression < .45f, 39 + c.confidence * 8 - c.caution * 7, "Search the last evidence position without inventing an identity", "No anonymous evidence, too injured or pinned");
	option(Tactic::Suppress, c.hostile && c.contact_age < 2 && c.firing_lane && c.ready && c.rounds >= 6 && c.protected_here && c.allies > 0, 40 + c.loyalty * 16 + (c.enemies > 1 ? 8 : 0), "Spend a bounded burst so a teammate can move", "Requires recent hostile evidence, protection, clear lane and surplus ammunition");
	option(Tactic::Hold, true, c.waiting ? 58 : 8, "Observe or hold an assigned safe position", "Always available as the bounded fallback");
	option(Tactic::Smoke, c.smoke > 0 && c.danger && (outgunned || c.downed_ally || c.suppression > .45f), 83 + c.caution * 12 + (critical ? 12 : 0), "Screen a withdrawal or rescue before exposing the survivor", "Smoke is absent or not justified by present risk");
	int best = int(Tactic::Hold);
	for (int i = 0; i < TACTIC_COUNT; ++i) {
		if (out.legal[i] && out.score[i] > out.score[best]) {
			best = i;
		}
	}
	const int old = int(previous);
	if (committed && old >= 0 && old < TACTIC_COUNT && out.legal[old] && out.score[old] >= out.score[best] - 8) {
		best = old;
	}
	out.action = Tactic(best);
	return out;
}
} // namespace lastlight
