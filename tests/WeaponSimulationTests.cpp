#include "WeaponSimulation.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
namespace {
void near(double a,double b,double eps=1e-10){assert(std::abs(a-b)<=eps*std::max(1.0,std::max(std::abs(a),std::abs(b))));}
lab::SimulationRequest fixture(){lab::SimulationRequest q;
 q.weapon={{"class","WP_AK74"},{"fire_dispersion_base","1"},{"fire_dispersion_condition_factor","2"},{"condition_shot_dec","0.001"},{"rpm","600"},{"ammo_mag_size","30"},{"fire_modes","1, 3, -1"},{"preffered_fire_mode","-1"},{"misfire_probability","0.001"},{"bullet_speed","100"},{"fire_distance","1000"}};
 q.ammo={{"k_disp","1.2"},{"impair","2"},{"buck_shot","1"},{"k_dist","1"}};
 q.bulletManager={{"gravity_const","10"},{"air_resistance_k","0.4"},{"time_step","100"},{"min_bullet_speed","3"}};q.weaponSection="wpn_test";q.ammoSection="ammo_test";return q;}
void sourceFormula(){auto q=fixture();const auto r=lab::SimulateWeapon(q);assert(r.supported&&r.spreadValid&&r.trajectoryValid);assert(r.spread.size()==30&&r.shots.size()==30);near(r.magazineWear,.06);near(r.conditionAfterMagazine,.94);near(r.magazineDuration,2.9);assert(r.shotsToZero==500&&r.fullMagazinesToZero==16&&r.remainingRoundsToZero==20);
 near(r.firstDispersionRadians,(3.14159265358979323846/180)*1.2*(1+2*.002));
 assert(r.shots.front().conditionAfter<r.shots.front().conditionBefore);near(r.shots.back().misfireProbability,.001+std::pow(.06,3));
 const auto same=lab::SimulateWeapon(q);for(size_t i=0;i<r.spread.size();++i){assert(r.spread[i].x==same.spread[i].x&&r.spread[i].y==same.spread[i].y);assert(std::hypot(r.spread[i].x,r.spread[i].y)<=r.maxConeRadius+1e-12);}
 q.weapon["rpm"]="1200";const auto fast=lab::SimulateWeapon(q);near(fast.magazineDuration,r.magazineDuration/2);for(size_t i=0;i<r.spread.size();++i){near(r.spread[i].x,fast.spread[i].x);near(r.spread[i].y,fast.spread[i].y);} // no fabricated rate/cone coupling
 q.spreadDistance=100;const auto far=lab::SimulateWeapon(q);near(far.spread[0].x,fast.spread[0].x*2);near(far.spread[0].y,fast.spread[0].y*2);
}
void wearModes(){auto q=fixture();q.weapon["condition_shot_dec"]="0.01";q.ammo["impair"]="1";q.weapon["ammo_mag_size"]="6";q.weapon["preffered_fire_mode"]="2";q.fireMode=3;
 auto r=lab::SimulateWeapon(q);near(r.magazineWear,.12);assert(r.shotsToZero==51);assert(r.shots[2].shotInTrigger==3&&r.shots[3].shotInTrigger==1);near(r.magazineDuration,.75);
 q.fireMode=-1;r=lab::SimulateWeapon(q);near(r.magazineWear,.21);assert(r.shotsToZero==30); // four full triangular magazines=.84; fifth shot6 passes1
 q.fireMode=1;r=lab::SimulateWeapon(q);near(r.magazineWear,.06);assert(r.shotsToZero==100);near(r.magazineDuration,1.75);
 q.ammo["buck_shot"]="8";const auto pellets=lab::SimulateWeapon(q);near(pellets.magazineWear,r.magazineWear);assert(pellets.spread.size()==48);assert(pellets.shotsToZero==100);
 q.condition=0;r=lab::SimulateWeapon(q);assert(r.spreadValid&&r.roundsFired==6&&r.shotsToZero==0);near(r.shots.front().misfireProbability,.99);
 q.condition=1;q.weapon["condition_shot_dec"]="0";r=lab::SimulateWeapon(q);assert(!r.lifetimeFinite&&r.magazineWear==0);
 q.weapon["condition_shot_dec"]="1e-30";r=lab::SimulateWeapon(q);assert(r.lifetimeFinite&&r.lifetimeCapped); // no unbounded shot loop
}
void trajectory(){lab::LaboratoryBullet b;b.vx=100;lab::StepLaboratoryBullet(b,.1,.4,10);near(b.x,10);near(b.y,0);near(b.vx,96);near(b.vy,-1);
 lab::StepLaboratoryBullet(b,.1,.4,10);near(b.x,19.6);near(b.y,-.1);near(b.vx,92.16);near(b.vy,-1.96);
 auto q=fixture();q.trajectoryDistance=100;q.bulletManager["air_resistance_k"]="0";auto r=lab::SimulateWeapon(q);assert(r.trajectoryReached);near(r.timeAtTarget,1);near(r.dropAtTarget,4.5); // explicit Euler, NOT continuous 5m parabola
 q.ammo["k_air_resistance"]="999";const auto ammoDrag=lab::SimulateWeapon(q);near(ammoDrag.dropAtTarget,r.dropAtTarget); // SP global drag only
 q.weapon["fire_distance"]="70";r=lab::SimulateWeapon(q);assert(r.trajectoryValid&&!r.trajectoryReached);assert(r.trajectory.back().distance<100);
 q.weapon["fire_distance"]="1000";q.bulletManager["min_bullet_speed"]="101";r=lab::SimulateWeapon(q);assert(!r.trajectoryReached);assert(r.trajectory.size()==2);
 q.bulletManager.erase("gravity_const");r=lab::SimulateWeapon(q);assert(r.spreadValid&&!r.trajectoryValid); // never invent9.81 for missing data
}
void classification(){auto q=fixture();for(const char* c:{"AMMO","G_F1","WP_RG6","WP_RPG7","W_BINOC","WP_KNIFE","W_MOUNTD","unknown"}){q.weapon["class"]=c;assert(!lab::SimulateWeapon(q).supported);}
 for(const char* c:{"WP_PM","WP_SVD","WP_SHOTG","W_FORT ","W_BM16"}){q.weapon["class"]=c;assert(lab::SimulationFireModes(q.weapon)==std::vector<int>{1});q.fireMode=1;assert(lab::SimulateWeapon(q).spreadValid);}
 q.weapon["class"]="WP_AK74";q.weapon.erase("fire_modes");assert(lab::SimulationFireModes(q.weapon)==std::vector<int>{-1});
 q.weapon["fire_modes"]="1, bad";assert(lab::SimulationFireModes(q.weapon).empty());
}
void validation(){auto q=fixture();for(const char* value:{"nan","inf","-1","0","0.5","10001"}){q.weapon["ammo_mag_size"]=value;assert(!lab::SimulateWeapon(q).spreadValid);}q=fixture();q.weapon["fire_dispersion_base"]="90";assert(!lab::SimulateWeapon(q).spreadValid);
 q=fixture();q.bulletManager["air_resistance_k"]="100";assert(!lab::SimulateWeapon(q).trajectoryValid);
 q=fixture();q.condition=std::numeric_limits<double>::quiet_NaN();assert(!lab::SimulateWeapon(q).spreadValid);
 assert(lab::DescribeSimulationParameter("condition_shot_dec").relevant);assert(lab::DescribeSimulationParameter("PDM_disp_base").relevant);assert(!lab::DescribeSimulationParameter("inv_weight").relevant);
}
void ammunitionMetadata(){
 // Inventory metadata must stay separate from weapon/fixture controls, including
 // common names such as inv_weight and properties of grenade ammunition.
 for(const char* key:{"class","box_size","cost","inv_weight","k_disp","k_hit","k_impulse","k_pierce","k_ap","k_dist","impair","buck_shot","tracer","tracer_color_ID","explosive","wm_size","k_air_resistance","can_be_unlimited","fake_grenade_name","visual","cform","inv_name","inv_name_short","description","inv_grid_x","inv_grid_y","inv_grid_width","inv_grid_height","kill_msg_x","kill_msg_y","kill_msg_width","kill_msg_height","immunities_sect","belt","default_to_ruck","can_take","can_trade","quest_item","slot","item_remove_time","$spawn","k_speed"}){
  const auto info=lab::DescribeAmmunitionParameter(key);
  assert(info.relevant&&!info.title.empty()&&!info.description.empty()&&!info.effect.empty());
 }
 for(const char* key:{"fire_dispersion_base","condition_shot_dec","rpm","cam_dispersion","PDM_disp_base","unknown_ammo_parameter","jump_height","$prefetch","GroupControlSection","discovery_dependency"}){
  const auto info=lab::DescribeAmmunitionParameter(key);
  assert(!info.relevant&&!info.title.empty()&&!info.effect.empty());
  assert(info.description.find(key)!=std::string::npos); // explicitly identifies the unverified key
 }
 assert(lab::DescribeAmmunitionParameter("inv_weight").relevant);
 assert(!lab::DescribeSimulationParameter("inv_weight").relevant);
 assert(lab::DescribeAmmunitionParameter("k_disp").description!=lab::DescribeSimulationParameter("k_disp").description);
}
}
int main(){sourceFormula();wearModes();trajectory();classification();validation();ammunitionMetadata();std::cout<<"WeaponSimulation PASS: source formulas, RNG, fire modes, condition, SP integrator, limits, ammunition metadata\n";}
