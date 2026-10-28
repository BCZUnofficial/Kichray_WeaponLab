#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>
#include <stdexcept>
#include <set>
#include "Renderer.h"
#include "WeaponUI.h"
#include "WeaponData.h"
#include "WeaponSimulation.h"
#include "IconAtlas.h"
#include "FlightInput.h"
#include "EngineTexture.h"
#include "Resource.h"
#include "StartupSplash.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
constexpr const wchar_t* AppTitle=L"Kichray WeaponLab 0.4.0";
std::string utf8(const std::wstring& text) {
    if(text.empty())return {};
    int size=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(size_t(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),size,nullptr,nullptr);
    return result;
}
std::wstring wide(const std::string& text,UINT page=CP_UTF8) {
    if(text.empty())return {};
    int size=MultiByteToWideChar(page,0,text.data(),int(text.size()),nullptr,0);
    std::wstring result(size_t(size),L'\0');
    MultiByteToWideChar(page,0,text.data(),int(text.size()),result.data(),size);
    return result;
}
std::wstring lower(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),towlower);return value;
}
std::wstring safeName(std::wstring value) {
    for(auto& c:value)if(c<32||wcschr(L"<>:\"/\\|?*",c))c=L'_';
    while(!value.empty()&&(value.back()==L'.'||value.back()==L' '))value.pop_back();
    if(value.empty()||value==L"."||value==L"..")value=L"material";
    return value;
}
fs::path executableDirectory() {
    std::vector<wchar_t> path(32768);
    DWORD length=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    return length&&length<path.size()?fs::path(path.data()).parent_path():fs::current_path();
}
std::vector<uint8_t> readBytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)return {};
    auto size=input.tellg();
    if(size<=0||size>std::streamoff(512u*1024u*1024u))return {};
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),size);
    return input?bytes:std::vector<uint8_t>{};
}
std::wstring chooseFile(HWND owner,const wchar_t* filter,const wchar_t* extension,bool save=false,const fs::path& initial={}) {
    std::array<wchar_t,32768> path{};
    std::error_code ec;const bool directory=!initial.empty()&&fs::is_directory(initial,ec);
    if(!initial.empty()&&!directory)wcsncpy_s(path.data(),path.size(),initial.c_str(),_TRUNCATE);
    const std::wstring initialDirectory=directory?initial.wstring():initial.parent_path().wstring();
    OPENFILENAMEW options{};options.lStructSize=sizeof(options);options.hwndOwner=owner;
    options.lpstrFilter=filter;options.lpstrFile=path.data();options.nMaxFile=DWORD(path.size());
    options.lpstrInitialDir=initialDirectory.empty()?nullptr:initialDirectory.c_str();
    options.lpstrDefExt=extension;options.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    options.Flags|=save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST;
    bool selected=save?GetSaveFileNameW(&options)!=FALSE:GetOpenFileNameW(&options)!=FALSE;
    return selected?std::wstring(path.data()):std::wstring{};
}
lab::Image loadImage(const fs::path& path,std::string& error) {
    error.clear();
    if(lower(path.extension().wstring())==L".dds")return lab::DecodeDDS(readBytes(path),error);
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf());
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,frame.GetAddressOf());
    UINT w=0,h=0;if(SUCCEEDED(hr))hr=frame->GetSize(&w,&h);
    if(FAILED(hr)||!w||!h||w>4096||h>4096){error="Не удалось прочитать изображение (максимум 4096×4096).";return {};}
    hr=factory->CreateFormatConverter(converter.GetAddressOf());
    if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    lab::Image image;image.w=w;image.h=h;image.pixels.resize(size_t(w)*h);
    if(SUCCEEDED(hr))hr=converter->CopyPixels(nullptr,w*4,UINT(image.pixels.size()*4),reinterpret_cast<BYTE*>(image.pixels.data()));
    if(FAILED(hr)){error="Ошибка преобразования изображения в RGBA.";return {};}
    return image;
}
fs::path stagingPath(const fs::path& path) {
    return path.wstring()+L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
}
bool saveTransparentPNG(const fs::path& path,const lab::Image& image,std::string& error) {
    error.clear();if(!image.valid()||image.w>1024||image.h>1024){error="Неверный размер снимка (максимум 1024×1024).";return false;}
    const fs::path staged=stagingPath(path);
    HRESULT hr=E_FAIL;
    {
        ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
        hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()));
        if(SUCCEEDED(hr))hr=factory->CreateStream(stream.GetAddressOf());
        if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(staged.c_str(),GENERIC_WRITE);
        if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.GetAddressOf());
        if(SUCCEEDED(hr))hr=encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);
        if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(frame.GetAddressOf(),nullptr);
        if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);
        if(SUCCEEDED(hr))hr=frame->SetSize(image.w,image.h);
        WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
        if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
        if(SUCCEEDED(hr)&&!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))hr=E_FAIL;
        if(SUCCEEDED(hr)){
            auto pixels=image.pixels;for(auto& pixel:pixels)std::swap(pixel.r,pixel.b);
            hr=frame->WritePixels(image.h,image.w*4,UINT(pixels.size()*sizeof(lab::Pixel)),reinterpret_cast<BYTE*>(pixels.data()));
        }
        if(SUCCEEDED(hr))hr=frame->Commit();
        if(SUCCEEDED(hr))hr=encoder->Commit();
    } // Release WIC's handle before replacing the chosen destination.
    if(FAILED(hr)||!MoveFileExW(staged.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(staged.c_str());error="Не удалось сохранить PNG. Существующий файл не изменён.";return false;}
    return true;
}
COLORREF colorRef(const float* color) {
    return RGB(int(std::clamp(color[0],0.f,1.f)*255+.5f),int(std::clamp(color[1],0.f,1.f)*255+.5f),int(std::clamp(color[2],0.f,1.f)*255+.5f));
}
RECT monitorWorkArea(HWND window) {
    MONITORINFO info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTOPRIMARY),&info))return info.rcWork;
    return RECT{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
}
fs::path settingsFile() {
    std::array<wchar_t,32768> path{};
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path.data(),DWORD(path.size()));
    auto dir=n&&n<path.size()?fs::path(path.data())/L"KichRay"/L"WeaponLab":executableDirectory();
    std::error_code ec;fs::create_directories(dir,ec);return dir/L"WeaponLab.ini";
}
std::wstring chooseFolder(HWND owner) {
    BROWSEINFOW info{};info.hwndOwner=owner;info.lpszTitle=L"Выберите папку";
    info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    auto item=SHBrowseForFolderW(&info);if(!item)return {};
    std::array<wchar_t,32768> path{};const bool ok=SHGetPathFromIDListW(item,path.data())!=FALSE;
    CoTaskMemFree(item);return ok?std::wstring(path.data()):std::wstring{};
}
fs::path assetPath(const fs::path& root,std::string name,const wchar_t* extension,const std::string& prefix) {
    if(name.size()>=2&&name.front()=='"'&&name.back()=='"')name=name.substr(1,name.size()-2);
    std::replace(name.begin(),name.end(),'\\','/');
    if(name.rfind(prefix,0)==0)name.erase(0,prefix.size());
    const std::string alias=prefix=="meshes/"?"$game_meshes$/":"$game_textures$/";
    if(name.rfind(alias,0)==0)name.erase(0,alias.size());
    fs::path path=fs::u8path(name);
    if(path.empty()||path.is_absolute()||path.has_root_name())return {};
    for(const auto& part:path)if(part==L"..")return {};
    if(path.extension().empty())path+=extension;
    return root/path;
}
bool overlaps(lab::IconRect a,lab::IconRect b) {
    return int64_t(a.x)<int64_t(b.x)+b.w&&int64_t(b.x)<int64_t(a.x)+a.w&&int64_t(a.y)<int64_t(b.y)+b.h&&int64_t(b.y)<int64_t(a.y)+a.h;
}
class App {
    HWND window_=nullptr;
    lab::Renderer renderer_;
    lab::WeaponDatabase data_;
    lab::IconAtlasDocument atlas_;
    lab::WeaponUIState ui_;
    lab::WeaponPathState loadedPaths_;
    lab::FlightInput flight_;
    ComPtr<ID3D11ShaderResourceView> atlasView_,snapshotView_,effectPreviewView_;
    lab::ParticleLibrary particles_;
    lab::HudConfiguration hud_;
    std::map<std::string,lab::Image> particleTextures_;
    std::map<std::string,std::string> rememberedEffects_;
    std::map<std::string,std::vector<bool>> meshVisibility_;
    std::string effectSection_;
    lab::Image snapshot_,fitted_;
    std::string section_,queuedSelection_;
    std::array<std::string,2> catalogSelection_,catalogSearch_,catalogParameterSearch_;
    std::set<std::string> editedIconSections_;
    std::string simulationSection_,lastSimulationAmmo_;
    bool textDraftPending_=false;
    bool ready_=false,running_=true,minimized_=false,viewportFocus_=false,dpiPending_=false;
    float dpi_=1.f;ULONGLONG lastTick_=0;
    enum class Drag { None, Orbit, Pan, Light, Model };
    Drag drag_=Drag::None;int dragButton_=-1;
    std::vector<std::pair<lab::WeaponAction,int>> pending_;
    void stopInput(){drag_=Drag::None;dragButton_=-1;flight_.Suspend();viewportFocus_=false;}
    void fail(const std::string& error){ui_.status=error;}
    const lab::WeaponRecord* current()const{return data_.find(section_);}
    const std::vector<lab::WeaponRecord>& catalogRecords()const {
        return ui_.catalog==lab::WeaponCatalog::Ammunition?data_.ammunition():data_.weapons();
    }
    bool texture(const lab::Image& im,ComPtr<ID3D11ShaderResourceView>& out) {
        if(!im.valid()){out.Reset();return false;}
        D3D11_TEXTURE2D_DESC d{};d.Width=im.w;d.Height=im.h;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA s{im.pixels.data(),im.w*4,0};ComPtr<ID3D11Texture2D> t;ComPtr<ID3D11ShaderResourceView> next;
        if(FAILED(renderer_.Device()->CreateTexture2D(&d,&s,t.GetAddressOf()))||FAILED(renderer_.Device()->CreateShaderResourceView(t.Get(),nullptr,next.GetAddressOf())))return false;
        out=std::move(next);return true;
    }
    void updateAtlas(){
        if(!texture(atlas_.image(),atlasView_)){
            atlasView_.Reset();const std::string error="Не удалось создать предпросмотр атласа на видеокарте. DDS загружен; проверьте размер текстуры и доступную видеопамять.";
            fail(error);ui_.warning+="\n"+error;
        }
        ui_.atlasWidth=int(atlas_.image().w);ui_.atlasHeight=int(atlas_.image().h);
    }
    void refreshRows() {
        if(const auto* record=current()){
            const auto actual=record->kind==lab::WeaponItemKind::Ammunition?lab::WeaponCatalog::Ammunition:lab::WeaponCatalog::Weapons;
            if(actual!=ui_.catalog){
                catalogSearch_[size_t(ui_.catalog)]=ui_.search;catalogParameterSearch_[size_t(ui_.catalog)]=ui_.parameterSearch;
                ui_.catalog=actual;ui_.forceCatalogTab=true;ui_.search=catalogSearch_[size_t(actual)];ui_.parameterSearch=catalogParameterSearch_[size_t(actual)];
            }
        }
        ui_.weapons.clear();ui_.selected=-1;
        const auto root=fs::u8path(loadedPaths_.values[4]);
        for(const auto& w:catalogRecords()) {
            std::error_code ec;const auto model=assetPath(root,w.visual,L".ogf","meshes/");
            ui_.weapons.push_back({w.section,w.label,w.sourceFile.u8string(),model.empty()||!fs::is_regular_file(model,ec)});
            if(w.section==section_)ui_.selected=int(ui_.weapons.size()-1);
        }
    }
    void updateSimulation() {
        lab::SimulationConfig weapon,physics;std::string error;
        if(ui_.selectedAmmoIndex>=0&&size_t(ui_.selectedAmmoIndex)<ui_.ammunition.size())lastSimulationAmmo_=ui_.ammunition[size_t(ui_.selectedAmmoIndex)].section;
        const std::string priorAmmo=lastSimulationAmmo_;
        ui_.ammunition.clear();ui_.simulation={};ui_.simulationAvailable=false;
        if(!current()||ui_.catalog==lab::WeaponCatalog::Ammunition){simulationSection_.clear();ui_.view=lab::WeaponView::Model;return;}
        if(!data_.sectionValues(section_,weapon,error)){ui_.simulation.error=error;if(ui_.view!=lab::WeaponView::Hud)ui_.view=lab::WeaponView::Model;return;}
        ui_.simulationAvailable=lab::IsSimulationFirearm(weapon);
        if(!ui_.simulationAvailable){if(ui_.view!=lab::WeaponView::Hud)ui_.view=lab::WeaponView::Model;return;}
        const auto ammo=data_.ammunitionForWeapon(section_);
        int selected=0;
        for(size_t i=0;i<ammo.size();++i){ui_.ammunition.push_back({ammo[i].section,ammo[i].label+(ammo[i].available?"":" [нет данных]")});if(ammo[i].section==priorAmmo)selected=int(i);}
        ui_.selectedAmmoIndex=selected;
        const auto modes=lab::SimulationFireModes(weapon);
        if(simulationSection_!=section_||std::find(modes.begin(),modes.end(),ui_.selectedFireMode)==modes.end())ui_.selectedFireMode=modes.empty()?1:modes.back();
        simulationSection_=section_;
        if(ammo.empty()) {ui_.simulation.supported=true;ui_.simulation.fireModes=modes;ui_.simulation.error="У оружия не указан доступный ammo_class.";return;}
        const auto& selectedAmmo=ammo[size_t(selected)];
        if(!selectedAmmo.available){ui_.simulation.supported=true;ui_.simulation.fireModes=modes;ui_.simulation.error=selectedAmmo.error;return;}
        std::string physicsError;data_.sectionValues("bullet_manager",physics,physicsError);
        lab::SimulationRequest request;request.weapon=std::move(weapon);request.ammo=selectedAmmo.values;request.bulletManager=std::move(physics);
        request.weaponSection=section_;request.ammoSection=selectedAmmo.section;request.fireMode=ui_.selectedFireMode;
        request.spreadDistance=ui_.spreadDistance;request.trajectoryDistance=ui_.trajectoryDistance;request.condition=ui_.condition;
        request.triggerPause=ui_.triggerPause;request.seed=ui_.seed;ui_.simulation=lab::SimulateWeapon(request);ui_.simulation.ammoName=selectedAmmo.label;
        if(!physicsError.empty())ui_.simulation.warnings.push_back(physicsError);
    }
    void refreshHud(bool rebuild) {
        lab::WeaponConfigValues values;std::string error;
        std::string target;
        if(current()&&ui_.catalog==lab::WeaponCatalog::Weapons&&data_.sectionValues(section_,values,error))target=lab::ConfigString(values,"hud");
        const bool changed=ui_.hudSection!=target;ui_.hudSection=target;ui_.hudAvailable=!target.empty();
        if(target.empty()){ui_.hudParameters.clear();ui_.hudWarning.clear();if(ui_.view==lab::WeaponView::Hud)ui_.view=lab::WeaponView::Model;return;}
        if(rebuild||changed){
            ui_.hudParameters.clear();std::vector<lab::WeaponValue> parameters;
            if(!data_.sectionParameters(target,parameters,error)){ui_.hudWarning=error;return;}
            for(const auto& p:parameters){auto row=lab::MakeWeaponParameter(p.key,p.value,p.sourceFile.u8string()+" ["+p.sourceSection+"]",p.inherited);
                if(row.numbers.size()==3&&(p.key=="position"||p.key=="orientation"||p.key=="fire_point"||p.key=="fire_point2"||p.key=="shell_point"||p.key=="zoom_offset")){
                    row.integer=false;row.step=p.key=="orientation"?.1:.001;row.minimum=p.key=="orientation"?-180:-1;row.maximum=-row.minimum;
                    for(auto n:row.numbers){row.minimum=std::min(row.minimum,n);row.maximum=std::max(row.maximum,n);}}
                ui_.hudParameters.push_back(std::move(row));}
            for(const auto* key:{"fire_point2","shell_point"})if(std::none_of(parameters.begin(),parameters.end(),[&](const auto& p){return p.key==key;})){
                std::string value="0, 0, 0";if(std::string(key)=="fire_point2")for(const auto& p:parameters)if(p.key=="fire_point")value=p.value;
                auto row=lab::MakeWeaponParameter(key,value,"Параметр отсутствует; будет добавлен при редактировании.");row.step=.001;row.minimum=-1;row.maximum=1;row.integer=false;ui_.hudParameters.push_back(std::move(row));}
        }
        lab::HudConfiguration checked;
        if(!lab::ReadWeaponHUD(data_,section_,checked,error))ui_.hudWarning=error;
        else {ui_.hudWarning="Общая секция HUD: изменения действуют на всё оружие, которое на неё ссылается.";hud_=std::move(checked);}
    }
    void refreshEffects() {
        const bool changed=effectSection_!=section_;
        if(changed){ui_.effectPickerOpen=false;ui_.effectCandidate.clear();effectPreviewView_.Reset();ui_.effectSlot=-1;}
        auto old=ui_.effects;ui_.effects.clear();effectSection_=section_;
        if(!current()||ui_.catalog!=lab::WeaponCatalog::Weapons)return;
        lab::WeaponConfigValues v;std::string error;if(!data_.sectionValues(section_,v,error))return;
        for(const auto& slot:lab::WeaponEffectSlots()){
            lab::WeaponEffectRow row;row.key=slot.key;row.label=slot.label;row.point=slot.point;row.name=lab::ConfigString(v,slot.key);row.enabled=!row.name.empty();
            const auto id=section_+"/"+slot.key;if(row.enabled)rememberedEffects_[id]=row.name;row.remembered=rememberedEffects_[id];
            if(!changed)for(const auto& e:old)if(e.key==row.key)row.visible=e.visible;
            ui_.effects.push_back(std::move(row));}
    }
    void reloadEffects() {
        lab::ParticleLibrary next;std::string error;particleTextures_.clear();ui_.effectNames.clear();effectPreviewView_.Reset();ui_.effectCandidate.clear();renderer_.ClearParticlePreview();
        if(!next.load(readBytes(fs::u8path(loadedPaths_.values[6])),error)){particles_=lab::ParticleLibrary{};ui_.particleLibraryStatus="particles.xr: "+error;return;}
        particles_=std::move(next);for(const auto& e:particles_.entries())ui_.effectNames.push_back(e.name);
        ui_.particleLibraryStatus="particles.xr: "+std::to_string(ui_.effectNames.size())+" эффектов и групп.";updateEffectsPreview();
    }
    lab::Image effectTexture(const std::string& name) {
        auto existing=particleTextures_.find(name);if(existing!=particleTextures_.end())return existing->second;
        std::string error;auto path=assetPath(fs::u8path(loadedPaths_.values[5]),name,L".dds","textures/");auto image=path.empty()?lab::Image{}:loadImage(path,error);
        size_t bytes=image.pixels.size()*sizeof(lab::Pixel);for(const auto& pair:particleTextures_)bytes+=pair.second.pixels.size()*sizeof(lab::Pixel);
        if(bytes>128u*1024u*1024u)particleTextures_.clear();particleTextures_[name]=image;return image;
    }
    bool emitterPoint(const std::string& key,lab::V3& point,std::string& error)const {
        if(renderer_.HudView()){
            lab::HudMatrix bone;if(!renderer_.BoneTransform(hud_.fireBone,bone)){error="В HUD нет fire_bone: "+hud_.fireBone;return false;}
            if(key=="shell_point"&&!hud_.hasShellPoint){error="В HUD отсутствует shell_point.";return false;}
            const auto local=key=="shell_point"?hud_.shellPoint:key=="fire_point2"?hud_.firePoint2:hud_.firePoint;point=lab::HudPoint(bone,local);return true;
        }
        lab::WeaponConfigValues values;if(!data_.sectionValues(section_,values,error))return false;auto found=values.find(key);
        if(found==values.end()&&key=="fire_point2")found=values.find("fire_point");std::vector<double> n;
        if(found==values.end()||!lab::ParseWeaponNumbers(found->second,n)||n.size()!=3){error="Нет корректной точки World: "+key;return false;}
        point={float(n[0]),float(n[1]),float(n[2])};return true;
    }
    void updateMarkers() {
        ui_.markers.clear();if(!ui_.hasModel||!ui_.showEmitters||ui_.catalog!=lab::WeaponCatalog::Weapons)return;
        for(const auto* key:{"fire_point","fire_point2","shell_point"}){lab::V3 point;std::string error;if(!emitterPoint(key,point,error))continue;
            lab::WeaponMarker marker;marker.label=key;marker.visible=renderer_.ProjectPoint(point,marker.x,marker.y,true);ui_.markers.push_back(marker);}
    }
    void updateEffectsPreview() {
        renderer_.ClearParticlePreview();ui_.effectStatus.clear();if(!ui_.hasModel||ui_.catalog!=lab::WeaponCatalog::Weapons)return;
        std::vector<std::pair<lab::ParticleLayer,lab::HudMatrix>> layers;std::vector<lab::Image> textures;
        for(const auto& slot:ui_.effects){if(!slot.visible)continue;const auto name=slot.enabled?slot.name:slot.remembered;
            if(name.empty()){ui_.effectStatus+=slot.label+": выберите эффект.\n";continue;}
            lab::V3 point;std::string error;if(!emitterPoint(slot.point,point,error)){ui_.effectStatus+=error+"\n";continue;}
            auto transform=lab::HudIdentity();transform[12]=point.x;transform[13]=point.y;transform[14]=point.z;
            if(renderer_.HudView()){auto model=lab::HudModelTransform(hud_,ui_.hudAiming);transform=lab::HudParticleBasis(model,lab::HudPoint(model,point));}
            const auto snapshot=particles_.snapshot(name,ui_.effectTime);
            for(const auto& w:snapshot.warnings)ui_.effectStatus+=w+"\n";
            for(const auto& layer:snapshot.layers){if(layer.particles.empty())continue;auto texture=effectTexture(layer.texture);if(!texture.valid()){ui_.effectStatus+="Не найдена текстура: "+layer.texture+"\n";continue;}
                layers.emplace_back(layer,transform);textures.push_back(std::move(texture));}
        }
        std::wstring error;if(!renderer_.SetParticlePreview(layers,textures,error))ui_.effectStatus+=utf8(error);
        updateMarkers();
    }
    void previewEffect(int index) {
        if(index<0||size_t(index)>=ui_.effectNames.size())return;ui_.effectCandidate=ui_.effectNames[size_t(index)];effectPreviewView_.Reset();
        auto snapshot=particles_.snapshot(ui_.effectCandidate,ui_.effectTime);ui_.effectStatus="Статичный кадр; фон и масштаб предпросмотра условные.\n";
        for(const auto& w:snapshot.warnings)ui_.effectStatus+=w+"\n";
        size_t count=0;for(const auto& layer:snapshot.layers){count+=layer.particles.size();if(!effectTexture(layer.texture).valid())ui_.effectStatus+="Не найдена текстура: "+layer.texture+"\n";}
        if(count==0)ui_.effectStatus+="В этом кадре нет частиц. Измените время в «Эффектах».\n";
        auto image=lab::ParticleThumbnail(snapshot,[&](const std::string& name){return effectTexture(name);});if(!texture(image,effectPreviewView_))ui_.effectStatus+="Ошибка создания предпросмотра на GPU.";
    }
    bool setEffect(int index,const std::string& name) {
        if(index<0||size_t(index)>=ui_.effects.size()||!current())return false;
        if(!flushPendingParameters()){refreshEffects();return false;}
        const auto key=ui_.effects[size_t(index)].key;std::string error;
        if(!name.empty()&&!particles_.contains(name)){fail("Эффект отсутствует в выбранном particles.xr: "+name);refreshEffects();return false;}
        if(key=="shell_particles"&&!name.empty()){
            lab::WeaponConfigValues v;std::vector<double> n;data_.sectionValues(section_,v,error);
            if(!lab::ParseWeaponNumbers(lab::ConfigString(v,"shell_point"),n)||n.size()!=3){fail("Сначала задайте World shell_point в параметрах оружия.");refreshEffects();return false;}
            if(!ui_.hudSection.empty()&&(!data_.sectionValues(ui_.hudSection,v,error)||!lab::ParseWeaponNumbers(lab::ConfigString(v,"shell_point"),n)||n.size()!=3)){fail("Сначала задайте HUD shell_point: движок читает его при подключённых гильзах.");refreshEffects();return false;}}
        if(!data_.setValue(section_,key,name,error)){fail(error);refreshEffects();return false;}
        refreshRows();refreshCurrent();updateEffectsPreview();ui_.status="Изменён "+key+". Нажмите «Сохранить» для записи LTX.";return true;
    }
    void applyHudPreview() {
        if(!ui_.hasModel||ui_.view!=lab::WeaponView::Hud)return;
        if(!std::isfinite(ui_.cameraFov))ui_.cameraFov=67.5f;if(!std::isfinite(ui_.hudFovFactor))ui_.hudFovFactor=.45f;
        ui_.cameraFov=std::clamp(ui_.cameraFov,5.f,179.f);ui_.hudFovFactor=std::clamp(ui_.hudFovFactor,.1f,1.f);
        std::string error;if(!lab::ReadWeaponHUD(data_,section_,hud_,error)){fail(error);return;}
        renderer_.SetHudView(true,lab::HudModelTransform(hud_,ui_.hudAiming),ui_.cameraFov*ui_.hudFovFactor,ui_.cameraFov);
        std::wstring renderError;
        if(ui_.hudMotionIndex>=0){if(!renderer_.SetAnimationPose(size_t(ui_.hudMotionIndex),ui_.hudMotionTime,true,renderError))fail(utf8(renderError));}
        else if(!renderer_.SetBindPose(&renderError))fail(utf8(renderError));
        lab::HudMatrix bone;if(!renderer_.BoneTransform(hud_.fireBone,bone))ui_.hudWarning="В модели нет fire_bone: "+hud_.fireBone+". Исправьте имя перед запуском игры.";
        updateEffectsPreview();
    }
    void refreshCurrent(bool rebuildParameters=true) {
        const auto* w=current();if(!w){ui_.parameters.clear();ui_.name.clear();ui_.description.clear();ui_.nameId.clear();ui_.descriptionId.clear();ui_.canEditName=false;ui_.canEditDescription=false;refreshHud(true);refreshEffects();clearModel();updateSimulation();return;}
        if(!textDraftPending_){ui_.name=w->name;ui_.description=w->description;}ui_.nameId=w->nameId;ui_.descriptionId=w->descriptionId;
        ui_.canEditName=!w->nameId.empty();ui_.canEditDescription=!w->descriptionId.empty();
        ui_.currentIconX=w->iconX;ui_.currentIconY=w->iconY;ui_.currentIconW=w->iconWidth;ui_.currentIconH=w->iconHeight;
        if(rebuildParameters){
            ui_.parameters.clear();for(const auto& p:w->values)ui_.parameters.push_back(lab::MakeWeaponParameter(p.key,p.value,p.sourceFile.u8string()+" ["+p.sourceSection+"]",p.inherited));
            if(w->kind==lab::WeaponItemKind::Weapon)for(const auto* key:{"shell_point","fire_point2"})if(std::none_of(w->values.begin(),w->values.end(),[&](const auto& p){return p.key==key;})){
                std::string value="0, 0, 0";if(std::string(key)=="fire_point2")for(const auto& p:w->values)if(p.key=="fire_point")value=p.value;
                auto row=lab::MakeWeaponParameter(key,value,"Параметр отсутствует; будет добавлен при редактировании.");row.step=.001;row.minimum=-1;row.maximum=1;row.integer=false;ui_.parameters.push_back(std::move(row));}
            ui_.iconX=w->iconX;ui_.iconY=w->iconY;ui_.iconW=w->iconWidth;ui_.iconH=w->iconHeight;
        }
        refreshHud(rebuildParameters);refreshEffects();updateSimulation();
    }
    void clearModel() {
        for(size_t i=0;i<renderer_.PartCount();++i)renderer_.SetPartVisible(i,false);
        renderer_.ClearParticlePreview();ui_.markers.clear();
        ui_.hasModel=false;ui_.meshes.clear();ui_.modelName.clear();snapshot_={};fitted_={};snapshotView_.Reset();ui_.hasSnapshot=false;
    }
    bool loadModel() {
        if(ui_.hasModel){auto& saved=meshVisibility_[ui_.modelName];saved.clear();for(const auto& p:ui_.meshes)saved.push_back(p.visible);}
        clearModel();const auto* record=current();if(!record)return false;
        const bool hudView=ui_.view==lab::WeaponView::Hud;
        std::string error,visual=record->visual;
        if(hudView){if(!lab::ReadWeaponHUD(data_,section_,hud_,error)){fail(error);return false;}visual=hud_.visual;}
        renderer_.SetHudView(hudView,hudView?lab::HudModelTransform(hud_,ui_.hudAiming):lab::HudIdentity(),ui_.cameraFov*ui_.hudFovFactor,ui_.cameraFov);
        const auto model=assetPath(fs::u8path(loadedPaths_.values[4]),visual,L".ogf","meshes/");
        lab::Mesh mesh;
        if(model.empty()||!lab::LoadOGFMesh(readBytes(model),mesh,error)){fail("Модель "+visual+": "+(error.empty()?"файл не найден":error));return false;}
        if(hudView){const auto refs=mesh.motionRefs;for(const auto& ref:refs){const auto omf=assetPath(fs::u8path(loadedPaths_.values[4]),ref,L".omf","meshes/");std::string motionError;
            if(omf.empty()||!lab::LoadOGFMotions(readBytes(omf),mesh,motionError))ui_.hudWarning+=" Не загружены анимации "+ref+": "+motionError;}}

        const auto root=fs::u8path(loadedPaths_.values[5]);std::vector<lab::PartImages> images;
        std::vector<lab::EngineTextureMetadata> metadata;std::vector<std::vector<lab::Image>> bumpMips,bumpXMips;
        std::vector<std::string> missing;
        std::vector<lab::WeaponMeshRow> meshRows;
        for(const auto& part:mesh.parts)if(part.valid()) {
            lab::PartImages im;lab::EngineTextureMetadata meta;
            meshRows.push_back({"Часть "+std::to_string(meshRows.size()+1)+" · "+part.texture,true});
            const auto file=assetPath(root,part.texture,L".dds","textures/");
            if(!file.empty()){
                im.diffuse=loadImage(file,error);auto thm=file;thm.replace_extension(L".thm");lab::ReadEngineTextureMetadata(readBytes(thm),meta);
            }
            std::vector<lab::Image> mips,xmips;
            if(!file.empty()&&(!meta.hasBumpMode||meta.bumpEnabled)){
                auto base=file;base.replace_extension();fs::path bump=base.wstring()+L"_bump.dds";
                if(meta.bumpEnabled&&!meta.bumpName.empty())bump=assetPath(root,meta.bumpName,L".dds","textures/");
                if(!bump.empty()){
                    auto bumpX=bump.parent_path()/(bump.stem().wstring()+L"#"+bump.extension().wstring());
                    mips=lab::DecodeDDSMipChain(readBytes(bump),error);xmips=lab::DecodeDDSMipChain(readBytes(bumpX),error);
                    if(!mips.empty()&&!xmips.empty()&&mips[0].w==xmips[0].w&&mips[0].h==xmips[0].h){im.bump=mips[0];im.bumpX=xmips[0];im.hasBump=true;}
                }
            }
            if(!im.diffuse.valid()){missing.push_back(part.texture);im.diffuse=lab::DefaultChecker(128);}
            images.push_back(std::move(im));metadata.push_back(meta);bumpMips.push_back(std::move(mips));bumpXMips.push_back(std::move(xmips));
        }
        std::wstring renderError;
        if(!renderer_.SetMesh(mesh,renderError)){fail(utf8(renderError));return false;}
        for(size_t i=0;i<images.size();++i){
            if(!renderer_.SetPartImages(i,images[i])){clearModel();fail("Ошибка загрузки текстур на видеокарту.");return false;}
            renderer_.SetPartMaterialClass(i,metadata[i].material);
            if(images[i].hasBump&&!renderer_.SetPartBumpMipImages(i,bumpMips[i],bumpXMips[i]))ui_.warning="Часть mip-уровней bump не загрузилась; используется основной уровень.";
        }
        renderer_.SkeletonEnabled(false);renderer_.OutlineEnabled(false);renderer_.ClearHover();renderer_.ClearBrushCursor();
        ui_.hasModel=true;ui_.meshes=std::move(meshRows);ui_.modelName=model.u8string();
        const auto prior=meshVisibility_.find(ui_.modelName);if(prior!=meshVisibility_.end()&&prior->second.size()==ui_.meshes.size())for(size_t i=0;i<ui_.meshes.size();++i){ui_.meshes[i].visible=prior->second[i];renderer_.SetPartVisible(i,prior->second[i]);}
        if(hudView){ui_.hudMotions.clear();ui_.hudMotionIndex=-1;ui_.hudMotionTime=0;const auto idle=hud_.idleMotion.substr(0,hud_.idleMotion.find(','));
            for(size_t i=0;i<renderer_.MotionCount();++i){const auto* m=renderer_.MotionAt(i);ui_.hudMotions.push_back(m->name);if(m->name==idle)ui_.hudMotionIndex=int(i);}
            applyHudPreview();}else updateEffectsPreview();
        ui_.status="Загружено: "+record->label+" ["+record->section+"].";
        if(!missing.empty()){ui_.status+=" Не найдены diffuse: ";for(const auto& name:missing)ui_.status+=name+"; ";}
        return true;
    }
    void select(int index) {
        if(index<0||size_t(index)>=ui_.weapons.size())return;
        const auto chosen=ui_.weapons[size_t(index)].section;
        if(!flushPendingParameters()||!flushPendingTexts())return;
        if(!data_.find(chosen))return;
        stopInput();section_=chosen;catalogSelection_[size_t(ui_.catalog)]=chosen;
        ui_.descriptionEdit={};refreshRows();refreshCurrent();loadModel();
    }
    void selectCatalog(int value) {
        if(value<0||value>1||value==int(ui_.catalog))return;
        // Reject a tab switch without losing an invalid parameter/text draft.
        ui_.forceCatalogTab=true;
        if(!flushPendingParameters()||!flushPendingTexts())return;
        const size_t previous=size_t(ui_.catalog);
        catalogSelection_[previous]=section_;catalogSearch_[previous]=ui_.search;
        catalogParameterSearch_[previous]=ui_.parameterSearch;
        stopInput();ui_.catalog=lab::WeaponCatalog(value);ui_.search=catalogSearch_[size_t(value)];
        ui_.parameterSearch=catalogParameterSearch_[size_t(value)];ui_.descriptionEdit={};
        section_=catalogSelection_[size_t(value)];
        const auto& records=catalogRecords();
        if(std::none_of(records.begin(),records.end(),[&](const lab::WeaponRecord& record){return record.section==section_;}))
            section_=records.empty()?std::string{}:records.front().section;
        catalogSelection_[size_t(value)]=section_;
        refreshRows();refreshCurrent();
        if(current())loadModel();
        else ui_.status=ui_.catalog==lab::WeaponCatalog::Ammunition?"Патроны в загруженных конфигах не найдены.":"Оружие в загруженных конфигах не найдено.";
    }
    void defaults() {
        fs::path root=fs::u8path(ui_.paths.values[0]);
        auto leaf=lower(root.filename().wstring());
        if(leaf==L"weapons")root=root.parent_path().parent_path();
        else if(leaf==L"config"||leaf==L"configs")root=root.parent_path();
        ui_.paths.values[0]=root.u8string();
        ui_.paths.values[1]=(root/L"config"/L"weapons").u8string();
        ui_.paths.values[2]=(root/L"config"/L"text"/L"rus"/L"string_table_enc_weapons.xml").u8string();
        ui_.paths.values[3]=(root/L"textures"/L"ui"/L"ui_icon_equipment.dds").u8string();
        ui_.paths.values[4]=(root/L"meshes").u8string();ui_.paths.values[5]=(root/L"textures").u8string();ui_.paths.values[6]=(root/L"particles.xr").u8string();
    }
    void rememberPaths() {
        const auto ini=settingsFile();bool ok=true;
        std::error_code ec;
        if(!fs::exists(ini,ec)){
            // Profile APIs choose Unicode only when the file already has a UTF-16LE BOM.
            std::ofstream initial(ini,std::ios::binary);const char bom[]={char(0xff),char(0xfe)};initial.write(bom,2);initial.close();
            if(!initial){ui_.warning="Не удалось создать файл настроек путей.";return;}
        }
        for(size_t i=0;i<loadedPaths_.values.size();++i)ok=WritePrivateProfileStringW(L"Paths",std::to_wstring(i).c_str(),wide(loadedPaths_.values[i]).c_str(),ini.c_str())&&ok;
        if(!ok)ui_.warning="Не удалось сохранить выбранные пути между запусками.";
    }
    bool saveData() {
        if(!flushPendingParameters()||!flushPendingTexts())return false;
        std::string error;
        for(const auto& section:editedIconSections_){const auto* record=data_.find(section);
            if(!record||!atlas_.grid.validate(atlas_.image(),{record->iconX,record->iconY,record->iconWidth,record->iconHeight},error)){
                fail("Координаты иконки ["+section+"] вне выбранного атласа. Исправьте их перед сохранением. "+error);return false;
            }
        }
        // Atlas first: if a later LTX/XML write fails, report it; never claim an all-file atomic transaction.
        if(atlas_.dirty()&&!atlas_.save(error)){fail(error);return false;}
        if(data_.dirty()&&!data_.save(error)){fail("Конфиги/XML не сохранены: "+error+" Уже сохранённые изменения DDS остаются на диске.");return false;}
        editedIconSections_.clear();ui_.status="Изменения сохранены. Резервные копии находятся рядом с исходными файлами.";return true;
    }
    bool confirmPending() {
        if(!data_.dirty()&&!atlas_.dirty()&&!hasPendingParameters()&&!textDraftPending_)return true;
        stopInput();const int result=MessageBoxW(window_,L"Есть несохранённые изменения конфигов, текстов или атласа.\n\nДа — сохранить; Нет — отбросить; Отмена — продолжить работу.",AppTitle,MB_YESNOCANCEL|MB_ICONQUESTION);
        return result==IDYES?saveData():result==IDNO;
    }
    bool loadPaths(bool confirm=true) {
        stopInput();if(confirm&&!confirmPending())return false;
        lab::WeaponDatabase next;std::string error;
        const fs::path weapons=fs::u8path(ui_.paths.values[1]);
        const auto system=weapons.parent_path()/L"system.ltx";
        std::error_code ec;const auto extra=fs::is_regular_file(system,ec)?system:fs::path{};
        if(!next.load(weapons,fs::u8path(ui_.paths.values[2]),error,extra)){fail(error);ui_.setupOpen=true;return false;}
        lab::IconAtlasDocument nextAtlas;
        if(!nextAtlas.load(fs::u8path(ui_.paths.values[3]),error)){fail("Атлас: "+error);ui_.setupOpen=true;return false;}
        data_=std::move(next);atlas_=std::move(nextAtlas);loadedPaths_=ui_.paths;
        ui_.warning.clear();for(const auto& warning:data_.warnings()){if(ui_.warning.size()>3000){ui_.warning+="\n…";break;}ui_.warning+=warning+"\n";}
        section_.clear();queuedSelection_.clear();catalogSelection_={};catalogSearch_={};catalogParameterSearch_={};lastSimulationAmmo_.clear();simulationSection_.clear();ui_.ammunition.clear();
        ui_.catalog=lab::WeaponCatalog::Weapons;ui_.forceCatalogTab=true;ui_.search.clear();ui_.parameterSearch.clear();ui_.descriptionEdit={};
        editedIconSections_.clear();textDraftPending_=false;ui_.parameters.clear();ui_.name.clear();ui_.description.clear();ui_.nameId.clear();ui_.descriptionId.clear();
        ui_.hudParameters.clear();ui_.hudSection.clear();ui_.hudAvailable=false;ui_.view=lab::WeaponView::Model;ui_.effectPickerOpen=false;ui_.effects.clear();effectSection_.clear();rememberedEffects_.clear();meshVisibility_.clear();renderer_.SetHudView(false);
        clearModel();refreshRows();updateSimulation();updateAtlas();reloadEffects();ui_.setupOpen=false;rememberPaths();
        ui_.status="Найдено оружия: "+std::to_string(data_.weapons().size())+", типов патронов: "+std::to_string(data_.ammunition().size())+". Выберите вкладку и предмет слева.";
        return true;
    }
    void lighting() {
        renderer_.SetLightIntensity(ui_.directedLight?ui_.lightIntensity:0.f);
        renderer_.SetLightOrbitDegrees(ui_.lightAzimuth,ui_.lightElevation);
        renderer_.SetColor(1,colorRef(ui_.lightColor));renderer_.SetColor(2,colorRef(ui_.ambient));
        renderer_.SetEnvironmentLighting({ui_.environment[0],ui_.environment[1],ui_.environment[2]},ui_.hemisphere,ui_.exposure,ui_.glossFactor);
    }
    lab::IconRect selectedRect()const{return {ui_.iconX,ui_.iconY,ui_.iconW,ui_.iconH};}
    bool applyRect() {
        if(!current()||!flushPendingParameters()||!flushPendingTexts())return false;std::string error;
        if(!atlas_.grid.validate(atlas_.image(),selectedRect(),error)){fail(error);return false;}
        const std::array<const char*,4> keys={"inv_grid_x","inv_grid_y","inv_grid_width","inv_grid_height"};
        const std::array<int,4> values={ui_.iconX,ui_.iconY,ui_.iconW,ui_.iconH};
        for(size_t i=0;i<4;++i)if(!data_.setValue(section_,keys[i],std::to_string(values[i]),error)){fail(error);return false;}
        editedIconSections_.insert(section_);refreshCurrent();ui_.status="Координаты иконки изменены в конфиге. Нажмите «Сохранить».";return true;
    }
    void capture() {
        if(!ui_.hasModel){fail("Сначала загрузите world-модель выбранного оружия или патронов.");return;}
        std::wstring error;
        if(!renderer_.CaptureTransparent(snapshot_,1024,error)){fail(utf8(error));return;}
        if(!previewSnapshot())return;ui_.status="icon_snapshot готов: прозрачный фон, текущие камера и свет. Выберите ячейки и вставьте снимок.";
    }
    bool previewSnapshot() {
        std::string error;if(!atlas_.grid.validate(atlas_.image(),selectedRect(),error)){fail(error);return false;}
        fitted_=lab::FitIconSnapshot(snapshot_,uint32_t(ui_.iconW*ui_.cellPixels),uint32_t(ui_.iconH*ui_.cellPixels),ui_.iconPadding,error);
        if(!fitted_.valid()||!texture(fitted_,snapshotView_)){snapshotView_.Reset();ui_.hasSnapshot=false;fail(error.empty()?"Не удалось подготовить снимок.":error);return false;}
        ui_.hasSnapshot=true;ui_.snapshotWidth=int(fitted_.w);ui_.snapshotHeight=int(fitted_.h);return true;
    }
    void stageIcon() {
        if(!current()||!flushPendingParameters()||!flushPendingTexts()||!previewSnapshot())return;
        std::string error;const auto rect=selectedRect();bool occupied=false;
        auto cropped=lab::CropIcon(atlas_.image(),rect,atlas_.grid,error);
        for(const auto p:cropped.pixels)if(p.a){occupied=true;break;}
        std::wstring warning=L"Выбранная область атласа будет заменена целиком, включая прозрачные пиксели.\n";
        int shared=0;
        for(const auto& w:data_.items())if(w.section!=section_&&overlaps(rect,{w.iconX,w.iconY,w.iconWidth,w.iconHeight})){
            if(shared<8)warning+=L"\n"+wide(w.label)+L" ["+wide(w.section)+L"]";++shared;
        }
        if(shared)warning+=L"\n\nЭто оружие или патроны используют пересекающиеся ячейки.";
        warning+=L"\n\nПредметы вне каталогов оружия и патронов также могут использовать эту область.\nВставить снимок?";
        if((occupied||shared)&&MessageBoxW(window_,warning.c_str(),AppTitle,MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;
        if(!applyRect())return;
        if(!atlas_.replace(rect,fitted_,error)){fail(error);return;}
        updateAtlas();
        ui_.status="Снимок вставлен в рабочую копию атласа, координаты внесены в конфиг. Нажмите «Сохранить».";
    }
    bool hasPendingParameters()const {for(const auto* rows:{&ui_.parameters,&ui_.hudParameters})for(const auto& p:*rows)if(p.pending)return true;return false;}
    bool flushPendingParameters() {
        for(auto& p:ui_.parameters)if(p.pending){std::string error;
            if(!lab::ValidateHudParameter(p.key,p.value,error)||!data_.setValue(section_,p.key,p.value,error)){fail("Исправьте значение "+p.key+": "+error);return false;}p.pending=false;
            if(p.key=="inv_grid_x"||p.key=="inv_grid_y"||p.key=="inv_grid_width"||p.key=="inv_grid_height")editedIconSections_.insert(section_);
        }
        for(auto& p:ui_.hudParameters)if(p.pending){std::string error;
            if(p.key=="fire_bone"&&ui_.hasModel&&renderer_.HudView()){lab::HudMatrix bone;lab::WeaponConfigValues value{{p.key,p.value}};if(!renderer_.BoneTransform(lab::ConfigString(value,p.key),bone)){fail("В HUD-модели нет кости: "+p.value);return false;}}
            if(!lab::ValidateHudParameter(p.key,p.value,error)||!data_.setValue(ui_.hudSection,p.key,p.value,error)){fail("Исправьте HUD "+p.key+": "+error);return false;}p.pending=false;
        }return true;
    }
    bool flushPendingTexts() {
        if(!textDraftPending_)return true;
        const auto nameId=ui_.nameId,descriptionId=ui_.descriptionId,name=ui_.name,description=ui_.description;
        std::string error;
        if(!nameId.empty()&&!data_.setText(nameId,name,error)){fail(error);return false;}
        if(!descriptionId.empty()&&!data_.setText(descriptionId,description,error)){fail(error);return false;}
        textDraftPending_=false;refreshRows();return true;
    }
    void action(lab::WeaponAction what,int value) {
        using A=lab::WeaponAction;std::string error;
        switch(what){
        case A::SelectModelView:
            if(value!=int(lab::WeaponView::Model)&&value!=int(lab::WeaponView::Hud))break;
            if(value==int(lab::WeaponView::Hud)&&(!ui_.hudAvailable||ui_.catalog!=lab::WeaponCatalog::Weapons))break;
            if(!flushPendingParameters()||!flushPendingTexts())break;
            stopInput();ui_.view=lab::WeaponView(value);ui_.tab=lab::WeaponTab::Parameters;ui_.forceTab=true;loadModel();break;
        case A::ChangeHudParameter:
            if(value>=0&&size_t(value)<ui_.hudParameters.size()&&!ui_.hudSection.empty()){
                auto& row=ui_.hudParameters[size_t(value)];const auto key=row.key;
                if(key=="fire_bone"&&ui_.hasModel&&renderer_.HudView()){lab::HudMatrix bone;lab::WeaponConfigValues value{{key,row.value}};if(!renderer_.BoneTransform(lab::ConfigString(value,key),bone)){row.pending=true;fail("В HUD-модели нет кости: "+row.value);break;}}
                if(!lab::ValidateHudParameter(key,row.value,error)||!data_.setValue(ui_.hudSection,key,row.value,error)){row.pending=true;fail(error);break;}
                row.pending=false;
                if(key=="visual")loadModel();else applyHudPreview();
                ui_.status="Изменён HUD ["+ui_.hudSection+"] "+key+"; требуется сохранение LTX.";
            }break;
        case A::HudPreviewSettings:stopInput();applyHudPreview();break;
        case A::EffectVisibility:updateEffectsPreview();break;
        case A::EffectEnabled:
            if(value>=0&&size_t(value)<ui_.effects.size()){
                const auto row=ui_.effects[size_t(value)];
                if(row.enabled&&row.remembered.empty()){refreshEffects();ui_.effectSlot=value;ui_.effectPickerOpen=true;fail("Выберите эффект для подключения.");}
                else setEffect(value,row.enabled?row.remembered:std::string{});
            }break;
        case A::OpenEffectPicker:
            if(value>=0&&size_t(value)<ui_.effects.size()){
                ui_.effectSlot=value;ui_.effectPickerOpen=true;const auto& row=ui_.effects[size_t(value)];const auto name=row.enabled?row.name:row.remembered;
                auto i=std::find(ui_.effectNames.begin(),ui_.effectNames.end(),name);if(i!=ui_.effectNames.end())previewEffect(int(i-ui_.effectNames.begin()));
                else {ui_.effectCandidate.clear();effectPreviewView_.Reset();ui_.effectStatus="Выберите эффект в списке.";}
            }break;
        case A::PreviewEffect:previewEffect(value);break;
        case A::ConnectEffect:
            if(!ui_.effectCandidate.empty()&&setEffect(ui_.effectSlot,ui_.effectCandidate)){
                ui_.effects[size_t(ui_.effectSlot)].visible=true;ui_.effectPickerOpen=false;updateEffectsPreview();}
            break;
        case A::ReloadEffects:reloadEffects();break;
        case A::SelectCatalog:selectCatalog(value);break;
        case A::SelectWeapon:
            if(!queuedSelection_.empty()){
                const auto chosen=queuedSelection_;queuedSelection_.clear();if(!flushPendingParameters()||!flushPendingTexts())break;
                if(data_.find(chosen)){stopInput();section_=chosen;catalogSelection_[size_t(ui_.catalog)]=chosen;ui_.descriptionEdit={};refreshRows();refreshCurrent();loadModel();}
            }else select(value);break;
        case A::SaveData:saveData();break;
        case A::DiscardData:
            if((data_.dirty()||hasPendingParameters()||textDraftPending_)&&MessageBoxW(window_,L"Отбросить все несохранённые изменения LTX и XML?",AppTitle,MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)break;
            if(!data_.reload(error)){fail(error);break;}editedIconSections_.clear();textDraftPending_=false;refreshRows();refreshCurrent();if(current())loadModel();break;
        case A::ReloadCatalog:ui_.paths=loadedPaths_;loadPaths();break;
        case A::DefaultPaths:defaults();break;
        case A::BrowsePath:{
            stopInput();std::wstring file;
            if(value==2)file=chooseFile(window_,L"String table XML\0*.xml\0Все файлы\0*.*\0",L"xml",false,fs::u8path(ui_.paths.values[2]));
            else if(value==6)file=chooseFile(window_,L"Particle library\0*.xr\0Все файлы\0*.*\0",L"xr",false,fs::u8path(ui_.paths.values[6]));
            else if(value==3)file=chooseFile(window_,L"DDS atlas\0*.dds\0Все файлы\0*.*\0",L"dds",false,fs::u8path(ui_.paths.values[3]));
            else file=chooseFolder(window_);
            if(!file.empty()&&value>=0&&value<7){ui_.paths.values[size_t(value)]=utf8(file);if(value==0)defaults();}break;}
        case A::ApplyPaths:loadPaths();break;
        case A::ChangeParameter:
            if(value>=0&&size_t(value)<ui_.parameters.size()&&current()){
                const auto key=ui_.parameters[size_t(value)].key;const auto text=ui_.parameters[size_t(value)].value;
                if(key=="hud"){
                    bool failed=false;for(auto& row:ui_.hudParameters)if(row.pending){if(!lab::ValidateHudParameter(row.key,row.value,error)||!data_.setValue(ui_.hudSection,row.key,row.value,error)){failed=true;break;}row.pending=false;}
                    if(failed){fail(error);break;}}
                if(!lab::ValidateHudParameter(key,text,error)||!data_.setValue(section_,key,text,error)){ui_.parameters[size_t(value)].pending=true;fail(error);break;}
                ui_.parameters[size_t(value)].pending=false;
                if(key=="inv_grid_x"||key=="inv_grid_y"||key=="inv_grid_width"||key=="inv_grid_height")editedIconSections_.insert(section_);
                refreshRows();refreshCurrent(false);
                if((key=="visual"&&ui_.view!=lab::WeaponView::Hud)||key=="hud")loadModel();else {updateEffectsPreview();ui_.status="Изменён параметр "+key+"; требуется сохранение.";}
            }break;
        case A::ChangeTexts:
            textDraftPending_=true;
            if(flushPendingTexts())ui_.status="Тексты изменены; требуется сохранение XML. Общие string ID изменятся у всех ссылающихся предметов.";
            break;
        case A::SimulationSettings:updateSimulation();stopInput();break;
        case A::SimulationView:stopInput();if(!ui_.simulationAvailable)ui_.view=lab::WeaponView::Model;break;
        case A::FocusParameter:
            if(ui_.simulationAvailable&&value>=0&&size_t(value)<ui_.parameters.size()){
                const auto& key=ui_.parameters[size_t(value)].key;
                if(lab::DescribeSimulationParameter(key).relevant){ui_.view=(key=="bullet_speed"||key=="fire_distance")?lab::WeaponView::Trajectory:lab::WeaponView::Spread;stopInput();}
            }break;
        case A::PartVisibility:
            if(value>=0&&size_t(value)<ui_.meshes.size())renderer_.SetPartVisible(size_t(value),ui_.meshes[size_t(value)].visible);
            break;
        case A::ShowAllParts:for(size_t i=0;i<ui_.meshes.size();++i){ui_.meshes[i].visible=true;renderer_.SetPartVisible(i,true);}break;
        case A::SetMode:stopInput();renderer_.FlyCameraEnabled(ui_.mode==lab::WeaponMode::Fly);break;
        case A::Frame:if(ui_.view!=lab::WeaponView::Hud)renderer_.FrameSelection();break;
        case A::ResetView:if(ui_.view!=lab::WeaponView::Hud)renderer_.ResetView();break;
        case A::AxisView:if(ui_.view!=lab::WeaponView::Hud)renderer_.AlignAxis(value);break;
        case A::LightSettings:lighting();break;
        case A::BackgroundSettings:renderer_.SetBackground(ui_.background);break;
        case A::Wireframe:renderer_.WireframeEnabled(ui_.wireframe);break;
        case A::ApplyIconRect:applyRect();break;
        case A::CaptureIcon:capture();break;
        case A::PreviewSnapshot:if(snapshot_.valid())previewSnapshot();break;
        case A::StageIcon:stageIcon();break;
        case A::SaveAtlas:if(!atlas_.save(error))fail(error);else ui_.status="DDS сохранён. Изменённые координаты LTX сохраняются общей кнопкой «Сохранить».";break;
        case A::DiscardAtlas:if(!atlas_.dirty()||MessageBoxW(window_,L"Отбросить изменения атласа? Координаты в конфиге остаются отдельными изменениями.",AppTitle,MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){atlas_.discard();updateAtlas();}break;
        case A::ExportSnapshot:{
            if(!previewSnapshot())break;stopInput();const auto file=chooseFile(window_,L"PNG с прозрачностью\0*.png\0",L"png",true,wide(section_+"_icon.png"));
            if(!file.empty()){const double scale=std::min(1.0,1024.0/std::max(fitted_.w,fitted_.h));
                const auto png=lab::FitIconSnapshot(snapshot_,std::max(1u,uint32_t(std::lround(fitted_.w*scale))),std::max(1u,uint32_t(std::lround(fitted_.h*scale))),int(std::lround(ui_.iconPadding*scale)),error);
                if(!png.valid()||!saveTransparentPNG(file,png,error))fail(error);else ui_.status="PNG сохранён: "+utf8(file);}break;}
        }
        ui_.dataDirty=data_.dirty()||hasPendingParameters()||textDraftPending_;ui_.atlasDirty=atlas_.dirty();
    }
    void input(float delta) {
        auto& io=ImGui::GetIO();auto held=[](int vk){return (GetAsyncKeyState(vk)&0x8000)!=0;};
        const lab::FlightInput::Keys keys={held('A'),held('D'),held('Q'),held('E'),held('W'),held('S')};
        if(ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1)||ImGui::IsMouseClicked(2))viewportFocus_=ui_.viewportHovered;
        const bool popup=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
        const bool foreground=GetForegroundWindow()==window_;
        if(!foreground||popup||io.KeyAlt){flight_.Sample(keys,false,false);drag_=Drag::None;return;}
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_S,false)){
            flight_.Suspend();pending_.emplace_back(lab::WeaponAction::SaveData,0);flight_.Sample(keys,false,true);return;
        }
        if(io.WantTextInput){flight_.Sample(keys,false,false);drag_=Drag::None;return;}
        if(ui_.view!=lab::WeaponView::Model){stopInput();flight_.Sample(keys,false,false);return;}
        const bool allowed=viewportFocus_&&(!io.WantCaptureKeyboard||ui_.viewportActive)&&(!ImGui::IsAnyItemActive()||ui_.viewportActive);
        if(allowed){
            if(ImGui::IsKeyPressed(ImGuiKey_F,false))renderer_.FrameSelection();
            if(ImGui::IsKeyPressed(ImGuiKey_Home,false))renderer_.ResetView();
            if(ImGui::IsKeyPressed(ImGuiKey_1,false)){ui_.mode=lab::WeaponMode::Fly;renderer_.FlyCameraEnabled(true);flight_.Suspend();}
            if(ImGui::IsKeyPressed(ImGuiKey_2,false)){ui_.mode=lab::WeaponMode::Orbit;renderer_.FlyCameraEnabled(false);flight_.Suspend();}
        }
        if(allowed&&ui_.mode==lab::WeaponMode::Fly){
            const float speed=delta*(io.KeyShift?6.f:io.KeyCtrl?.6f:2.f);
            const auto m=flight_.Sample(keys,true,io.KeyCtrl||held(VK_CONTROL));renderer_.MoveCamera(m.right*speed,m.up*speed,m.forward*speed);
        }else flight_.Sample(keys,false,false);
        if(drag_!=Drag::None&&(dragButton_<0||!ImGui::IsMouseDown(dragButton_)))drag_=Drag::None;
        if(ui_.viewportHovered&&drag_==Drag::None){
            if(ImGui::IsMouseClicked(2)){drag_=Drag::Pan;dragButton_=2;}
            else if(ImGui::IsMouseClicked(1)){drag_=Drag::Orbit;dragButton_=1;}
            else if(ImGui::IsMouseClicked(0)){
                dragButton_=0;drag_=ui_.mode==lab::WeaponMode::Light?Drag::Light:ui_.mode==lab::WeaponMode::Model?Drag::Model:ui_.mode==lab::WeaponMode::Pan?Drag::Pan:Drag::Orbit;
            }
        }
        const float dx=io.MouseDelta.x/dpi_,dy=io.MouseDelta.y/dpi_;
        if(drag_==Drag::Orbit)renderer_.OrbitCamera(dx,dy);
        if(drag_==Drag::Pan)renderer_.PanCamera(dx,dy);
        if(drag_==Drag::Model)renderer_.RotateModel(dx,dy);
        if(drag_==Drag::Light){renderer_.OrbitLight(dx,dy);ui_.lightAzimuth=renderer_.LightAzimuthDegrees();ui_.lightElevation=renderer_.LightElevationDegrees();}
        if(ui_.viewportHovered&&io.MouseWheel!=0)renderer_.Zoom(io.MouseWheel);
    }
    void applyDpi(){
        auto& io=ImGui::GetIO();if(ready_)ImGui_ImplDX11_InvalidateDeviceObjects();
        lab::ConfigureWeaponUIStyle(dpi_);ui_.uiScale=dpi_;io.Fonts->Clear();
        static ImVector<ImWchar> ranges;ImFontGlyphRangesBuilder builder;builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        const ImWchar extra[]={0x2000,0x206f,0x2190,0x22ff,0};builder.AddRanges(extra);builder.BuildRanges(&ranges);
        wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);const auto font=fs::path(windows)/L"Fonts"/L"segoeui.ttf";
        if(!io.Fonts->AddFontFromFileTTF(utf8(font.wstring()).c_str(),16.f*dpi_,nullptr,ranges.Data))io.Fonts->AddFontDefault();
        if(ready_&&!ImGui_ImplDX11_CreateDeviceObjects())throw std::runtime_error("Direct3D font creation failed");
    }
    void frame(bool interactive=true){
        const auto now=GetTickCount64();const float delta=std::clamp(float(now-lastTick_)*.001f,.001f,.05f);lastTick_=now;
        if(dpiPending_){applyDpi();dpiPending_=false;}
        if(ui_.viewportRect.w>1&&ui_.viewportRect.h>1){std::wstring e;if(!renderer_.ResizeScene(UINT(ui_.viewportRect.w),UINT(ui_.viewportRect.h),e))fail(utf8(e));}
        updateMarkers();
        ui_.effectPreviewTexture=static_cast<ImTextureID>(reinterpret_cast<intptr_t>(effectPreviewView_.Get()));
        ui_.viewportTexture=static_cast<ImTextureID>(reinterpret_cast<intptr_t>(renderer_.SceneTexture()));
        ui_.atlasTexture=static_cast<ImTextureID>(reinterpret_cast<intptr_t>(atlasView_.Get()));
        ui_.snapshotTexture=static_cast<ImTextureID>(reinterpret_cast<intptr_t>(snapshotView_.Get()));
        ui_.dataDirty=data_.dirty()||hasPendingParameters()||textDraftPending_;ui_.atlasDirty=atlas_.dirty();
        ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
        pending_.clear();lab::DrawWeaponUI(ui_,[&](lab::WeaponAction a,int n){
            if(a==lab::WeaponAction::SelectWeapon&&n>=0&&size_t(n)<ui_.weapons.size())queuedSelection_=ui_.weapons[size_t(n)].section;
            pending_.emplace_back(a,n);
        });
        if(interactive)input(delta);
        ImGui::Render();renderer_.DrawScene();renderer_.BeginUI();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const HRESULT hr=renderer_.PresentUI();
        // Process mutations after draw lists consumed their SRVs and row references.
        const auto actions=pending_;
        for(const auto& a:actions)if(a.first==lab::WeaponAction::ChangeParameter||a.first==lab::WeaponAction::ChangeHudParameter||a.first==lab::WeaponAction::ChangeTexts)action(a.first,a.second);
        for(const auto& a:actions)if(a.first!=lab::WeaponAction::ChangeParameter&&a.first!=lab::WeaponAction::ChangeHudParameter&&a.first!=lab::WeaponAction::ChangeTexts)action(a.first,a.second);
        if(FAILED(hr)){
            stopInput();MessageBoxW(window_,L"Direct3D: устройство вывода потеряно. Закройте программу после сохранения изменений.",AppTitle,MB_OK|MB_ICONERROR);
            if(confirmPending())running_=false;else {minimized_=true;ui_.status="Direct3D потерян; закройте окно для повторного предложения сохранения.";}
        }
    }
    static LRESULT CALLBACK wndProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
        App* a=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(message==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);a->window_=w;SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
        if(!a)return DefWindowProcW(w,message,wp,lp);
        const bool handled=ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(w,message,wp,lp);
        switch(message){
        case WM_SIZE:a->minimized_=wp==SIZE_MINIMIZED;if(a->renderer_.Device()&&!a->minimized_){std::wstring e;if(!a->renderer_.Resize(LOWORD(lp),HIWORD(lp),e))a->fail(utf8(e));}return 0;
        case WM_DPICHANGED:{a->stopInput();a->dpi_=float(HIWORD(wp))/96.f;a->dpiPending_=true;const auto& r=*reinterpret_cast<const RECT*>(lp);SetWindowPos(w,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        case WM_GETMINMAXINFO:{const RECT work=monitorWorkArea(w);auto* mm=reinterpret_cast<MINMAXINFO*>(lp);mm->ptMinTrackSize={std::min(LONG(960*a->dpi_),work.right-work.left),std::min(LONG(600*a->dpi_),work.bottom-work.top)};return 0;}
        case WM_CANCELMODE:case WM_KILLFOCUS:a->stopInput();break;
        case WM_ACTIVATEAPP:if(!wp)a->stopInput();break;
        case WM_CAPTURECHANGED:if(reinterpret_cast<HWND>(lp)!=w){a->drag_=Drag::None;a->dragButton_=-1;}break;
        case WM_CLOSE:if(a->confirmPending())DestroyWindow(w);return 0;
        case WM_DESTROY:a->running_=false;PostQuitMessage(0);return 0;
        case WM_ERASEBKGND:return 1;
        case WM_SYSCOMMAND:if((wp&0xfff0)==SC_KEYMENU)return 0;break;
        }
        return handled?1:DefWindowProcW(w,message,wp,lp);
    }
public:
    ~App(){if(ready_){ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();}if(ImGui::GetCurrentContext())ImGui::DestroyContext();if(window_&&IsWindow(window_))DestroyWindow(window_);}
    bool create(HINSTANCE instance,bool software=false){
        WNDCLASSEXW klass{};klass.cbSize=sizeof(klass);klass.style=CS_CLASSDC;klass.lpfnWndProc=wndProc;klass.hInstance=instance;klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);klass.lpszClassName=L"KichrayWeaponLab";
        klass.hIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_MATERIALLAB),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
        klass.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_MATERIALLAB),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
        if(!RegisterClassExW(&klass)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
        dpi_=ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY));const RECT area=monitorWorkArea(nullptr);
        int width=std::min(int(1490*dpi_),int(area.right-area.left)),height=std::min(int(900*dpi_),int(area.bottom-area.top));
        window_=CreateWindowExW(0,klass.lpszClassName,AppTitle,WS_OVERLAPPEDWINDOW,area.left+(area.right-area.left-width)/2,area.top+(area.bottom-area.top-height)/2,width,height,nullptr,nullptr,instance,this);
        if(!window_)return false;std::wstring error;if(!renderer_.Init(window_,error,software)){MessageBoxW(window_,error.c_str(),AppTitle,MB_OK|MB_ICONERROR);return false;}
        IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;
        applyDpi();if(!ImGui_ImplWin32_Init(window_))return false;
        if(!ImGui_ImplDX11_Init(renderer_.Device(),renderer_.Context())){ImGui_ImplWin32_Shutdown();return false;}ready_=true;
        if(!ImGui_ImplDX11_CreateDeviceObjects())return false;
        ui_.paths.values[0]="D:\\Games\\S.T.A.L.K.E.R. Shadow of Chernobyl\\gamedata";defaults();
        const auto ini=settingsFile();std::array<wchar_t,32768> remembered{};
        bool haveSettings=false,haveParticlePath=false;
        for(size_t i=0;i<ui_.paths.values.size();++i){GetPrivateProfileStringW(L"Paths",std::to_wstring(i).c_str(),L"",remembered.data(),DWORD(remembered.size()),ini.c_str());if(remembered[0]){ui_.paths.values[i]=utf8(remembered.data());haveSettings=true;if(i==6)haveParticlePath=true;}}
        if(!haveParticlePath)ui_.paths.values[6]=(fs::u8path(ui_.paths.values[0])/L"particles.xr").u8string();
        renderer_.FlyCameraEnabled(true);renderer_.SkeletonEnabled(false);renderer_.OutlineEnabled(false);clearModel();lighting();
        if(haveSettings&&!software)loadPaths(false);
        lastTick_=GetTickCount64();frame(false);frame(false);return true;
    }
    int run(){
        ShowWindow(window_,SW_SHOWDEFAULT);UpdateWindow(window_);MSG message{};
        while(running_){
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){running_=false;break;}TranslateMessage(&message);DispatchMessageW(&message);}
            if(!running_)break;if(minimized_){WaitMessage();continue;}frame();
        }return 0;
    }
    int smoke(){
        // Explicit native entrypoint; never edits a user's gamedata.
        std::ofstream log(executableDirectory()/L"weaponlab-smoke.txt");bool ok=true;
        auto check=[&](bool pass,const char* name){log<<(pass?"PASS ":"FAIL ")<<name<<'\n';ok=ok&&pass;};
        std::wstring error;auto mesh=lab::MakeCube();check(renderer_.SetMesh(mesh,error),"mesh upload");
        lab::PartImages images;images.diffuse=lab::DefaultChecker(32);check(renderer_.SetPartImages(0,images),"texture upload");
        ui_.setupOpen=false;ui_.hasModel=true;renderer_.ResetView();
        for(int axis=0;axis<6;++axis){renderer_.AlignAxis(axis);lab::Image im;check(renderer_.CaptureTransparent(im,1024,error)&&im.valid(),"signed-axis transparent capture");}
        renderer_.ResetView();lab::Image shot;check(renderer_.CaptureTransparent(shot,1024,error),"transparent capture");
        bool alpha=false,body=false;for(auto p:shot.pixels){alpha=alpha||p.a==0;body=body||p.a>0;}check(alpha&&body,"transparent background and visible model");
        renderer_.SetPartVisible(0,false);lab::Image hidden;
        const bool hiddenCaptured=renderer_.CaptureTransparent(hidden,1024,error);
        check(hiddenCaptured&&hidden.valid()&&std::all_of(hidden.pixels.begin(),hidden.pixels.end(),[](lab::Pixel p){return p.a==0;}),"hidden mesh excluded from icon snapshot");
        renderer_.SetPartVisible(0,true);lab::Image restored;
        const bool restoredCaptured=renderer_.CaptureTransparent(restored,1024,error);
        check(restoredCaptured&&restored.valid()&&std::any_of(restored.pixels.begin(),restored.pixels.end(),[](lab::Pixel p){return p.a>0;}),"mesh visibility restored");
        renderer_.SetHudView(true,lab::HudOffset({0,0,4},{0,0,0}),40);float px=0,py=0;
        check(renderer_.ProjectPoint({0,0,0},px,py)&&std::abs(px-.5f)<.001f&&std::abs(py-.5f)<.001f,"HUD native units and camera centre");
        lab::Image hudFrame;check(renderer_.CaptureTransparent(hudFrame,512,error)&&std::any_of(hudFrame.pixels.begin(),hudFrame.pixels.end(),[](lab::Pixel p){return p.a>0;}),"HUD first person render");
        renderer_.SetHudView(false);renderer_.SetPartVisible(0,false);lab::Image withoutParticles,withParticles;
        check(renderer_.CaptureFrame(withoutParticles),"particle baseline capture");
        lab::ParticleLayer particle;particle.texture="fixture";particle.shader="particles\\blend";lab::ParticleSample sample;sample.size={.6f,.6f,.6f};particle.particles.push_back(sample);
        lab::Image sprite;sprite.w=sprite.h=2;sprite.pixels.assign(4,{255,20,10,255});
        check(renderer_.SetParticlePreview({{particle,lab::HudIdentity()}},{sprite},error),"particle shader and buffers");
        check(renderer_.CaptureFrame(withParticles)&&withParticles.pixels.size()==withoutParticles.pixels.size()&&std::memcmp(withParticles.pixels.data(),withoutParticles.pixels.data(),withParticles.pixels.size()*sizeof(lab::Pixel))!=0,"static game sprite pass is visible");
        renderer_.ClearParticlePreview();renderer_.SetPartVisible(0,true);
        std::string e;auto icon=lab::FitIconSnapshot(shot,250,100,2,e);check(icon.valid(),"snapshot fit");
        const auto temporary=fs::temp_directory_path()/(L"WeaponLab-smoke-"+std::to_wstring(GetCurrentProcessId())+L".png");
        check(saveTransparentPNG(temporary,icon,e),"WIC PNG write");const auto reopened=loadImage(temporary,e);
        check(reopened.valid()&&reopened.w==icon.w&&reopened.h==icon.h&&std::memcmp(reopened.pixels.data(),icon.pixels.data(),icon.pixels.size()*sizeof(lab::Pixel))==0,"PNG exact alpha roundtrip");
        std::error_code ec;fs::remove(temporary,ec);
        for(const auto dims:{POINT{960,600},POINT{1490,900}}){SetWindowPos(window_,nullptr,0,0,dims.x,dims.y,SWP_NOMOVE|SWP_NOZORDER);for(int i=0;i<3;++i){ui_.tab=lab::WeaponTab(i);ui_.forceTab=true;frame(false);frame(false);check(ui_.viewportRect.w>100&&ui_.viewportRect.h>100,"viewport layout");}}
        return ok?0:1;
    }
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR args,int){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(com))return 2;
    int result=3;
    try{ImGui_ImplWin32_EnableDpiAwareness();lab::StartupSplash splash;splash.Start(instance);App app;
        const bool smoke=args&&std::wstring(args)==L"--smoke-test";
        if(app.create(instance,smoke)){splash.Finish();result=smoke?app.smoke():app.run();}else splash.Cancel();
    }catch(const std::exception& e){MessageBoxW(nullptr,wide(e.what()).c_str(),AppTitle,MB_OK|MB_ICONERROR);}
    CoUninitialize();return result;
}
