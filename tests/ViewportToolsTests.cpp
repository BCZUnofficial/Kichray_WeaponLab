#include "ViewportTools.h"
#include "MaterialAlpha.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
float distance(lab::V3 a,lab::V3 b){const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}

void cameraNavigation(){
    lab::ViewportCamera c;
    const auto eye=c.eye();c.look(120,30);
    check(distance(eye,c.eye())<1e-5f,"Looking around in flight must keep the eye stationary");
    const auto before=c.eye();c.move(0,0,1);
    check(std::fabs(distance(before,c.eye())-1.f)<1e-5f,"Flight forward must move one world unit");
    c.fly=false;const auto pivot=c.pivot;c.look(-15,10);
    check(distance(pivot,c.pivot)<1e-6f,"Orbit must keep the target fixed");
    const auto orbitEye=c.eye();c.fly=true;
    check(distance(orbitEye,c.eye())<1e-6f,"Switching flight/orbit must not jump the camera");
    c.look(100000,100000);check(std::isfinite(c.eye().x),"Large mouse motion must keep the camera finite");
    c.reset();check(c.fly&&distance(c.pivot,{})<1e-6f,"Reset must preserve the selected navigation mode");
}
void framePortraitViewport(){
    lab::ViewportCamera c;
    c.frame({2,3,4},.75f,2.f);const float wide=c.distance;
    c.frame({2,3,4},.75f,.5f);
    check(c.distance>wide,"A narrow viewport must move farther away to frame the same mesh");
    check(distance(c.pivot,{2,3,4})<1e-6f,"Focus must center the selected mesh, not the world origin");
}
void flightDirection(){
    // Guard the reported backward-flight symptom at the math/input boundary.
    // Forward input must move in the actual view direction at every yaw/pitch;
    // a released (zero) input must never retain velocity from the last frame.
    for(float yaw:{-3.141592654f,-1.570796327f,0.f,.65f,1.570796327f,3.141592654f})
        for(float pitch:{-1.570796327f,-1.3f,0.f,1.3f,1.570796327f}){
            lab::ViewportCamera c;c.pivot={2,-3,5};c.yaw=yaw;c.pitch=pitch;
            const auto before=c.eye();const auto offset=c.offset();
            const lab::V3 forward{-offset.x/c.distance,-offset.y/c.distance,-offset.z/c.distance};
            c.move(0,0,.25f);const auto after=c.eye();
            const lab::V3 expected{before.x+forward.x*.25f,before.y+forward.y*.25f,before.z+forward.z*.25f};
            check(distance(after,expected)<2e-6f,"W movement must follow view direction, including vertical flight");
            c.move(0,0,-.25f);
            check(distance(c.eye(),before)<2e-6f,"S movement must be the exact reverse of W");
            const auto stopped=c.eye();
            for(int frame=0;frame<100;++frame)c.move(0,0,0);
            check(distance(c.eye(),stopped)==0,"no input must mean no movement or residual backward velocity");
            c.zoom(1);const auto zoomed=c.eye();
            const float along=(zoomed.x-stopped.x)*forward.x+(zoomed.y-stopped.y)*forward.y+(zoomed.z-stopped.z)*forward.z;
            check(along>0,"positive fly zoom must advance along the same view direction as W");
            const auto eye=c.eye();c.look(30,-20);
            check(distance(c.eye(),eye)<2e-6f,"looking after movement must not add backward translation");
        }
}
void signedAxisViews(){
    lab::ViewportCamera c;c.pivot={1,2,3};c.distance=4;
    const lab::V3 directions[]={{1,0,0},{0,1,0},{0,0,1},{-1,0,0},{0,-1,0},{0,0,-1}};
    for(int axis=0;axis<6;++axis){
        const auto pivot=c.pivot;const auto d=c.distance;const auto mode=c.fly;
        c.alignAxis(axis);
        const auto off=c.offset();const auto expected=directions[axis];
        check(distance({off.x/d,off.y/d,off.z/d},expected)<1e-6f,"signed axis points at requested side");
        check(distance(c.pivot,pivot)==0&&c.distance==d&&c.fly==mode,"axis alignment preserves pivot, distance and mode");
        const auto up=c.up();
        const float dot=up.x*off.x+up.y*off.y+up.z*off.z;
        check(std::isfinite(dot)&&std::fabs(dot)<1e-5f,"vertical signed views retain finite orthogonal camera up");
        const auto before=c.eye();c.move(0,0,1);
        check(distance(c.eye(),{before.x-expected.x,before.y-expected.y,before.z-expected.z})<1e-6f,"signed views retain forward-flight convention");
        c.pivot=pivot;
    }
    const auto eye=c.eye();c.alignAxis(6);check(distance(eye,c.eye())==0,"invalid axis does not change view");
}
void diffuseAlpha(){
    lab::Image im;im.w=im.h=4;im.pixels.assign(16,{70,100,150,255});
    auto info=lab::InspectAlpha(im);
    check(lab::ResolveAlpha(info,true,lab::AlphaPreview::Automatic)==lab::AlphaMode::Opaque,"opaque textures must retain depth writes");
    im.pixels[0].a=0;info=lab::InspectAlpha(im);
    check(lab::ResolveAlpha(info,true,lab::AlphaPreview::Automatic)==lab::AlphaMode::Mask,"binary alpha must use cutout rendering");
    im.pixels[1].a=128;info=lab::InspectAlpha(im);
    check(lab::ResolveAlpha(info,true,lab::AlphaPreview::Automatic)==lab::AlphaMode::Blend,"fractional alpha must use blending");
    check(lab::ResolveAlpha(info,false,lab::AlphaPreview::Blend)==lab::AlphaMode::Opaque,"alpha checkbox must override automatic and forced modes");
    check(lab::ResolveAlpha(info,true,lab::AlphaPreview::Mask)==lab::AlphaMode::Mask,"mask override must remain available for antialiased foliage");
    lab::GrayImage opacity;opacity.w=opacity.h=2;opacity.pixels={0,255,0,255};
    check(lab::SampleOpacity(opacity,.25f,.25f)==0&&lab::SampleOpacity(opacity,.75f,.25f)==1,"picking must distinguish holes and solid texels");
    check(std::fabs(lab::SampleOpacity(opacity,.5f,.5f)-.5f)<1e-6f,"picking must match bilinear alpha filtering");
    check(lab::SampleOpacity(opacity,1.25f,-.75f)==0,"alpha picking must wrap tiled UVs");
    auto bytes=lab::EncodeBC3DDS(im,{im});std::string error;auto decoded=lab::DecodeDDS(bytes,error);
    check(decoded.valid()&&decoded.pixels[0].a==0&&std::abs(int(decoded.pixels[1].a)-128)<=20,"DXT5 alpha must survive decoding");
    auto write=[&](size_t offset,uint32_t value){for(int i=0;i<4;++i)bytes[offset+size_t(i)]=uint8_t(value>>(i*8));};
    bytes.resize(136);write(84,0x31545844);bytes[128]=bytes[129]=0;bytes[130]=bytes[131]=255;write(132,0xffffffff);
    decoded=lab::DecodeDDS(bytes,error);check(decoded.valid()&&lab::InspectAlpha(decoded).invisible,"DXT1 transparent selector must produce holes");
    bytes.resize(144);write(84,0x33545844);for(size_t i=128;i<136;++i)bytes[i]=0x80;bytes[136]=bytes[137]=0;bytes[138]=bytes[139]=255;write(140,0);
    decoded=lab::DecodeDDS(bytes,error);check(decoded.valid()&&decoded.pixels[0].a==0&&decoded.pixels[1].a==136,"DXT3 explicit alpha must be retained");
}

}
int main(){
    try{cameraNavigation();framePortraitViewport();flightDirection();signedAxisViews();diffuseAlpha();
        std::cout<<"PASS: camera, framing, six signed axis views, diffuse alpha\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
