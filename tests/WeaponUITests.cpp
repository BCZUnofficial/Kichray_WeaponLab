#include "WeaponUI.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool result,const char* text){if(!result)throw std::runtime_error(text);}
struct Context {
    ImVector<ImWchar> glyphRanges;
    Context(float scale=1){IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DeltaTime=1.f/60;
        
#ifdef _WIN32
        const char* font="C:/Windows/Fonts/segoeui.ttf";
#else
        const char* font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
        ImFontGlyphRangesBuilder glyphs;glyphs.AddRanges(io.Fonts->GetGlyphRangesCyrillic());glyphs.AddText("…−—×→");glyphs.BuildRanges(&glyphRanges);
        if(!io.Fonts->AddFontFromFileTTF(font,16.f*scale,nullptr,glyphRanges.Data))io.Fonts->AddFontDefault();
        unsigned char* data;int w,h;io.Fonts->GetTexDataAsRGBA32(&data,&w,&h);io.Fonts->SetTexID(1);lab::ConfigureWeaponUIStyle(scale);}
    ~Context(){ImGui::DestroyContext();}
};
lab::WeaponUIState fixture(){
    lab::WeaponUIState s;s.setupOpen=false;s.selected=0;s.hasModel=true;s.modelName="weapons/ak74/ak74.ogf";
    s.weapons={{"wpn_ak74","АК-74","w_ak74.ltx",false},{"wpn_pm","ПМ","w_pm.ltx",false}};
    s.parameters.push_back(lab::MakeWeaponParameter("hit_power","0.4","w_ak74.ltx"));
    s.parameters.push_back(lab::MakeWeaponParameter("cost","2000","w_ak74.ltx"));
    s.parameters.push_back(lab::MakeWeaponParameter("fire_modes","1, -1","w_ak74.ltx"));
    s.parameters.push_back(lab::MakeWeaponParameter("visual","weapons\\ak74\\ak74.ogf","w_ak74.ltx"));
    s.name="АК-74";s.nameId="wpn-ak74";s.description="Описание оружия";s.descriptionId="enc_weapons1_wpn-ak74";
    s.atlasWidth=s.atlasHeight=1024;s.atlasTexture=ImTextureID(2);s.snapshotTexture=ImTextureID(3);s.snapshotWidth=600;s.snapshotHeight=300;
    return s;
}
void frame(lab::WeaponUIState& s,int w,int h,const lab::WeaponUICallback& action=[](lab::WeaponAction,int){}){
    auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(float(w),float(h));ImGui::NewFrame();lab::DrawWeaponUI(s,action);ImGui::Render();}
