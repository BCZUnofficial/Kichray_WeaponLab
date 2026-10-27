#include "IconAtlas.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace lab {
namespace {
constexpr uint64_t MaxPixels=16u*1024u*1024u;
constexpr size_t MaxFileBytes=256u*1024u*1024u;
void put32(std::vector<uint8_t>& bytes,uint32_t value) {
    for(int n=0;n<4;++n)bytes.push_back(uint8_t(value>>(8*n)));
}
bool readFile(const std::filesystem::path& path,std::vector<uint8_t>& bytes,std::string& error) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file){error="Не удалось открыть DDS: "+path.u8string();return false;}
    const auto length=file.tellg();
    if(length<0||uint64_t(length)>MaxFileBytes){error="Недопустимый размер файла атласа DDS.";return false;}
    std::vector<uint8_t> next(static_cast<size_t>(length));
    file.seekg(0);
    if(!next.empty())file.read(reinterpret_cast<char*>(next.data()),std::streamsize(next.size()));
    if(!file){error="Не удалось полностью прочитать DDS.";return false;}
    bytes=std::move(next);return true;
}
std::filesystem::path temporaryPath(const std::filesystem::path& path) {
    static std::atomic<uint64_t> sequence{0};
    const auto tick=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto result=path;result+=std::string(".weaponlab.tmp.")+std::to_string(tick)+"."+std::to_string(++sequence);
    return result;
}
bool writeExclusive(const std::filesystem::path& path,const std::vector<uint8_t>& bytes,std::string& error) {
#ifdef _WIN32
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error="Не удалось создать временный DDS.";return false;}
    DWORD written=0;
    const bool okay=bytes.size()<=MAXDWORD&&WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
    CloseHandle(file);
#else
    const int file=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
    if(file<0){error="Не удалось создать временный DDS.";return false;}
    size_t at=0;
    while(at<bytes.size()) {
        const auto count=::write(file,bytes.data()+at,bytes.size()-at);
        if(count<=0)break;
        at+=size_t(count);
    }
    const bool okay=at==bytes.size()&&::fsync(file)==0;
    ::close(file);
