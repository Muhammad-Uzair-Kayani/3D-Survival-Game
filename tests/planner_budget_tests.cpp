#include "../modules/last_light/strategy.h"
#include <iostream>
#include <stdexcept>
using namespace lastlight;
void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try{
 StrategicInput in;in.nodes={{{0,0},"start"},{{6,0},"supply"},{{12,0},"escape"}};in.edges={{0,1,0,0},{1,2,0,0}};in.enforce_budget=true;in.ammo=1;in.initial=Loaded;in.weight=14;in.capacity=14;
 StrategicAction ammo{"ammo","Observed ammunition",1,1,1,0,Loaded,0,1,0,1};ammo.ammo_gain=12;ammo.weight_delta=.18;
 StrategicAction leave{"escape","Escape past threat",2,0,5,Loaded,Escaped,0,1,0,1};leave.ammo_cost=8;
 in.actions={ammo,leave};auto full=make_strategy(in);require(full.alternatives.empty(),"Full carrying capacity cannot invent room for supplies");
 StrategicAction pack{"pack","Observed backpack",0,2,1,0,HasPack,0,1,0,1};pack.capacity_gain=10;pack.weight_delta=.5;in.actions.push_back(pack);
 auto prepared=make_strategy(in);require(!prepared.alternatives.empty()&&prepared.alternatives[0].complete,"A real backpack permits resupply and the affordable route");require(prepared.alternatives[0].expected_ammo>=4.9f,"Ammunition accounting includes actual resupply and expenditure");require(prepared.alternatives[0].expected_weight<=24,"Predicted load obeys upgraded capacity");
 bool refilled=false;for(auto&s:prepared.alternatives[0].steps)if(s.kind=="ammo")refilled=true;require(refilled,"An existing Loaded flag does not hide a real ammunition shortage");
 StrategicInput injured;injured.nodes={{{0,0},"injured"},{{10,0},"exit"}};injured.edges={{0,1,0,0}};injured.enforce_budget=true;injured.health=20;injured.min_health=25;
 StrategicAction heal{"heal","Use medical supplies",0,0,3,0,Treated,0,2,0,1};heal.medical_cost=1;heal.heal=40;
 StrategicAction exit{"extract","Leave",1,0,5,0,Escaped,0,1,0,1};injured.actions={heal,exit};require(make_strategy(injured).alternatives.empty(),"A plan cannot consume a nonexistent medical kit");injured.medical=1;auto safe=make_strategy(injured);require(!safe.alternatives.empty()&&safe.alternatives[0].expected_health>50,"Available medicine enables the otherwise unsafe route");
 StrategicInput unknown;unknown.nodes={{{0,0},"cache"}};unknown.enforce_budget=true;unknown.actions={{"search","Inspect unknown cache",0,1,0,0,Scouted,0,2,1,.5f}};auto partial=make_strategy(unknown);require(!partial.alternatives.empty()&&!partial.alternatives[0].complete,"Exploration is a partial plan and does not promise extraction");require(partial.alternatives[0].expected_ammo==0,"Unknown cache contents cannot be imagined as ammunition");
 StrategicInput high;high.nodes={{{0,0},"ground"},{{6,0},"gantry",false,3.6}};high.edges={{0,1,0,0}};StrategicAction gate{"operate","Upper console",1,5,5,0,Escaped,0,1,0,1};gate.exact=true;gate.destination={7,1};gate.elevation=3.6;high.actions={gate};auto elevated=make_strategy(high);require(elevated.alternatives[0].steps.back().height==3.6f&&elevated.alternatives[0].steps.back().point.x==7,"Plans preserve exact 3D target placement");
 std::cout<<"Resource budgets, real preparations, partial plans and elevation checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;} }