ImGuiWindow* findWindow(const char* name){for(auto* w:GImGui->Windows)if(w->Active&&std::string(w->Name).find(name)!=std::string::npos)return w;return nullptr;}
void geometry(){for(float scale:{1.f,1.5f}){Context ctx(scale);auto s=fixture();s.uiScale=scale;
    for(auto size:std::vector<ImVec2>{{960,600},{1490,900},{1920,1080}})for(int tab=0;tab<3;++tab){s.tab=lab::WeaponTab(tab);s.forceTab=true;
        for(int settle=0;settle<4;++settle)frame(s,int(size.x),int(size.y));
        require(s.renderedPaneCount==3,"three independent panes");require(s.viewportRect.w>100&&s.viewportRect.h>100,"usable viewport");
        require(s.viewportRect.x>=s.leftRect.x+s.leftRect.w,"viewport does not overlap catalog");
        require(s.viewportRect.x+s.viewportRect.w<=s.inspectorRect.x+1,"viewport does not overlap inspector");
        require(s.inspectorRect.x+s.inspectorRect.w<=size.x,"inspector stays within display");
        require(s.tab==lab::WeaponTab(tab),"requested tab active");
    }
}}
void controls(){Context ctx;auto s=fixture();auto& io=ImGui::GetIO();
    std::vector<std::pair<lab::WeaponAction,int>> actions;
    auto step=[&]{frame(s,1490,900,[&](lab::WeaponAction a,int i){actions.emplace_back(a,i);});};for(int i=0;i<4;++i)step();
    auto count=[&](lab::WeaponAction a){return std::count_if(actions.begin(),actions.end(),[&](auto e){return e.first==a;});};
    auto click=[&](ImVec2 point){io.AddMousePosEvent(point.x,point.y);step();io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();};
    auto hoverID=[&](ImGuiWindow* w,ImGuiID id){require(w!=nullptr,"target UI window exists");for(float y=w->Pos.y+5;y<w->Pos.y+w->Size.y-5;y+=3){io.AddMousePosEvent(w->Pos.x+50,y);step();if(GImGui->HoveredId==id)return ImVec2(w->Pos.x+50,y);}throw std::runtime_error("target UI control not found");};
    auto* inspector=findWindow("InspectorScroll");require(inspector!=nullptr,"inspector scroll exists");
    const auto row=inspector->GetID(0);const auto component=ImHashData(&s.selected,sizeof(s.selected),row); // same integer 0 seed
    const auto slider=ImHashStr("##StepSlider",0,component);
    const auto sliderPoint=hoverID(inspector,slider);const float scrollBefore=inspector->Scroll.y;const double before=s.parameters[0].numbers[0];actions.clear();io.AddMouseWheelEvent(0,1);step();step();
    require(std::abs(s.parameters[0].numbers[0]-before-.01)<1e-10,"wheel advances exactly one step");require(count(lab::WeaponAction::ChangeParameter)==1,"wheel emits one edit");require(s.parameters[0].pending,"pending value remains until shell accepts backend write");
    actions.clear();io.AddMouseWheelEvent(0,-1);step();step();
    require(std::abs(s.parameters[0].numbers[0]-before)<1e-10,"down wheel reverses one step");require(inspector->Scroll.y==scrollBefore,"down wheel never scrolls inspector over a slider");
    io.AddMousePosEvent(20,20);step();io.AddMousePosEvent(sliderPoint.x,sliderPoint.y);io.AddMouseWheelEvent(0,-1);step();step();
    require(std::abs(s.parameters[0].numbers[0]-before+.01)<1e-10&&inspector->Scroll.y==scrollBefore,"first simultaneous hover/down wheel tick is owned by slider");
    s.parameters[0].step=.125;actions.clear();io.AddMouseWheelEvent(0,1);step();step();
    const double v=s.parameters[0].numbers[0];require(std::abs(v/.125-std::round(v/.125))<1e-10,"keyboard-defined step grid is respected");
    s.parameters[0]=lab::MakeWeaponParameter("hit_power","1");s.parameters[0].minimum=.25;s.parameters[0].maximum=3.25;s.parameters[0].step=1;
    hoverID(inspector,slider);io.AddMouseWheelEvent(0,1);step();step();
    require(s.parameters[0].value=="2.25","fractional slider anchor is never rounded by stale integral formatting");
    auto* view=findWindow("ViewportPane");require(view!=nullptr,"viewport pane exists");
    // Locate each axis by its stable ID, searching horizontally and vertically.
    const char* axes[]={"+X","+Y","+Z","-X","-Y","-Z"};
    for(int axis=0;axis<6;++axis){const auto id=view->GetID(axes[axis]);bool found=false;ImVec2 pos;
        for(float y=view->Pos.y+5;y<view->Pos.y+100&&!found;y+=4)for(float x=view->Pos.x+5;x<view->Pos.x+view->Size.x-5;x+=4){io.AddMousePosEvent(x,y);step();if(GImGui->HoveredId==id){found=true;pos=ImVec2(x,y);break;}}
        require(found,"signed axis button exists");actions.clear();click(pos);require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::AxisView,axis))!=actions.end(),"signed axis dispatch matches contract");
    }
    // Clicking another mode must preserve ImGui style stack balance.
    click(ImVec2(view->Pos.x+80,view->Pos.y+20));step();require(GImGui->ColorStack.Size==0,"mode color stack balanced");
}
void atlas(){Context ctx;auto s=fixture();s.iconEditorOpen=true;s.hasSnapshot=true;auto& io=ImGui::GetIO();std::vector<std::pair<lab::WeaponAction,int>> actions;
    auto step=[&]{frame(s,1490,900,[&](lab::WeaponAction a,int i){actions.emplace_back(a,i);});};for(int i=0;i<4;++i)step();
    require(s.atlasRect.w==1000&&s.atlasRect.h==1000,"1024 atlas shows exactly 20 by 20 50px cells");
    const float x=s.atlasRect.x,y=s.atlasRect.y;
    io.AddMousePosEvent(x+1.5f*50,y+2.5f*50);step();io.AddMouseButtonEvent(0,true);step();
    io.AddMousePosEvent(x+4.5f*50,y+3.5f*50);step();io.AddMouseButtonEvent(0,false);step();
    require(s.iconX==1&&s.iconY==2&&s.iconW==4&&s.iconH==2,"drag selects inclusive exact cell rectangle");
    require(!actions.empty()&&std::all_of(actions.begin(),actions.end(),[](auto a){return a.first==lab::WeaponAction::PreviewSnapshot;}),"selection refreshes fitted preview without writing files");
    s.atlasWidth=2048;s.atlasHeight=3072;s.atlasZoom=1;step();step();
    require(s.atlasRect.w==2000&&s.atlasRect.h==3050,"arbitrary DDS keeps exact 50px cells across whole atlas");
    auto* scroll=findWindow("AtlasScroll");require(scroll&&scroll->ScrollMax.x>0&&scroll->ScrollMax.y>0,"large atlas scrolls both axes");
    ImGui::SetScrollX(scroll,1000);ImGui::SetScrollY(scroll,1100);step();step();
    const float cx=s.atlasRect.x+22.5f*50,cy=s.atlasRect.y+24.5f*50;
    io.AddMousePosEvent(cx,cy);step();io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();
    require(s.iconX==22&&s.iconY==24,"cells past twentieth column and row remain selectable");
    s.iconX=2147483647;s.iconW=2147483647;step(); // malformed fields must not overflow bounds check
}
void description(){Context ctx;auto s=fixture();s.tab=lab::WeaponTab::Texts;s.forceTab=true;
    s.description="Длинное описание оружия с русскими словами, пробелами и неизменными переносами. ";
    for(int i=0;i<4;++i)s.description+=s.description;
    s.description+="\nВторая строка. ";s.description+=std::string(150,'x');
    const auto original=s.description;auto& io=ImGui::GetIO();
    auto step=[&]{frame(s,960,600);};for(int i=0;i<5;++i)step();
    require(s.description==original,"visual wrapping never mutates stored description");
    auto* editor=findWindow("WrappedDescriptionEditor");require(editor!=nullptr,"wrapped editor exists");
    require(editor->ScrollMax.x==0&&editor->ScrollMax.y>0,"long description wraps vertically without horizontal scroll");
    const auto lines=lab::LayoutWeaponDescription(original,180);
    require(lines.size()>5,"description is broken into visual lines");
    for(const auto& line:lines){require(line.end>=line.begin,"visual line offsets ordered");
        require(ImGui::CalcTextSize(original.data()+line.begin,original.data()+line.end).x<=181,"even long unbroken words fit line width");
        require(line.begin==0||(static_cast<unsigned char>(original[line.begin])&0xc0)!=0x80,"wrap starts at UTF8 boundary");}
    io.AddMousePosEvent(editor->Pos.x+20,editor->Pos.y+20);step();io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();
    require(s.descriptionEdit.focused,"click activates wrapped text editor");
    const size_t position=s.descriptionEdit.cursor;
    io.AddInputCharactersUTF8("Я");step();step();
    require(s.description==original.substr(0,position)+"Я"+original.substr(position),"UTF8 typing edits original bytes without soft newline insertion");
    io.AddKeyEvent(ImGuiKey_Backspace,true);step();io.AddKeyEvent(ImGuiKey_Backspace,false);step();
    require(s.description==original,"backspace removes one Cyrillic codepoint");
    io.AddKeyEvent(ImGuiKey_Enter,true);step();io.AddKeyEvent(ImGuiKey_Enter,false);step();
    require(s.description==original.substr(0,position)+"\n"+original.substr(position),"Enter alone adds explicit source newline");
    require(io.WantTextInput,"description claims keyboard from camera");
    s.description="a";s.descriptionEdit.observed="a";s.descriptionEdit.cursor=s.descriptionEdit.anchor=1;s.descriptionEdit.undo={"Я"};
    io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);step();io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);step();
    require(s.description=="Я"&&s.descriptionEdit.cursor==0,"undo snaps caret to UTF8 boundary");
    io.AddInputCharactersUTF8("Б");step();step();require(s.description=="БЯ","typing after unicode undo cannot split codepoint");
}
void meshVisibility(){Context ctx;auto s=fixture();s.tab=lab::WeaponTab::Scene;s.forceTab=true;s.meshes={{"Main",true},{"Extra",true}};auto& io=ImGui::GetIO();
    std::vector<std::pair<lab::WeaponAction,int>> actions;auto step=[&]{frame(s,960,600,[&](lab::WeaponAction a,int value){actions.emplace_back(a,value);});};
    for(int i=0;i<4;++i)step();auto* inspector=findWindow("InspectorScroll");require(inspector!=nullptr,"scene inspector exists");
    ImGui::SetScrollY(inspector,inspector->ScrollMax.y);step();step();const int index=1;const auto parent=ImHashData(&index,sizeof(index),inspector->ID);const auto target=ImHashStr("Extra",0,parent);
    bool found=false;ImVec2 point;
    for(float y=inspector->Pos.y+5;y<inspector->Pos.y+inspector->Size.y-5;y+=3){io.AddMousePosEvent(inspector->Pos.x+20,y);step();if(GImGui->HoveredId==target){found=true;point=io.MousePos;break;}}
    require(found,"mesh checkbox visible by scrolling");io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();
    require(!s.meshes[1].visible&&std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::PartVisibility,1))!=actions.end(),"part checkbox toggles intended renderer part");
}
void simulation(){Context ctx;auto s=fixture();s.simulationAvailable=true;s.ammunition={{"ammo_545","5.45 обычный"}};
    s.view=lab::WeaponView::Spread;s.simulation.supported=true;s.simulation.spreadValid=true;s.simulation.magazineSize=30;
    s.simulation.fireModes={1,3,-1};s.simulation.roundsFired=30;s.simulation.rpm=600;s.simulation.maxConeRadius=.1;
    s.simulation.spread={{.02,.03,0,1,0,0},{-.01,-.01,.1,.99,1,0}};
    for(int i=0;i<4;++i)frame(s,960,600);
    require(!s.viewportHovered&&!s.viewportActive,"simulation never routes pointer into 3D camera");
    require(findWindow("SimulationScroll")!=nullptr,"simulation replaces model viewport");
    s.view=lab::WeaponView::Trajectory;s.simulation.trajectoryValid=true;s.simulation.trajectory={{0,0,0,800},{100,-.1,.15,700},{200,-.5,.3,600}};
    frame(s,960,600);require(!s.viewportActive,"trajectory also isolates camera");
    s.simulationAvailable=false;frame(s,960,600);require(s.view==lab::WeaponView::Model,"nonfirearm always returns to model");
}
void snapshot(const std::string& path,int width,int height) {
    unsigned char* atlas=nullptr;int aw=0,ah=0;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas,&aw,&ah);
    std::vector<unsigned char> rgb(size_t(width)*height*3,15);
    auto edge=[](ImVec2 a,ImVec2 b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
    auto* data=ImGui::GetDrawData();
    for(int li=0;li<data->CmdListsCount;++li){const auto& list=*data->CmdLists[li];
        for(const auto& cmd:list.CmdBuffer){if(cmd.UserCallback)continue;
            for(unsigned k=0;k+2<cmd.ElemCount;k+=3){
                const auto& a=list.VtxBuffer[list.IdxBuffer[cmd.IdxOffset+k]+cmd.VtxOffset];
                const auto& b=list.VtxBuffer[list.IdxBuffer[cmd.IdxOffset+k+1]+cmd.VtxOffset];
                const auto& c=list.VtxBuffer[list.IdxBuffer[cmd.IdxOffset+k+2]+cmd.VtxOffset];
                const float area=edge(a.pos,b.pos,c.pos.x,c.pos.y);if(std::fabs(area)<1e-6f)continue;
                int x0=std::max({0,int(std::floor(std::min({a.pos.x,b.pos.x,c.pos.x}))),int(std::ceil(cmd.ClipRect.x))});
                int y0=std::max({0,int(std::floor(std::min({a.pos.y,b.pos.y,c.pos.y}))),int(std::ceil(cmd.ClipRect.y))});
                int x1=std::min({width,int(std::ceil(std::max({a.pos.x,b.pos.x,c.pos.x}))),int(cmd.ClipRect.z)});
                int y1=std::min({height,int(std::ceil(std::max({a.pos.y,b.pos.y,c.pos.y}))),int(cmd.ClipRect.w)});
                for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                    const float wa=edge(b.pos,c.pos,x+.5f,y+.5f)/area,wb=edge(c.pos,a.pos,x+.5f,y+.5f)/area,wc=1-wa-wb;
                    if(wa<0||wb<0||wc<0)continue;
                    const float u=a.uv.x*wa+b.uv.x*wb+c.uv.x*wc,v=a.uv.y*wa+b.uv.y*wb+c.uv.y*wc;
                    const int tx=std::clamp(int(u*aw),0,aw-1),ty=std::clamp(int(v*ah),0,ah-1);
                    const unsigned char flat[4]={255,255,255,255};
                    const auto* tex=cmd.GetTexID()==1?atlas+(size_t(ty)*aw+tx)*4:flat;
                    const float alpha=(((a.col>>24)&255)*wa+((b.col>>24)&255)*wb+((c.col>>24)&255)*wc)*tex[3]/65025.f;
                    for(int ch=0;ch<3;++ch){const float color=(((a.col>>(ch*8))&255)*wa+((b.col>>(ch*8))&255)*wb+((c.col>>(ch*8))&255)*wc)*tex[ch]/255.f;
                        auto& out=rgb[(size_t(y)*width+x)*3+ch];out=static_cast<unsigned char>(std::clamp(color*alpha+out*(1-alpha),0.f,255.f));}
                }
            }
        }
    }
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";out.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));
    require(bool(out),"cannot write audit screenshot");
}
void numeric(){
    auto v=lab::MakeWeaponParameter("values","-0.1, 2.5, 1e-3");require(v.numbers.size()==3,"numeric vectors parsed");
    require(lab::MakeWeaponParameter("texture","a\\b").numbers.empty(),"strings stay text");
    require(lab::MakeWeaponParameter("v","nan").numbers.empty(),"nonfinite is not numeric");
    require(lab::MakeWeaponParameter("v","1,,2").numbers.empty(),"malformed numeric vectors stay raw");
    auto integer=lab::MakeWeaponParameter("value","1");integer.value="1.5";lab::RefreshWeaponParameterNumbers(integer);
    require(!integer.integer&&integer.numbers.size()==1&&integer.numbers[0]==1.5,"raw integer-to-fraction edit cannot retain integer formatting");
    integer.step=.125;integer.value="";lab::RefreshWeaponParameterNumbers(integer);integer.value="3";lab::RefreshWeaponParameterNumbers(integer);
    require(integer.step==.125&&!integer.integer&&integer.maximum>=3,"transient empty raw edit preserves custom fractional step and includes current value");
    auto textual=lab::MakeWeaponParameter("value","auto");textual.value="1000";lab::RefreshWeaponParameterNumbers(textual);
    require(textual.integer&&textual.step==1&&textual.minimum<=1000&&textual.maximum>=1000,"first text-to-numeric edit initializes sensible controls");
    require(std::abs(lab::WeaponSteppedValue(-1,1,.25,3)+.25)<1e-12,"negative stepped range");
    require(lab::WeaponSteppedValue(0,1,.3,100)==.9||std::abs(lab::WeaponSteppedValue(0,1,.3,100)-.9)<1e-12,"nondivisible range stays on grid");
}
}
int main(int argc,char** argv){try{
    if(argc>=5&&std::string(argv[1])=="--snapshot"){
        Context ctx;auto s=fixture();const int w=std::stoi(argv[3]),h=std::stoi(argv[4]);
        if(argc>5&&std::string(argv[5])=="icons")s.iconEditorOpen=true;
        if(argc>5&&std::string(argv[5])=="text"){s.tab=lab::WeaponTab::Texts;s.forceTab=true;for(int i=0;i<6;++i)s.description+=" Длинное описание оружия без искусственных переносов: точность и состояние зависят от конфигурации.";}
        if(argc>5&&std::string(argv[5])=="spread"){s.simulationAvailable=true;s.view=lab::WeaponView::Spread;s.ammunition={{"ammo_545","5.45 обычный"}};s.simulation.supported=s.simulation.spreadValid=true;s.simulation.magazineSize=30;s.simulation.roundsFired=30;s.simulation.rpm=600;s.simulation.maxConeRadius=.25;s.simulation.fireModes={1,3,-1};for(int i=0;i<30;++i)s.simulation.spread.push_back({.2*std::sin(i*3.2),.2*std::cos(i*1.6),i*.1,1,i,0});}
        if(argc>5&&std::string(argv[5])=="trajectory"){s.simulationAvailable=true;s.view=lab::WeaponView::Trajectory;s.ammunition={{"ammo_545","5.45 обычный"}};s.simulation.supported=s.simulation.trajectoryValid=true;s.simulation.fireModes={1,3,-1};s.simulation.magazineSize=30;s.simulation.rpm=600;for(int i=0;i<=200;i+=5)s.simulation.trajectory.push_back({double(i),-.00003*i*i,i/800.,800.});}
        if(argc>5&&std::string(argv[5])=="scene"){s.tab=lab::WeaponTab::Scene;s.forceTab=true;s.meshes={{"Основная модель",true},{"Дополнительный меш",false}};}
        for(int i=0;i<5;++i)frame(s,w,h);
        snapshot(argv[2],w,h);return 0;
    }
    numeric();geometry();controls();atlas();description();meshVisibility();simulation();std::cout<<"PASS: WeaponUI bidirectional wheel, wrapped UTF8 editing, arbitrary atlas, mesh visibility, simulation panes\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