#endif
    if(!okay){std::error_code ec;std::filesystem::remove(path,ec);error="Ошибка записи временного DDS; исходный атлас не изменён.";}
    return okay;
}
bool replaceAtomic(const std::filesystem::path& staged,const std::filesystem::path& path,std::string& error) {
#ifdef _WIN32
    if(!MoveFileExW(staged.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error="Не удалось заменить DDS (возможно, файл открыт другой программой).";return false;
    }
#else
    std::error_code ec;std::filesystem::rename(staged,path,ec);
    if(ec){error="Не удалось заменить DDS: "+ec.message();return false;}
#endif
    return true;
}
struct Associated {double r=0,g=0,b=0,a=0;};
void sample(Associated& out,const Pixel& p,double weight) {
    out.a+=p.a*weight;
    const double scale=p.a*weight;
    out.r+=p.r*scale;out.g+=p.g*scale;out.b+=p.b*scale;
}
Pixel finish(Associated sum,double totalWeight) {
    if(totalWeight<=0||sum.a<=0)return {0,0,0,0};
    auto byte=[](double x){return uint8_t(std::clamp(std::lround(x),0l,255l));};
    const auto alpha=byte(sum.a/totalWeight);
    if(alpha==0)return {0,0,0,0};
    return {byte(sum.r/sum.a),byte(sum.g/sum.a),byte(sum.b/sum.a),alpha};
}
bool samePixels(const Image& a,const Image& b) {
    if(a.w!=b.w||a.h!=b.h||a.pixels.size()!=b.pixels.size())return false;
    for(size_t i=0;i<a.pixels.size();++i) {
        const auto p=a.pixels[i],q=b.pixels[i];
        if(p.r!=q.r||p.g!=q.g||p.b!=q.b||p.a!=q.a)return false;
    }
    return true;
}
}
int IconGrid::columns(const Image& atlas) const {
    return atlas.valid()&&cellPixels>0?int(std::min(uint32_t(INT32_MAX),atlas.w/uint32_t(cellPixels))):0;
}
int IconGrid::rows(const Image& atlas) const {
    return atlas.valid()&&cellPixels>0?int(std::min(uint32_t(INT32_MAX),atlas.h/uint32_t(cellPixels))):0;
}
bool IconGrid::validate(const Image& atlas,const IconRect& rect,std::string& error) const {
    error.clear();
    if(!atlas.valid()){error="Атлас иконок не загружен.";return false;}
    if(cellPixels<=0||cellPixels>1024){error="Размер клетки должен быть от 1 до 1024 пикселей.";return false;}
    const int cols=columns(atlas),rowCount=rows(atlas);
    if(rect.x<0||rect.y<0||rect.w<=0||rect.h<=0||rect.x>=cols||rect.y>=rowCount||rect.w>cols-rect.x||rect.h>rowCount-rect.y) {
        error="Прямоугольник иконки выходит за границы целых клеток атласа.";return false;
    }
    return true;
}
Image CropIcon(const Image& atlas,const IconRect& rect,const IconGrid& grid,std::string& error) {
    if(!grid.validate(atlas,rect,error))return {};
    Image icon;icon.w=uint32_t(rect.w*grid.cellPixels);icon.h=uint32_t(rect.h*grid.cellPixels);
    icon.pixels.resize(size_t(icon.w)*icon.h);
    const size_t x=size_t(rect.x)*grid.cellPixels,y=size_t(rect.y)*grid.cellPixels;
    for(size_t row=0;row<icon.h;++row)
        std::copy_n(atlas.pixels.begin()+(y+row)*atlas.w+x,icon.w,icon.pixels.begin()+row*icon.w);
    return icon;
}
Image FitIconSnapshot(const Image& source,uint32_t width,uint32_t height,int padding,std::string& error) {
    error.clear();
    if(!source.valid()||uint64_t(source.w)*source.h>MaxPixels){error="Снимок модели отсутствует или слишком велик.";return {};}
    if(width==0||height==0||width>16384||height>16384||uint64_t(width)*height>MaxPixels||padding<0||uint64_t(padding)*2>=width||uint64_t(padding)*2>=height) {
        error="Размер иконки превышает предел 16384 по стороне / 16 мегапикселей либо поля не оставляют места для модели.";return {};
    }
    uint32_t minX=source.w,minY=source.h,maxX=0,maxY=0;
    bool visible=false;
    for(uint32_t y=0;y<source.h;++y)for(uint32_t x=0;x<source.w;++x)if(source.pixels[size_t(y)*source.w+x].a) {
        visible=true;minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);
    }
    if(!visible){error="Снимок полностью прозрачный: поместите модель в кадр.";return {};}
    const uint32_t sw=maxX-minX+1,sh=maxY-minY+1;
    const uint32_t usableW=width-2*uint32_t(padding),usableH=height-2*uint32_t(padding);
    const double scale=std::min(double(usableW)/sw,double(usableH)/sh);
    const uint32_t dw=std::clamp(uint32_t(std::max(1l,std::lround(sw*scale))),1u,usableW);
    const uint32_t dh=std::clamp(uint32_t(std::max(1l,std::lround(sh*scale))),1u,usableH);
    Image result;result.w=width;result.h=height;result.pixels.assign(size_t(width)*height,Pixel{0,0,0,0});
    const uint32_t startX=(width-dw)/2,startY=(height-dh)/2;
    const bool downsample=dw<sw||dh<sh;
    for(uint32_t y=0;y<dh;++y)for(uint32_t x=0;x<dw;++x) {
        Associated sum;double weights=0;
        if(downsample) {
            const double left=double(x)*sw/dw,right=double(x+1)*sw/dw;
            const double top=double(y)*sh/dh,bottom=double(y+1)*sh/dh;
            const uint32_t xEnd=std::min(sw,uint32_t(std::ceil(right))),yEnd=std::min(sh,uint32_t(std::ceil(bottom)));
            for(uint32_t iy=uint32_t(top);iy<yEnd;++iy)for(uint32_t ix=uint32_t(left);ix<xEnd;++ix) {
                const double weight=std::max(0.0,std::min(right,double(ix+1))-std::max(left,double(ix)))*
                    std::max(0.0,std::min(bottom,double(iy+1))-std::max(top,double(iy)));
                sample(sum,source.pixels[size_t(minY+iy)*source.w+minX+ix],weight);weights+=weight;
            }
        } else {
            const double fx=std::clamp((double(x)+.5)*sw/dw-.5,0.0,double(sw-1));
            const double fy=std::clamp((double(y)+.5)*sh/dh-.5,0.0,double(sh-1));
            const uint32_t x0=uint32_t(fx),y0=uint32_t(fy),x1=std::min(sw-1,x0+1),y1=std::min(sh-1,y0+1);
            const double wx=fx-x0,wy=fy-y0;
            sample(sum,source.pixels[size_t(minY+y0)*source.w+minX+x0],(1-wx)*(1-wy));
            sample(sum,source.pixels[size_t(minY+y0)*source.w+minX+x1],wx*(1-wy));
            sample(sum,source.pixels[size_t(minY+y1)*source.w+minX+x0],(1-wx)*wy);
            sample(sum,source.pixels[size_t(minY+y1)*source.w+minX+x1],wx*wy);weights=1;
        }
        result.pixels[size_t(startY+y)*width+startX+x]=finish(sum,weights);
    }
    return result;
}
bool ReplaceIcon(Image& atlas,const IconRect& rect,const IconGrid& grid,const Image& icon,std::string& error) {
    if(!grid.validate(atlas,rect,error))return false;
    if(!icon.valid()||icon.w!=uint32_t(rect.w*grid.cellPixels)||icon.h!=uint32_t(rect.h*grid.cellPixels)) {
        error="Размер снимка не совпадает с выделенными клетками. Сначала создайте снимок под этот размер.";return false;
    }
    const size_t x=size_t(rect.x)*grid.cellPixels,y=size_t(rect.y)*grid.cellPixels;
    for(size_t row=0;row<icon.h;++row)
        std::copy_n(icon.pixels.begin()+row*icon.w,icon.w,atlas.pixels.begin()+(y+row)*atlas.w+x);
    return true;
}
std::vector<uint8_t> EncodeIconAtlasDDS(const Image& image) {
    if(!image.valid()||image.w>16384||image.h>16384||uint64_t(image.w)*image.h>MaxPixels)return {};
    std::vector<uint8_t> bytes;bytes.reserve(128+image.pixels.size()*4);
    put32(bytes,0x20534444);put32(bytes,124);put32(bytes,0x100fu);
    put32(bytes,image.h);put32(bytes,image.w);put32(bytes,image.w*4);put32(bytes,0);put32(bytes,1);
    for(int i=0;i<11;++i)put32(bytes,0);
    put32(bytes,32);put32(bytes,0x41);put32(bytes,0);put32(bytes,32);
    put32(bytes,0x00ff0000);put32(bytes,0x0000ff00);put32(bytes,0x000000ff);put32(bytes,0xff000000);
    put32(bytes,0x1000);for(int i=0;i<4;++i)put32(bytes,0);
    for(const auto p:image.pixels){bytes.push_back(p.b);bytes.push_back(p.g);bytes.push_back(p.r);bytes.push_back(p.a);}
    return bytes;
}
bool IconAtlasDocument::load(const std::filesystem::path& path,std::string& error) {
    error.clear();std::vector<uint8_t> bytes;
    if(!readFile(path,bytes,error))return false;
    Image next=DecodeDDS(bytes,error);if(!next.valid())return false;
    path_=path;originalBytes_=std::move(bytes);image_=std::move(next);originalImage_=image_;dirty_=false;backup_.clear();return true;
}
bool IconAtlasDocument::replace(const IconRect& rect,const Image& icon,std::string& error) {
    if(!ReplaceIcon(image_,rect,grid,icon,error))return false;
    dirty_=!samePixels(image_,originalImage_);return true;
}
void IconAtlasDocument::discard() {image_=originalImage_;dirty_=false;}
bool IconAtlasDocument::save(std::string& error) {
    error.clear();
    if(!loaded()||path_.empty()){error="Атлас не загружен.";return false;}
    if(!dirty_)return true;
    std::vector<uint8_t> current;
    if(!readFile(path_,current,error))return false;
    if(current!=originalBytes_){error="DDS изменён вне WeaponLab. Перезагрузите атлас перед сохранением; чужие изменения не перезаписаны.";return false;}
    auto bytes=EncodeIconAtlasDDS(image_);if(bytes.empty()){error="Не удалось сформировать DDS.";return false;}
    const auto staged=temporaryPath(path_);
    if(!writeExclusive(staged,bytes,error))return false;
    auto cleanup=[&](){std::error_code ec;std::filesystem::remove(staged,ec);};
    std::filesystem::path backup;
    bool copied=false;
    for(unsigned i=0;i<10000;++i) {
        backup=path_;backup+=std::string(".weaponlab.bak")+(i?"."+std::to_string(i):"");
        std::error_code ec;
        if(std::filesystem::copy_file(path_,backup,std::filesystem::copy_options::none,ec)){copied=true;break;}
        if(ec!=std::errc::file_exists){error="Не удалось создать резервную копию DDS: "+ec.message();break;}
    }
    if(!copied){if(error.empty())error="Слишком много резервных копий DDS.";cleanup();return false;}
    // Recheck after backup creation; changing the file in another editor is a conflict.
    if(!readFile(path_,current,error)||current!=originalBytes_) {
        if(error.empty())error="DDS изменён во время сохранения. Исходный файл не перезаписан.";
        cleanup();return false;
    }
    if(!replaceAtomic(staged,path_,error)){cleanup();return false;}
    backup_=std::move(backup);originalBytes_=std::move(bytes);originalImage_=image_;dirty_=false;return true;
}
}
