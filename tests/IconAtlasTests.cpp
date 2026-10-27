#include "IconAtlas.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;
using namespace lab;
namespace {
Image image(uint32_t w,uint32_t h,Pixel color={0,0,0,0}) {return {w,h,std::vector<Pixel>(size_t(w)*h,color)};}
bool same(Pixel a,Pixel b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
void write(const fs::path& path,const std::vector<uint8_t>& bytes) {
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));assert(f.good());
}
std::vector<uint8_t> read(const fs::path& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
}
int main() {
    std::string error;
    IconGrid grid;
    auto atlas=image(1024,1024,{1,2,3,255});
    assert(grid.columns(atlas)==20&&grid.rows(atlas)==20);
    assert(grid.validate(atlas,{19,19,1,1},error));
    for(auto rect:{IconRect{-1,0,1,1},IconRect{0,0,0,1},IconRect{20,0,1,1},IconRect{19,19,2,1},IconRect{0,0,INT32_MAX,1}})
        assert(!grid.validate(atlas,rect,error));
    auto partial=image(150,120);
    assert(grid.columns(partial)==3&&grid.rows(partial)==2);
    assert(!grid.validate(partial,{0,1,1,2},error));
    IconGrid custom;custom.cellPixels=10;
    assert(custom.columns(partial)==15&&custom.rows(partial)==12);
    custom.cellPixels=0;assert(!custom.validate(partial,{0,0,1,1},error));
    auto icon=image(100,50,{40,80,120,128});icon.pixels[0]={8,9,10,0};
    assert(ReplaceIcon(atlas,{2,3,2,1},grid,icon,error));
    for(uint32_t y=0;y<atlas.h;++y)for(uint32_t x=0;x<atlas.w;++x) {
        const auto p=atlas.pixels[size_t(y)*atlas.w+x];
        if(x>=100&&x<200&&y>=150&&y<200)assert(same(p,icon.pixels[size_t(y-150)*100+x-100]));
        else assert(same(p,{1,2,3,255}));
    }
    auto cropped=CropIcon(atlas,{2,3,2,1},grid,error);
    assert(cropped.w==100&&cropped.h==50);
    for(size_t i=0;i<icon.pixels.size();++i)assert(same(cropped.pixels[i],icon.pixels[i]));
    assert(!ReplaceIcon(atlas,{0,0,1,1},grid,icon,error));
    auto bytes=EncodeIconAtlasDDS(atlas);
    auto decoded=DecodeDDS(bytes,error);assert(decoded.valid()&&decoded.w==1024&&decoded.h==1024);
    assert(bytes.size()==128+1024*1024*4);
    for(size_t i=0;i<atlas.pixels.size();++i)assert(same(atlas.pixels[i],decoded.pixels[i]));
    const auto mips=DecodeDDSMipChain(bytes,error);assert(mips.size()==1);
    // Alpha bounds remove capture background; preserve silhouette ratio and padding.
    auto source=image(80,80,{250,0,250,0});
    for(unsigned y=30;y<50;++y)for(unsigned x=10;x<70;++x)source.pixels[y*80+x]={100,150,200,255};
    auto fit=FitIconSnapshot(source,150,50,4,error);assert(fit.valid()&&fit.w==150&&fit.h==50);
    assert(fit.pixels[0].a==0);assert(fit.pixels[25*150+75].a==255);
    unsigned xmin=150,xmax=0,ymin=50,ymax=0;
    for(unsigned y=0;y<50;++y)for(unsigned x=0;x<150;++x)if(fit.pixels[y*150+x].a) {
        xmin=std::min(xmin,x);xmax=std::max(xmax,x);ymin=std::min(ymin,y);ymax=std::max(ymax,y);
    }
    assert(xmax-xmin+1==126&&ymax-ymin+1==42&&ymin==4&&xmin==12);
    // Downsampling uses associated color: transparent purple cannot darken red edges.
    auto fringe=image(4,2,{0,0,255,0});
    fringe.pixels[0]={255,0,0,255};fringe.pixels[3]={255,0,0,255};
    fringe.pixels[4]={255,0,0,255};fringe.pixels[7]={255,0,0,255};
    auto small=FitIconSnapshot(fringe,2,1,0,error);assert(small.valid());
    for(auto p:small.pixels)assert(p.r==255&&p.g==0&&p.b==0&&p.a==128);
    auto large=FitIconSnapshot(fringe,16,8,0,error);assert(large.valid());
    for(auto p:large.pixels)if(p.a)assert(p.r==255&&p.g==0&&p.b==0);
    assert(!FitIconSnapshot(image(2,2),50,50,0,error).valid());
    assert(!FitIconSnapshot(source,50,50,25,error).valid());
    assert(FitIconSnapshot(source,1025,50,0,error).valid());
    assert(!FitIconSnapshot(source,16385,50,0,error).valid());
    assert(!FitIconSnapshot(source,8192,8192,0,error).valid());
    assert(!FitIconSnapshot(source,50,50,-1,error).valid());
    // Texture dimensions do not change cell size or cap logical coordinates at20.
    auto big=image(2048,3072,{4,5,6,255});
    assert(grid.columns(big)==40&&grid.rows(big)==61);
    assert(grid.validate(big,{39,60,1,1},error));
    assert(!grid.validate(big,{40,60,1,1},error));
    assert(!grid.validate(big,{39,61,1,1},error));
    auto edgeIcon=image(50,50,{90,100,110,64});
    assert(ReplaceIcon(big,{39,60,1,1},grid,edgeIcon,error));
    assert(same(big.pixels[size_t(3000)*2048+1950],edgeIcon.pixels[0]));
    // The remainder strips (48px right,22px bottom) are never interpreted as cells.
    assert(same(big.pixels[size_t(3000)*2048+2000],{4,5,6,255}));
    assert(same(big.pixels[size_t(3050)*2048+1950],{4,5,6,255}));
    auto largeIcon=FitIconSnapshot(source,1500,100,2,error);assert(largeIcon.valid());
    assert(ReplaceIcon(big,{0,0,30,2},grid,largeIcon,error));
    auto bigBytes=EncodeIconAtlasDDS(big);auto bigDecoded=DecodeDDS(bigBytes,error);
    assert(bigDecoded.valid()&&bigDecoded.w==2048&&bigDecoded.h==3072);
    assert(same(bigDecoded.pixels[size_t(3000)*2048+1950],edgeIcon.pixels[0]));
    assert(same(bigDecoded.pixels.back(),{4,5,6,255}));
    auto veryWide=image(4096,512,{7,8,9,255});assert(grid.columns(veryWide)==81&&grid.rows(veryWide)==10);
    assert(grid.validate(veryWide,{80,9,1,1},error));
    assert(ReplaceIcon(veryWide,{80,9,1,1},grid,edgeIcon,error));
    assert(same(veryWide.pixels[size_t(499)*4096+4049],edgeIcon.pixels.back()));
    assert(same(veryWide.pixels[size_t(500)*4096+4049],{7,8,9,255}));
    auto maximumSide=image(16384,50,{12,13,14,255});assert(grid.columns(maximumSide)==327);
    assert(grid.validate(maximumSide,{326,0,1,1},error));
    auto maximumDDS=EncodeIconAtlasDDS(maximumSide);auto maximumDecoded=DecodeDDS(maximumDDS,error);
    assert(maximumDecoded.valid()&&maximumDecoded.w==16384&&maximumDecoded.h==50);
    assert(same(maximumDecoded.pixels.back(),{12,13,14,255}));
    const auto dir=fs::temp_directory_path()/("weaponlab-atlas-test-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);const auto path=dir/"atlas.dds";
    const auto original=EncodeIconAtlasDDS(image(200,100,{8,9,10,255}));write(path,original);
    IconAtlasDocument document;assert(document.load(path,error)&&!document.dirty());
    assert(document.replace({1,0,2,1},icon,error)&&document.dirty());
    assert(document.save(error)&&!document.dirty());
    assert(read(document.lastBackupPath())==original);
    const auto firstBackup=document.lastBackupPath();
    auto saved=read(path);assert(saved!=original);
    assert(document.save(error)&&document.lastBackupPath()==firstBackup);
    auto blank=image(100,50);
    assert(document.replace({1,0,2,1},blank,error));
    document.discard();assert(!document.dirty());
    assert(same(document.image().pixels[50],{8,9,10,0}));
    assert(document.replace({1,0,2,1},blank,error));
    write(path,original);assert(!document.save(error)&&document.dirty());assert(read(path)==original);
    write(path,saved);assert(document.save(error));assert(document.lastBackupPath()!=firstBackup);
    assert(read(firstBackup)==original);
    auto outside=DecodeDDS(read(path),error);assert(same(outside.pixels[199],{8,9,10,255}));
    assert(same(outside.pixels[50],{0,0,0,0}));
    assert(!document.load(dir/"missing.dds",error));assert(document.loaded());
    fs::remove_all(dir);
    std::cout<<"IconAtlas tests passed\n";
}
