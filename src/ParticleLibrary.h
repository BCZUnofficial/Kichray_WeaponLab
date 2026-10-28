#pragma once
#include "Core.h"
#include <map>
#include <functional>

namespace lab {
struct ParticleSample {
    V3 position{},velocity{},size{.05f,.05f,.05f},rotation{};
    float r=1,g=1,b=1,a=1,age=0,frame=0;
};
struct ParticleLayer {
    std::string name,texture,shader;
    uint32_t flags=0;
    float frameU=1,frameV=1,frameSpeed=0;
    uint32_t frameColumns=1,frameCount=1;
    V3 velocityScale{},defaultRotation{};
    std::vector<ParticleSample> particles;
};
struct ParticleSnapshot { std::vector<ParticleLayer> layers;std::vector<std::string> warnings;float sampledTime=0; };
struct ParticleEntry {std::string name;bool group=false;};
// Read-only particles.xr v1, PED v1 and PGD v3, as in the pinned engine master.
// The game library and its textures are never rewritten by the editor.
class ParticleLibrary {
    struct Action {uint32_t type=0,flags=0;std::vector<uint8_t> data;};
    struct Effect {ParticleLayer layer;uint32_t maximum=0;float limit=0;std::vector<Action> actions;};
    struct Member {std::string effect,onPlay,onBirth,onDeath;float start=0,end=0;uint32_t flags=0;};
    struct Group {std::vector<Member> members;float limit=0;};
    std::map<std::string,Effect> effects_;
    std::map<std::string,Group> groups_;
    std::vector<ParticleEntry> entries_;
    ParticleLayer sampleEffect(const Effect& effect,float time,uint32_t seed,std::vector<std::string>& warnings,float stop=-1)const;
public:
    bool load(const std::vector<uint8_t>& bytes,std::string& error);
    const std::vector<ParticleEntry>& entries()const{return entries_;}
    bool contains(const std::string& name)const;
    ParticleSnapshot snapshot(const std::string& name,float time,uint32_t seed=1701)const;
    // Keep exact-time sampling above; editor may explicitly choose a visible frame.
    ParticleSnapshot preview(const std::string& name,float time,bool automatic,uint32_t seed=1701)const;
};
enum class ParticleBlend { Alpha, Add, AlphaAdd };
ParticleBlend ParticleShaderBlend(const std::string& shader);
bool ParticleSnapshotVisible(const ParticleSnapshot& snapshot);
Image ParticleThumbnail(const ParticleSnapshot& snapshot,const std::function<Image(const std::string&)>& texture,uint32_t size=384);
}
