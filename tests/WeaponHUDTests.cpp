#include "WeaponHUD.h"
#include "ParticleLibrary.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace {
using Bytes=std::vector<uint8_t>;
void u(Bytes& b,uint32_t n){for(int i=0;i<4;++i)b.push_back(uint8_t(n>>(i*8)));}
void f(Bytes& b,float n){uint32_t v;std::memcpy(&v,&n,4);u(b,v);}
void vec(Bytes& b,float x,float y,float z){f(b,x);f(b,y);f(b,z);}
void z(Bytes& b,const char* s){b.insert(b.end(),s,s+std::strlen(s)+1);}
void chunk(Bytes& b,uint32_t id,const Bytes& payload){u(b,id);u(b,uint32_t(payload.size()));b.insert(b.end(),payload.begin(),payload.end());}
Bytes word(uint32_t v){Bytes b;u(b,v);return b;}
void pointDomain(Bytes& b,float x,float y,float z0){u(b,0);vec(b,x,y,z0);for(int i=0;i<13;++i)f(b,0);}
Bytes fixture(bool group=false){
    Bytes root;chunk(root,1,{1,0});Bytes effect;chunk(effect,1,{1,0});Bytes name;z(name,"weapons\\fixture");chunk(effect,2,name);chunk(effect,3,word(30));chunk(effect,5,word(1));
    Bytes sprite;z(sprite,"particles\\blend");z(sprite,"fx\\fixture");chunk(effect,7,sprite);
    Bytes actions;u(actions,4);
    u(actions,21);u(actions,0);u(actions,21); // type, flags, serialized type
    pointDomain(actions,0,0,1);pointDomain(actions,1,0,0);pointDomain(actions,0,0,0);pointDomain(actions,.1f,.2f,.1f);pointDomain(actions,1,.5f,.2f);
    f(actions,1);f(actions,30.303031f);f(actions,0);f(actions,0);vec(actions,0,0,0);f(actions,0);
    u(actions,8);u(actions,0);u(actions,8);vec(actions,0,-9,0);
    u(actions,12);u(actions,0);u(actions,12);
    u(actions,10);u(actions,0);u(actions,10);f(actions,2);u(actions,0);
    chunk(effect,4,actions);Bytes effects;chunk(effects,0,effect);chunk(root,3,effects);
    if(group){Bytes g;chunk(g,1,{3,0});name.clear();z(name,"weapons\\group");chunk(g,2,name);chunk(g,3,word(0));Bytes members;u(members,1);z(members,"weapons\\fixture");z(members,"");z(members,"");z(members,"");f(members,.5f);f(members,1);u(members,4);chunk(g,4,members);Bytes groups;chunk(groups,0,g);chunk(root,4,groups);}
    return root;
}
bool near(float a,float b){return std::abs(a-b)<1e-5f;}
void write(const std::filesystem::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f<<text;assert(f.good());}
}
int main(){
    using namespace lab;
    auto m=HudOffset({1,2,3},{0,0,0});auto p=HudPoint(m,{.5f,1,2});assert(near(p.x,1.5f)&&near(p.y,3)&&near(p.z,5));
    p=HudDirection(HudOffset({},{90,0,0}),{0,0,1});assert(near(p.x,-1)&&near(p.y,0)&&near(p.z,0));
    p=HudDirection(HudOffset({},{0,90,0}),{0,0,1});assert(near(p.x,0)&&near(p.y,1)&&near(p.z,0));
    p=HudDirection(HudOffset({},{0,0,90}),{1,0,0});assert(near(p.x,0)&&near(p.y,-1)&&near(p.z,0));
    auto chained=HudMultiply(HudOffset({1,0,0},{}),HudOffset({0,2,0},{90,0,0}));p=HudPoint(chained,{});assert(near(p.x,0)&&near(p.y,2)&&near(p.z,1));
    HudConfiguration h;h.position={1,2,3};h.zoomX=1.57079632679f;h.zoomOffset={0,0,1};p=HudPoint(HudModelTransform(h,true),{});assert(near(p.x,1)&&near(p.y,-3)&&near(p.z,3));
    auto basis=HudParticleBasis(HudOffset({},{30,20,45}),{4,5,6});auto forward=HudDirection(basis,{0,0,1}),reference=HudDirection(HudOffset({},{30,20,45}),{0,0,1});assert(near(forward.x,reference.x)&&near(forward.y,reference.y));p=HudPoint(basis,{});assert(p.x==4&&p.y==5&&p.z==6);
    std::string error;assert(!ValidateHudParameter("position","1,2",error));assert(!ValidateHudParameter("orientation","nan,0,0",error));assert(ValidateHudParameter("position","-1, -.25, 0.001",error));
    assert(ClassifyWeaponParameter("fire_dispersion_base")==ParameterGroup::Accuracy);assert(ClassifyWeaponParameter("k_disp")==ParameterGroup::Accuracy);assert(ClassifyWeaponParameter("condition_shot_dec")==ParameterGroup::Durability);assert(ClassifyWeaponParameter("bullet_speed")==ParameterGroup::Ballistics);assert(ClassifyWeaponParameter("fire_point",true)==ParameterGroup::Emitters);assert(ClassifyWeaponParameter("orientation",true)==ParameterGroup::HudPose);assert(ClassifyWeaponParameter("unrecognized")==ParameterGroup::Other);
    const auto dir=std::filesystem::temp_directory_path()/("weaponlab-hud-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir/"weapons");
    write(dir/"text.xml","<string_table></string_table>");write(dir/"weapons"/"test.ltx","[effect_base]\nflame_particles = weapons\\fixture\n[wpn_test]:effect_base\nclass = WP_AK74\nvisual = world.ogf\nhud = gun_view\n[gun_view]:pose_base\nvisual = hud.ogf\nfire_bone = muzzle\nfire_point = 0, 0, 1\n[pose_base]\nposition = 0.1, -0.2, 0.3\norientation = 0, 0, 0\n");
    WeaponDatabase db;assert(db.load(dir/"weapons",dir/"text.xml",error));assert(ReadWeaponHUD(db,"wpn_test",h,error));assert(h.section=="gun_view"&&h.firePoint2.z==1&&!h.hasShellPoint);
    std::vector<WeaponValue> rows;assert(db.sectionParameters("gun_view",rows,error));bool inherited=false;for(const auto& r:rows)if(r.key=="position")inherited=r.inherited&&r.sourceSection=="pose_base";assert(inherited);
    assert(db.setValue("gun_view","position","-0.3, 0.1, 0.4",error));assert(db.setValue("wpn_test","flame_particles","",error));assert(db.save(error));assert(db.reload(error));assert(ReadWeaponHUD(db,"wpn_test",h,error)&&near(h.position.x,-.3f));WeaponConfigValues values;assert(db.sectionValues("wpn_test",values,error)&&values.count("flame_particles")&&values.at("flame_particles").empty());assert(db.sectionValues("effect_base",values,error)&&!values.at("flame_particles").empty());assert(db.sectionValues("pose_base",values,error)&&values.at("position")=="0.1, -0.2, 0.3");
    std::filesystem::remove_all(dir);
    ParticleLibrary library;auto bytes=fixture(true);assert(library.load(bytes,error));assert(library.entries().size()==2&&library.contains("weapons\\fixture"));
    auto snap=library.snapshot("weapons\\fixture",.1f);assert(snap.layers.size()==1&&snap.warnings.empty());assert(snap.layers[0].texture=="fx\\fixture"&&!snap.layers[0].particles.empty());const auto sample=snap.layers[0].particles.front();assert(sample.position.x>0&&sample.position.y<0&&sample.position.z==1);
    auto repeated=library.snapshot("weapons\\fixture",.1f);assert(repeated.layers[0].particles.size()==snap.layers[0].particles.size());assert(repeated.layers[0].particles.front().position.x==sample.position.x);
    assert(library.snapshot("weapons\\group",.4f).layers.empty());assert(!library.snapshot("weapons\\group",.6f).layers[0].particles.empty());assert(library.snapshot("weapons\\fixture",5).layers.empty());
    Image sprite;sprite.w=sprite.h=2;sprite.pixels.assign(4,{255,255,255,255});auto thumb=ParticleThumbnail(snap,[&](const std::string&){return sprite;},128);assert(thumb.valid());bool painted=false;for(const auto& v:thumb.pixels)painted|=v.r!=24;assert(painted);
    for(size_t cut=0;cut<bytes.size();++cut){auto truncated=Bytes(bytes.begin(),bytes.begin()+cut);ParticleLibrary bad;std::string diagnostic;if(bad.load(truncated,diagnostic)){for(const auto& e:bad.entries())bad.snapshot(e.name,.1f);}else assert(!diagnostic.empty());}
    Bytes compressed;chunk(compressed,0x80000001u,{1,0});assert(!library.load(compressed,error));assert(library.entries().size()==2); // transactional failure
    std::cout<<"PASS: HUD transforms, inheritance/save, disjoint groups, empty effect override, particles.xr/PED/PGD, frozen source/gravity/move, malformed chunks\n";
}
