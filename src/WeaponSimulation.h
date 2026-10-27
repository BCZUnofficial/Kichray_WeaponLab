#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace lab {
using SimulationConfig=std::map<std::string,std::string>;
// The fixture holds the barrel still; player movement, aim and camera recoil are not added.
struct SimulationRequest {
    SimulationConfig weapon,ammo,bulletManager;
    std::string weaponSection,ammoSection;
    int fireMode=-1; // -1 automatic, 1 single, N round burst
    double condition=1.0;
    double spreadDistance=50.0; // UI range 10..100 metres
    double trajectoryDistance=200.0; // UI range 100..300 metres
    double triggerPause=0.25; // laboratory pause between trigger pulls, not a game constant
    uint32_t seed=1701;
};
struct SpreadPoint {double x=0,y=0,time=0,condition=1;int shot=0,pellet=0;};
struct TrajectoryPoint {double distance=0,height=0,time=0,speed=0;};
struct ShotSample {double time=0,conditionBefore=1,conditionAfter=1,dispersionRadians=0,misfireProbability=0;int shotInTrigger=0;};
struct SimulationResult {
    bool supported=false,spreadValid=false,trajectoryValid=false,trajectoryReached=false;
    bool lifetimeFinite=false,lifetimeCapped=false;
    std::string error,weaponType,ammoName,stopReason;
    std::vector<std::string> warnings;
    std::vector<SpreadPoint> spread;
    std::vector<TrajectoryPoint> trajectory;
    std::vector<ShotSample> shots;
    std::vector<int> fireModes;
    int magazineSize=0,pelletsPerShot=1,roundsFired=0;
    double rpm=0,shotInterval=0,magazineDuration=0;
    double initialCondition=1,conditionAfterMagazine=1,magazineWear=0;
    uint64_t shotsToZero=0,fullMagazinesToZero=0;
    int remainingRoundsToZero=0;
    double firstDispersionRadians=0,lastDispersionRadians=0,maxConeRadius=0;
    double startSpeed=0,maximumTravel=0,dropAtTarget=0,timeAtTarget=0,speedAtTarget=0;
};
struct SimulationParameterInfo {
    bool relevant=false;
    std::string title,description,effect;
};
// Only bullet-firing firearm classes. Grenades, ammo, knives, binoculars and rocket launchers are excluded.
bool IsSimulationFirearm(const SimulationConfig& weapon);
std::vector<int> SimulationFireModes(const SimulationConfig& weapon);
SimulationParameterInfo DescribeSimulationParameter(const std::string& key);
SimulationResult SimulateWeapon(const SimulationRequest& request);
// One step of single-player CBulletManager::CalcBullet in an unobstructed world.
// Position advances at the old velocity; linear drag and gravity change the next velocity.
struct LaboratoryBullet {double x=0,y=0,vx=0,vy=0,travelled=0,time=0;};
void StepLaboratoryBullet(LaboratoryBullet& bullet,double seconds,double drag,double gravity);
}
