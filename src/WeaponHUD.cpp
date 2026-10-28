#include "WeaponHUD.h"
#include <algorithm>
#include <cmath>

namespace lab {
HudMatrix HudIdentity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
HudMatrix HudMultiply(const HudMatrix& a,const HudMatrix& b){HudMatrix r{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)r[i*4+j]+=a[i*4+k]*b[k*4+j];return r;}
HudMatrix HudOffset(V3 pos,V3 hpb){
    constexpr float deg=3.14159265358979323846f/180.f;
    const float h=hpb.x*deg,p=hpb.y*deg,b=hpb.z*deg;
    const float ch=std::cos(h),sh=std::sin(h),cp=std::cos(p),sp=std::sin(p),cb=std::cos(b),sb=std::sin(b);
    return {ch*cb-sp*sh*sb,-cp*sb,sp*ch*sb+sh*cb,0,
        sp*sh*cb+ch*sb,cp*cb,sh*sb-sp*ch*cb,0,
        -cp*sh,sp,cp*ch,0,pos.x,pos.y,pos.z,1};
}
V3 HudDirection(const HudMatrix& m,V3 p){return {p.x*m[0]+p.y*m[4]+p.z*m[8],p.x*m[1]+p.y*m[5]+p.z*m[9],p.x*m[2]+p.y*m[6]+p.z*m[10]};}
V3 HudPoint(const HudMatrix& m,V3 p){auto r=HudDirection(m,p);return {r.x+m[12],r.y+m[13],r.z+m[14]};}
HudMatrix HudParticleBasis(const HudMatrix& hud,V3 origin){
    auto d=HudDirection(hud,{0,0,1});const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(length<1e-8f)return HudIdentity();
    d={d.x/length,d.y/length,d.z/length};V3 right,up;
    // Same forward/up convention as generate_orthonormal_basis_normalized.
    if(std::abs(d.y)>.99999f){right={d.y>0?-1.f:1.f,0,0};up={0,0,1};}
    else {const float q=1/std::sqrt(d.x*d.x+d.z*d.z);right={d.z*q,0,-d.x*q};up={d.y*right.z,d.z*right.x-d.x*right.z,-d.y*right.x};}
    return {right.x,right.y,right.z,0,up.x,up.y,up.z,0,d.x,d.y,d.z,0,origin.x,origin.y,origin.z,1};
}
std::string ConfigString(const WeaponConfigValues& v,const std::string& k){auto i=v.find(k);if(i==v.end())return {};auto s=i->second;auto a=s.find_first_not_of(" \t");if(a==std::string::npos)return {};s=s.substr(a,s.find_last_not_of(" \t")-a+1);if(s.size()>=2&&s.front()=='"'&&s.back()=='"')s=s.substr(1,s.size()-2);return s;}
bool ValidateHudParameter(const std::string& key,const std::string& value,std::string& error){
    const bool vector=key=="position"||key=="orientation"||key=="fire_point"||key=="fire_point2"||key=="shell_point"||key=="zoom_offset"||key=="grenade_zoom_offset";
    const bool scalar=key=="zoom_rotate_x"||key=="zoom_rotate_y"||key=="grenade_zoom_rotate_x"||key=="grenade_zoom_rotate_y";
    if(!vector&&!scalar)return true;std::vector<double> n;
    if(!ParseWeaponNumbers(value,n)||n.size()!=(vector?3u:1u)||std::any_of(n.begin(),n.end(),[](double x){return std::abs(x)>100000;})){
        error=vector?"Нужны три конечных числа X, Y, Z (через запятую).":"Нужно одно конечное число.";return false;}return true;
}
bool ReadWeaponHUD(const WeaponDatabase& db,const std::string& weapon,HudConfiguration& out,std::string& error){
    out={};WeaponConfigValues w,v;if(!db.sectionValues(weapon,w,error))return false;out.section=ConfigString(w,"hud");
    if(out.section.empty()){error="У оружия не указана секция hud.";return false;}
    if(!db.sectionValues(out.section,v,error))return false;
    out.visual=ConfigString(v,"visual");out.fireBone=ConfigString(v,"fire_bone");out.idleMotion=ConfigString(v,"anim_idle");
    if(out.visual.empty()){error="В ["+out.section+"] не указан visual.";return false;}
    auto get=[&](const char* key,V3& target,bool required){auto i=v.find(key);if(i==v.end()){if(required)error="В ["+out.section+"] отсутствует "+key;return !required;}
        if(!ValidateHudParameter(key,i->second,error))return false;std::vector<double> n;ParseWeaponNumbers(i->second,n);target={float(n[0]),float(n[1]),float(n[2])};return true;};
    if(!get("position",out.position,true)||!get("orientation",out.orientation,true)||!get("fire_point",out.firePoint,true))return false;
    out.firePoint2=out.firePoint;out.hasShellPoint=v.count("shell_point")!=0;
    if(!get("fire_point2",out.firePoint2,false)||!get("shell_point",out.shellPoint,false)||!get("zoom_offset",out.zoomOffset,false))return false;
    for(const auto& p:std::array<std::pair<const char*,float*>,2>{{{"zoom_rotate_x",&out.zoomX},{"zoom_rotate_y",&out.zoomY}}}){
        auto i=v.find(p.first);if(i==v.end())continue;if(!ValidateHudParameter(p.first,i->second,error))return false;std::vector<double> n;ParseWeaponNumbers(i->second,n);*p.second=float(n[0]);}
    return true;
}
HudMatrix HudModelTransform(const HudConfiguration& h,bool aiming){
    auto m=HudOffset(h.position,h.orientation);if(!aiming)return m;
    // Weapon::UpdateHudAdditonal: rotateX, mulA(rotateY), translate_over.
    const float cx=std::cos(h.zoomX),sx=std::sin(h.zoomX),cy=std::cos(h.zoomY),sy=std::sin(h.zoomY);
    HudMatrix x={1,0,0,0,0,cx,sx,0,0,-sx,cx,0,0,0,0,1};
    HudMatrix y={cy,0,-sy,0,0,1,0,0,sy,0,cy,0,0,0,0,1};auto z=HudMultiply(x,y);
    z[12]=h.zoomOffset.x;z[13]=h.zoomOffset.y;z[14]=h.zoomOffset.z;return HudMultiply(m,z);
}
const std::array<WeaponEffectSlot,6>& WeaponEffectSlots(){static const std::array<WeaponEffectSlot,6> s={{{"flame_particles","Пламя","fire_point"},{"smoke_particles","Дым","fire_point"},{"shell_particles","Гильзы","shell_point"},{"flame_particles_2","Второе пламя","fire_point2"},{"silencer_flame_particles","Пламя глушителя","fire_point"},{"silencer_smoke_particles","Дым глушителя","fire_point"}}};return s;}
ParameterGroup ClassifyWeaponParameter(const std::string& k,bool hud){
    auto has=[&](const char* s){return k.find(s)!=std::string::npos;};auto starts=[&](const char* s){return k.rfind(s,0)==0;};
    if(k=="fire_bone"||starts("fire_point")||k=="shell_point")return ParameterGroup::Emitters;
    if(has("particles")||starts("light_"))return ParameterGroup::Effects;
    if(hud&&(k=="position"||k=="orientation"||has("zoom_offset")||has("zoom_rotate")))return ParameterGroup::HudPose;
    if(has("disp")||starts("cam_")||starts("zoom_cam_")||starts("PDM_"))return ParameterGroup::Accuracy;
    if(has("condition")||has("misfire")||k=="impair")return ParameterGroup::Durability;
    if(starts("hit_")||k=="k_hit"||k=="k_impulse"||k=="k_ap"||k=="k_pierce"||k=="wm_size"||k=="explosive")return ParameterGroup::Damage;
    if(k=="bullet_speed"||k=="fire_distance"||k=="k_dist"||k=="k_air_resistance"||k=="k_speed"||starts("tracer"))return ParameterGroup::Ballistics;
    if(starts("rpm")||has("fire_mode")||k=="queue_size"||starts("fire_single"))return ParameterGroup::FireMode;
    if(starts("ammo_")||k=="box_size"||k=="buck_shot"||k=="can_be_unlimited"||k=="fake_grenade_name")return ParameterGroup::Ammo;
    if(starts("snd_"))return ParameterGroup::Sound;
    if(starts("anim")||has("motion")||k=="allow_inertion")return ParameterGroup::Animation;
    if(has("scope")||has("silencer")||has("grenade_launcher")||has("addon"))return ParameterGroup::Addons;
    if(starts("inv_grid")||starts("kill_msg"))return ParameterGroup::Icon;
    if(k=="cost"||k=="inv_weight"||k=="slot"||has("ruck")||starts("ef_")||k=="belt"||k=="default_to_ruck"||k=="sprint_allowed")return ParameterGroup::Inventory;
    if(k=="visual"||k=="hud"||k=="class"||starts("inv_name")||k=="description"||k=="cform"||starts("$"))return ParameterGroup::Identity;
    if(k=="position"||k=="orientation"||starts("strap_"))return ParameterGroup::HudPose;
    return ParameterGroup::Other;
}
const char* ParameterGroupTitle(ParameterGroup g){static const char* names[]={"Урон и пробитие","Точность и отдача","Прочность и осечки","Баллистика и трассеры","Темп и режимы огня","Боеприпасы и магазин","Положение и поворот","Точки эффектов","Эффекты и вспышка","Анимации и инерция","Звуки","Обвесы","Инвентарь и стоимость","Иконка и сетка","Модель и описание","Прочие параметры"};return int(g)>=0&&g<ParameterGroup::Count?names[int(g)]:names[15];}
std::string HudParameterHelp(const std::string& k){
    if(k=="position")return "Вся HUD-модель относительно камеры: X вправо, Y вверх, Z вперёд; метры. OGF не изменяется.";
    if(k=="orientation")return "Вся модель: heading (вокруг Y), pitch (вокруг X), bank (вокруг Z), градусы; порядок setHPB движка.";
    if(k=="fire_point"||k=="fire_point2"||k=="shell_point")return "Смещение в метрах в локальной системе fire_bone. Вторая точка используется вторым пламенем; дым использует fire_point.";
    if(k=="fire_bone")return "Имя кости HUD для всех трёх точек. Направление эффекта берётся из всей HUD-модели, а не из поворота кости.";
    if(k=="zoom_offset")return "Сдвиг всей модели при прицеливании, метры. Включите «Прицеливание» в HUD.";
    if(k=="zoom_rotate_x"||k=="zoom_rotate_y")return "Поворот при прицеливании, РАДИАНЫ (в отличие от orientation).";
    if(k=="visual")return "OGF первого лица из meshes. Секция HUD может быть общей у нескольких оружий.";
    return {};
}
}
