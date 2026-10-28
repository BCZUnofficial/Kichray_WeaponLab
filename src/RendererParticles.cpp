#include "Renderer.h"
#include <cstring>
#include <cmath>

namespace lab {
using namespace DirectX;
namespace {
XMMATRIX matrix(const HudMatrix& m){XMFLOAT4X4 f;std::memcpy(&f,m.data(),sizeof(f));return XMLoadFloat4x4(&f);}
V3 point(const XMMATRIX& m,V3 p){XMFLOAT3 f;XMStoreFloat3(&f,XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),m));return {f.x,f.y,f.z};}
V3 direction(const XMMATRIX& m,V3 p){XMFLOAT3 f;XMStoreFloat3(&f,XMVector3TransformNormal(XMVectorSet(p.x,p.y,p.z,0),m));return {f.x,f.y,f.z};}
V3 plus(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}V3 times(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float magnitude(V3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}V3 unit(V3 v){float l=magnitude(v);return l>1e-6f?times(v,1/l):V3{1,0,0};}
struct SpriteVertex {XMFLOAT3 position;XMFLOAT2 uv;XMFLOAT4 color;};
}
XMMATRIX Renderer::rawModelWorld()const{
    if(hudView_)return matrix(hudTransform_);
    return XMMatrixTranslation(-modelCenter_.x,-modelCenter_.y,-modelCenter_.z)*XMMatrixScaling(modelScale_,modelScale_,modelScale_)*XMMatrixRotationX(modelPitch_)*XMMatrixRotationY(modelYaw_);
}
bool Renderer::BoneTransform(const std::string& name,HudMatrix& out)const{
    for(size_t i=0;i<bones_.size();++i)if(bones_[i].name==name&&i<boneTransforms_.size()){std::memcpy(out.data(),&boneTransforms_[i],sizeof(XMFLOAT4X4));return true;}return false;
}
bool Renderer::ProjectPoint(V3 p,float& x,float& y)const{
    if(!width_||!height_)return false;auto eye=camera_.eye(),up=camera_.up();
    auto view=hudView_?XMMatrixIdentity():XMMatrixLookAtLH(XMVectorSet(eye.x,eye.y,eye.z,1),XMVectorSet(camera_.pivot.x,camera_.pivot.y,camera_.pivot.z,1),XMVectorSet(up.x,up.y,up.z,0));
    auto projection=XMMatrixPerspectiveFovLH(hudView_?XMConvertToRadians(hudFov_):XM_PIDIV4,float(width_)/height_,hudView_?.2f:.005f,100.f);
    XMFLOAT4 q;XMStoreFloat4(&q,XMVector4Transform(XMVectorSet(p.x,p.y,p.z,1),rawModelWorld()*view*projection));
    if(!std::isfinite(q.w)||q.w<=.005f||q.z<0)return false;x=.5f+q.x/q.w*.5f;y=.5f-q.y/q.w*.5f;return x>=0&&x<=1&&y>=0&&y<=1;
}
bool Renderer::initParticles(std::wstring& error){
    if(particleVS_)return true;
    static const char code[]=R"(
cbuffer Transform : register(b0) { row_major float4x4 viewProjection; };
struct V { float3 p:POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct P { float4 p:SV_POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
P VS(V v){P p;p.p=mul(float4(v.p,1),viewProjection);p.uv=v.uv;p.color=v.color;return p;}
Texture2D sprite:register(t0); SamplerState sampler0:register(s0);
float4 PS(P p):SV_TARGET {return sprite.Sample(sampler0,p.uv)*p.color;}
)";
    Microsoft::WRL::ComPtr<ID3DBlob> vs,ps,errors;HRESULT hr=D3DCompile(code,sizeof(code)-1,nullptr,nullptr,nullptr,"VS","vs_5_0",0,0,&vs,&errors);
    if(SUCCEEDED(hr))hr=D3DCompile(code,sizeof(code)-1,nullptr,nullptr,nullptr,"PS","ps_5_0",0,0,&ps,&errors);
    if(FAILED(hr)){error=L"Cannot compile particle preview shaders";return false;}
    D3D11_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,20,D3D11_INPUT_PER_VERTEX_DATA,0}};
    hr=device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&particleVS_);
    if(SUCCEEDED(hr))hr=device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&particlePS_);
    if(SUCCEEDED(hr))hr=device_->CreateInputLayout(layout,3,vs->GetBufferPointer(),vs->GetBufferSize(),&particleInput_);
    D3D11_BUFFER_DESC d{};d.ByteWidth=sizeof(XMFLOAT4X4);d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(SUCCEEDED(hr))hr=device_->CreateBuffer(&d,nullptr,&particleCB_);
    d.ByteWidth=4096*6*sizeof(SpriteVertex);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(SUCCEEDED(hr))hr=device_->CreateBuffer(&d,nullptr,&particleVB_);
    D3D11_BLEND_DESC b{};auto& t=b.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_SRC_ALPHA;t.DestBlend=D3D11_BLEND_ONE;t.BlendOp=D3D11_BLEND_OP_ADD;t.SrcBlendAlpha=D3D11_BLEND_ONE;t.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(SUCCEEDED(hr))hr=device_->CreateBlendState(&b,&particleAlphaAddBlend_);
    t.SrcBlend=D3D11_BLEND_ONE; // Blender_Particle: ADD is ONE+ONE, unlike ALPHA-ADD.
    if(SUCCEEDED(hr))hr=device_->CreateBlendState(&b,&particleAddBlend_);
    if(FAILED(hr)){particleVS_.Reset();particlePS_.Reset();particleInput_.Reset();particleCB_.Reset();particleVB_.Reset();particleAddBlend_.Reset();particleAlphaAddBlend_.Reset();error=L"Cannot create particle preview GPU resources";return false;}return true;
}
bool Renderer::SetParticlePreview(const std::vector<std::pair<ParticleLayer,HudMatrix>>& layers,const std::vector<Image>& images,std::wstring& error){
    particleBatches_.clear();if(layers.empty())return true;if(layers.size()!=images.size()||!initParticles(error))return false;
    std::vector<ParticleBatch> next;for(size_t i=0;i<layers.size();++i){if(layers[i].first.particles.size()>4096){error=L"Too many preview particles";return false;}ParticleBatch batch;batch.layer=layers[i].first;batch.transform=layers[i].second;
        if(!makeTexture(images[i],batch.texture)){error=L"Cannot upload particle texture";return false;}next.push_back(std::move(batch));}particleBatches_=std::move(next);return true;
}
void Renderer::drawParticles(const XMMATRIX& view,const XMMATRIX& projection,V3 eye,V3 up){
    if(particleBatches_.empty()||!particleVS_)return;
    // Placement editor: HUD model, attachment markers and sprites share one
    // projection/depth range. Mixing world FOV with HUD FOV detached the muzzle.
    const D3D11_VIEWPORT viewport={0,0,float(width_),float(height_),0,hudView_?.02f:1.f};context_->RSSetViewports(1,&viewport);
    XMFLOAT4X4 vp;XMStoreFloat4x4(&vp,view*projection);context_->UpdateSubresource(particleCB_.Get(),0,nullptr,&vp,0,0);
    auto* cb=particleCB_.Get();auto* vb=particleVB_.Get();auto* sampler=materialSampler_.Get();UINT stride=sizeof(SpriteVertex),offset=0;
    context_->IASetInputLayout(particleInput_.Get());context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(particleVS_.Get(),nullptr,0);context_->PSSetShader(particlePS_.Get(),nullptr,0);context_->VSSetConstantBuffers(0,1,&cb);context_->PSSetSamplers(0,1,&sampler);
    context_->RSSetState(raster_.Get());context_->OMSetDepthStencilState(alphaDepth_.Get(),0);
    const float sizeScale=hudView_?1.f:modelScale_;
    for(const auto& batch:particleBatches_){
        auto transform=matrix(batch.transform)*(hudView_?XMMatrixIdentity():rawModelWorld());
        const auto& layer=batch.layer;std::vector<const ParticleSample*> sorted;for(const auto& p:layer.particles)sorted.push_back(&p);
        std::stable_sort(sorted.begin(),sorted.end(),[&](const auto* a,const auto* b){return point(transform*view,a->position).z>point(transform*view,b->position).z;});
        std::vector<SpriteVertex> vertices;vertices.reserve(sorted.size()*6);
        for(const auto* ptr:sorted){const auto& p=*ptr;auto pos=point(transform,p.position),velocity=direction(transform,p.velocity);auto look=unit({pos.x-eye.x,pos.y-eye.y,pos.z-eye.z});V3 right=hudView_?V3{1,0,0}:unit(cross(up,look)),top=up;
            const float speed=magnitude(p.velocity);float rx=std::abs(p.size.x)*.5f,ry=std::abs(p.size.y)*.5f;
            if(layer.flags&(1<<18)){rx+=speed*layer.velocityScale.x;ry+=speed*layer.velocityScale.y;}
            if(layer.flags&(1<<15)){
                if(speed>1e-5f&&(layer.flags&(1<<21))){auto forward=unit(velocity);V3 reference=std::abs(forward.y)>.99f?V3{0,0,1}:V3{0,1,0};right=unit(cross(reference,forward));top=unit(cross(forward,right));}
                else if(speed<1e-5f&&(layer.flags&(1<<20))){constexpr float degree=180.f/3.14159265358979323846f;auto rotation=HudOffset({}, {layer.defaultRotation.y*degree,layer.defaultRotation.x*degree,layer.defaultRotation.z*degree});top=unit(direction(transform,HudDirection(rotation,{0,0,1})));right=unit(direction(transform,HudDirection(rotation,{1,0,0})));}
                else {top=unit(speed>1e-5f?velocity:direction(transform,{0,0,1}));right=unit(cross(top,look));}
            }
            auto a=times(plus(times(right,std::cos(p.rotation.x)),times(top,std::sin(p.rotation.x))),rx*sizeScale);
            auto b=times(plus(times(top,std::cos(p.rotation.x)),times(right,-std::sin(p.rotation.x))),ry*sizeScale);
            const uint32_t frame=uint32_t(std::max(0.f,p.frame))%layer.frameCount;float u=(frame%layer.frameColumns)*layer.frameU,v=(frame/layer.frameColumns)*layer.frameV;
            V3 corners[]={plus(plus(pos,times(a,-1)),b),plus(plus(pos,a),b),plus(plus(pos,a),times(b,-1)),plus(plus(pos,times(a,-1)),times(b,-1))};
            XMFLOAT2 uv[]={{u,v},{u+layer.frameU,v},{u+layer.frameU,v+layer.frameV},{u,v+layer.frameV}};
            for(int k:{0,1,2,0,2,3})vertices.push_back({{corners[k].x,corners[k].y,corners[k].z},uv[k],{std::clamp(p.r,0.f,1.f),std::clamp(p.g,0.f,1.f),std::clamp(p.b,0.f,1.f),std::clamp(p.a,0.f,1.f)}});
        }
        if(vertices.empty())continue;D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context_->Map(particleVB_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))continue;
        std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(SpriteVertex));context_->Unmap(particleVB_.Get(),0);
        const auto mode=ParticleShaderBlend(layer.shader);
        const float factor[4]={0,0,0,0};context_->OMSetBlendState(mode==ParticleBlend::Add?particleAddBlend_.Get():mode==ParticleBlend::AlphaAdd?particleAlphaAddBlend_.Get():alphaBlend_.Get(),factor,0xffffffffu);
        auto* texture=batch.texture.Get();context_->PSSetShaderResources(0,1,&texture);context_->Draw(UINT(vertices.size()),0);
    }
    context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);context_->OMSetDepthStencilState(nullptr,0);
}
}
