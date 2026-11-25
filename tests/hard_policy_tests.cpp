#include "../modules/last_light/hard_policy.h"
#include <iostream>
#include <stdexcept>
using namespace lastlight;
void require(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){try{
 TacticalInput c;c.danger=true;c.hostile=true;c.visible=true;c.ready=true;c.firing_lane=true;c.distance=12;c.rounds=12;c.magazine=24;c.reserve=24;c.enemies=1;c.contact_age=0;c.cover_available=true;
 auto a=choose_tactic(c);require(a.action==Tactic::Cover,"Exposed agent reaches cover first");
 c.finishing_peek=true;require(choose_tactic(c,Tactic::Peek,true).action==Tactic::Peek,"An exposed peek gets a chance to fire before returning to the same cover");
 c.firing_lane=false;require(!choose_tactic(c,Tactic::Peek,true).legal[int(Tactic::Peek)],"A committed peek still respects the firing lane");
 c.finishing_peek=false;c.firing_lane=true;
 c.cover_distance=28;c.confidence=.9;require(choose_tactic(c).action==Tactic::Peek,"Healthy confident agent avoids an unnecessary long exposed run to distant cover");c.cover_distance=0;
 c.protected_here=true;c.cover_available=false;c.peek_available=true;c.confidence=.9;auto b=choose_tactic(c);require(b.action==Tactic::Peek,"Prepared protected agent engages a confirmed threat");
 c.firing_lane=false;c.flank_available=true;auto blocked=choose_tactic(c);require(blocked.action==Tactic::Flank,"Neutral firing obstruction produces a new angle, not a shot");require(!blocked.legal[int(Tactic::Peek)],"Friendly lane veto is hard");
 c.health=20;c.rounds=0;c.smoke=1;auto hurt=choose_tactic(c);require(hurt.action==Tactic::Smoke || hurt.action==Tactic::Withdraw,"Outgunned injured survivor preserves life");require(!hurt.legal[int(Tactic::Peek)],"Empty weapon can never fire");
 c.has_medical=true;c.suppression=.9;auto pinned=choose_tactic(c);require(!pinned.legal[int(Tactic::Treat)] && !pinned.legal[int(Tactic::Reload)],"Pinned survivor cannot heal/reload in the firing lane");
 c=TacticalInput();c.heard=true;c.confidence=.8;auto noise=choose_tactic(c);require(noise.action==Tactic::Investigate,"Anonymous noise leads to investigation");require(!noise.legal[int(Tactic::Peek)],"Noise cannot authorize an attack");
 c=TacticalInput();c.needs_loot=true;c.learned[int(Tactic::Peek)]=100;auto learned=choose_tactic(c);require(!learned.legal[int(Tactic::Peek)],"Learned utility cannot bypass policy");
 c=TacticalInput();c.downed_ally=true;c.has_medical=true;c.rescue_safe=false;require(!choose_tactic(c).legal[int(Tactic::Rescue)],"Dangerous rescue requires preparation");c.rescue_safe=true;c.loyalty=.9;require(choose_tactic(c).action==Tactic::Rescue,"Safe loyal rescue is prioritized");
 for(int h:{10,50,100})for(int ammo:{0,1,6})for(int enemies:{0,1,3})for(bool cover:{false,true})for(bool ally:{false,true})for(float suppression:{0.f,.9f}){c=TacticalInput();c.health=h;c.rounds=ammo;c.enemies=enemies;c.protected_here=cover;c.allies=ally;c.suppression=suppression;require(tactical_context(c)>=0&&tactical_context(c)<216,"Context bins bounded");}
 std::array<float,TACTIC_COUNT> values{};learn_tactical_value(values,int(Tactic::Cover),-1,0);require(values[int(Tactic::Cover)]<0,"Actual bad outcome reduces contextual action value");for(int k=0;k<1000;++k)learn_tactical_value(values,int(Tactic::Peek),2,2);require(values[int(Tactic::Peek)]<=2,"Learning is bounded");
 std::cout<<"Hard tactical constraints, cover, lanes, uncertainty, survival and learning checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;} }
