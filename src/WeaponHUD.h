#pragma once
#include "Core.h"
#include "WeaponData.h"
#include <array>

namespace lab {
// Row-vector matrices; native X-Ray metres, no viewer normalization.
using HudMatrix=std::array<float,16>;
HudMatrix HudIdentity();
HudMatrix HudMultiply(const HudMatrix& first,const HudMatrix& then);
HudMatrix HudOffset(V3 position,V3 headingPitchBankDegrees);
V3 HudPoint(const HudMatrix& matrix,V3 point);
V3 HudDirection(const HudMatrix& matrix,V3 direction);
HudMatrix HudParticleBasis(const HudMatrix& hud,V3 origin);
std::string ConfigString(const WeaponConfigValues& values,const std::string& key);
struct HudConfiguration {
    std::string section,visual,fireBone,idleMotion;
    V3 position{},orientation{},firePoint{},firePoint2{},shellPoint{},zoomOffset{};
    float zoomX=0,zoomY=0;
    bool hasShellPoint=false;
};
bool ReadWeaponHUD(const WeaponDatabase& database,const std::string& weapon,
                   HudConfiguration& hud,std::string& error);
HudMatrix HudModelTransform(const HudConfiguration& hud,bool aiming);
bool ValidateHudParameter(const std::string& key,const std::string& value,std::string& error);
// A disjoint, deterministic classification. Unrecognized keys stay editable.
enum class ParameterGroup { Damage,Accuracy,Durability,Ballistics,FireMode,Ammo,
    HudPose,Emitters,Effects,Animation,Sound,Addons,Inventory,Icon,Identity,Other,Count };
ParameterGroup ClassifyWeaponParameter(const std::string& key,bool hud=false);
const char* ParameterGroupTitle(ParameterGroup group);
std::string HudParameterHelp(const std::string& key);
struct WeaponEffectSlot { const char* key;const char* label;const char* point; };
const std::array<WeaponEffectSlot,6>& WeaponEffectSlots();
}
