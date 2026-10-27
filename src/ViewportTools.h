#pragma once
#include "Core.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace lab {
// Kept independent of Win32/D3D so navigation and stroke edge cases can be tested.
struct ViewportCamera {
    V3 pivot{};
    float yaw=.65f, pitch=.25f, distance=3.8f;
    bool fly=true;

    V3 offset() const {
        return {distance*std::cos(pitch)*std::sin(yaw), distance*std::sin(pitch),
                distance*std::cos(pitch)*std::cos(yaw)};
    }
    V3 eye() const { const auto o=offset(); return {pivot.x+o.x,pivot.y+o.y,pivot.z+o.z}; }
    V3 up() const {return {-std::sin(pitch)*std::sin(yaw),std::cos(pitch),-std::sin(pitch)*std::cos(yaw)};}
    // 0..2 look from +X,+Y,+Z; 3..5 look from -X,-Y,-Z.
    // A signed view changes only orientation. FrameSelection remains in Renderer.
    void alignAxis(int axis){
        constexpr float halfPi=1.570796327f,pi=3.141592654f;
        switch(axis){
        case 0:yaw=halfPi;pitch=0.f;break;
        case 1:yaw=0.f;pitch=halfPi;break;
        case 2:yaw=0.f;pitch=0.f;break;
        case 3:yaw=-halfPi;pitch=0.f;break;
        case 4:yaw=0.f;pitch=-halfPi;break;
        case 5:yaw=pi;pitch=0.f;break;
        default:break;
        }
    }
    void look(float dx,float dy) {
        const auto before=eye();
        yaw=std::remainder(yaw+dx*.01f,6.283185307f);
        pitch=std::clamp(pitch+dy*.01f,-1.570796327f,1.570796327f);
        if(fly){const auto o=offset();pivot={before.x-o.x,before.y-o.y,before.z-o.z};}
    }
    void move(float right,float up,float forward) {
        pivot.x+=-std::cos(yaw)*right-std::cos(pitch)*std::sin(yaw)*forward;
        pivot.y+=up-std::sin(pitch)*forward;
        pivot.z+=std::sin(yaw)*right-std::cos(pitch)*std::cos(yaw)*forward;
    }
    void pan(float dx,float dy) { move(-dx*.0018f*distance,-dy*.0018f*distance,0); }
    void zoom(float steps) {
        if(fly) move(0,0,steps*std::max(.025f,distance*.12f));
        else distance=std::clamp(distance*std::pow(.86f,steps),.04f,24.f);
    }
    void frame(V3 center,float radius,float aspect) {
        pivot=center;
        const float halfFov=std::atan(std::tan(.392699082f)*std::min(1.f,std::max(.1f,aspect)));
        distance=std::max(.06f,radius*1.12f/std::sin(halfFov));
    }
    void reset() { const bool wasFly=fly; *this=ViewportCamera{}; fly=wasFly; }
};

}
