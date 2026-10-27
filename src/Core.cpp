#include "Core.h"
#include <algorithm>
#include <cmath>

namespace lab {
namespace {
struct SampleWeight {uint32_t index;float weight;};
using AxisWeights=std::vector<std::vector<SampleWeight>>;
AxisWeights resampleWeights(uint32_t source,uint32_t target){
 AxisWeights weights(target);
 for(uint32_t i=0;i<target;++i){
  if(target<source){
   const double begin=double(i)*source/target,end=double(i+1)*source/target;
   for(uint32_t j=uint32_t(std::floor(begin));j<std::min(source,uint32_t(std::ceil(end)));++j){const double overlap=std::min(end,double(j+1))-std::max(begin,double(j));if(overlap>0)weights[i].push_back({j,float(overlap/(end-begin))});}
  }else{
   const double center=(double(i)+.5)*source/target-.5;const int left=int(std::floor(center));const float t=float(center-left);
   const uint32_t a=uint32_t(std::clamp(left,0,int(source)-1)),b=uint32_t(std::clamp(left+1,0,int(source)-1));
   if(a==b)weights[i].push_back({a,1});else{weights[i].push_back({a,1-t});weights[i].push_back({b,t});}
  }
 }
 return weights;
}
Image resizeImageExact(const Image& src,uint32_t w,uint32_t h){
 if(!src.valid()||!w||!h)return {};
 if(src.w==w&&src.h==h)return src;
 Image out;out.w=w;out.h=h;out.pixels.resize(size_t(w)*h);
 const auto xs=resampleWeights(src.w,w),ys=resampleWeights(src.h,h);
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){double sum[4]={};for(const auto yy:ys[y])for(const auto xx:xs[x]){const Pixel p=src.pixels[size_t(yy.index)*src.w+xx.index];const double weight=double(xx.weight)*yy.weight;sum[0]+=p.r*weight;sum[1]+=p.g*weight;sum[2]+=p.b*weight;sum[3]+=p.a*weight;}auto byte=[](double v){return uint8_t(std::clamp(v,0.,255.)+.5);};out.pixels[size_t(y)*w+x]={byte(sum[0]),byte(sum[1]),byte(sum[2]),byte(sum[3])};}
 return out;
}
}

Image ResizeImageMax(const Image& src,uint32_t maxDimension){
 if(!src.valid()||!maxDimension)return {};
 const uint32_t biggest=std::max(src.w,src.h);if(biggest<=maxDimension)return src;
 const float k=float(maxDimension)/float(biggest);const uint32_t w=std::max(1u,uint32_t(std::lround(src.w*k))),h=std::max(1u,uint32_t(std::lround(src.h*k)));
 return resizeImageExact(src,w,h);
}
Image DefaultChecker(uint32_t w){Image im;im.w=im.h=w;im.pixels.resize(size_t(w)*w);for(uint32_t y=0;y<w;++y)for(uint32_t x=0;x<w;++x){int check=((x/32)^(y/32))&1;uint8_t k=uint8_t(check?166:104);im.pixels[size_t(y)*w+x]={uint8_t(k+18),k,uint8_t(k-7),255};}return im;}
Mesh MakeCube(){Mesh m;MeshPart part;struct Face{V3 n,p[4];};const Face faces[]={{{0,0,1},{{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}}},{{0,0,-1},{{1,-1,-1},{-1,-1,-1},{-1,1,-1},{1,1,-1}}},{{1,0,0},{{1,-1,1},{1,-1,-1},{1,1,-1},{1,1,1}}},{{-1,0,0},{{-1,-1,-1},{-1,-1,1},{-1,1,1},{-1,1,-1}}},{{0,1,0},{{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}}},{{0,-1,0},{{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}}};
 const float uv[4][2]={{0,1},{1,1},{1,0},{0,0}};for(auto& f:faces){uint32_t o=uint32_t(part.vertices.size());for(int k=0;k<4;++k)part.vertices.push_back({f.p[k],f.n,uv[k][0],uv[k][1]});for(int k:{0,1,2,0,2,3})part.indices.push_back(o+k);}m.parts.push_back(std::move(part));m.sourceParts=1;return m;}

}
