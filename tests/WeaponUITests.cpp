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
lab::WeaponUIState ammunitionFixture(){
    auto s=fixture();s.catalog=lab::WeaponCatalog::Ammunition;s.forceCatalogTab=true;
    s.weapons={{"ammo_5.45x39_fmj","5.45×39 обычный","weapons.ltx",false},{"ammo_5.45x39_ap","5.45×39 бронебойный","weapons.ltx",false}};
    s.parameters.clear();
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"box_size","30"},{"k_disp","1"},{"k_pierce","1.2"},{"k_ap","0.4"},{"impair","1"}})
        s.parameters.push_back(lab::MakeWeaponParameter(pair.first,pair.second,"weapons.ltx"));
    s.name="5.45×39 обычный";s.nameId="ammo-5.45x39-fmj";s.descriptionId="enc_weapons1_ammo-5.45x39-fmj";
    s.description="Патроны калибра 5.45×39 мм. Параметры коробки и боеприпаса редактируются отдельно от оружия.";
    s.modelName="weapons/ammo/ammo_545x39_fmj.ogf";return s;
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
        auto* catalog=findWindow("CatalogPane");auto* bar=GImGui->TabBars.GetByKey(catalog->GetID("WeaponCatalog"));
        require(bar&&bar->Tabs.Size==2&&bar->Tabs[0].Width+1>=bar->Tabs[0].ContentWidth&&bar->Tabs[1].Width+1>=bar->Tabs[1].ContentWidth,"both catalog labels fit at supported sizes and DPI");
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
void ammunitionCatalog(){Context ctx;auto s=fixture();auto& io=ImGui::GetIO();
    std::vector<std::pair<lab::WeaponAction,int>> actions;
    auto step=[&]{frame(s,1490,900,[&](lab::WeaponAction a,int value){actions.emplace_back(a,value);});};
    auto click=[&](ImVec2 point){io.AddMousePosEvent(point.x,point.y);step();io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();};
    auto hover=[&](ImGuiWindow* window,ImGuiID id){require(window!=nullptr,"catalog target window exists");
        for(float y=window->Pos.y+5;y<window->Pos.y+window->Size.y-5;y+=3){io.AddMousePosEvent(window->Pos.x+30,y);step();if(GImGui->HoveredId==id)return io.MousePos;}
        throw std::runtime_error("catalog target control not found");};
    for(int i=0;i<4;++i)step();auto* pane=findWindow("CatalogPane");require(pane!=nullptr,"catalog pane exists");
    auto* bar=GImGui->TabBars.GetByKey(pane->GetID("WeaponCatalog"));require(bar&&bar->Tabs.Size==2,"weapon and ammunition tabs are separate");
    const auto ammoTabPoint=ImVec2(bar->BarRect.Min.x+bar->Tabs[1].Offset+bar->Tabs[1].Width*.5f,bar->BarRect.GetCenter().y);
    click(ammoTabPoint);step();
    require(s.catalog==lab::WeaponCatalog::Weapons&&s.weapons[0].section=="wpn_ak74","catalog click never replaces accepted rows or commits draft");
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::SelectCatalog,1))!=actions.end(),"ammunition tab requests shell catalog switch");
    // Simulate a failed draft flush: the shell retains the old catalog and restores its tab.
    s.forceCatalogTab=true;actions.clear();step();step();step();
    require(bar->SelectedTabId==bar->Tabs[0].ID&&actions.empty(),"rejected switch restores weapon tab without another request");
    // Simulate an accepted switch with the shell's filtered, unique ammunition rows.
    s=ammunitionFixture();for(int i=0;i<4;++i)step();actions.clear();
    require(bar->SelectedTabId==bar->Tabs[1].ID&&s.weapons.size()==2,"accepted ammunition catalog renders only supplied rows");
    ImGui::NewFrame();ImGui::LogToBuffer();lab::DrawWeaponUI(s,[](lab::WeaponAction,int){});const std::string output=GImGui->LogBuffer.c_str();ImGui::LogFinish();ImGui::Render();
    const auto meaning=lab::DescribeAmmunitionParameter("box_size");require(meaning.relevant&&!meaning.title.empty()&&output.find(meaning.title)!=std::string::npos,"ammunition-specific parameter explanation is rendered");
    require(output.find("wpn_ak74")==std::string::npos&&output.find("ammo_5.45x39_fmj")!=std::string::npos,"weapon rows do not leak into ammunition tab");
    require(output.find("ammo_5.45x39_fmj")==output.rfind("ammo_5.45x39_fmj"),"supplied ammo row is rendered once");
    // A stale firearm result must never enable ammunition simulation or consume a camera view.
    s.simulationAvailable=true;s.view=lab::WeaponView::Spread;step();require(s.view==lab::WeaponView::Model&&findWindow("SimulationScroll")==nullptr,"ammunition always keeps 3D viewport despite stale simulation state");
    s.parameters={lab::MakeWeaponParameter("k_disp","1","weapons.ltx")};step();auto* inspector=findWindow("InspectorScroll");
    const int zero=0;const auto row=inspector->GetID(zero);const auto component=ImHashData(&zero,sizeof(zero),row);const auto slider=ImHashStr("##StepSlider",0,component);
    hover(inspector,slider);actions.clear();io.AddMouseWheelEvent(0,1);step();step();
    require(s.parameters[0].value!="1"&&s.view==lab::WeaponView::Model,"ammo parameter wheel editing works without switching view");
    require(std::none_of(actions.begin(),actions.end(),[](auto action){return action.first==lab::WeaponAction::FocusParameter||action.first==lab::WeaponAction::SimulationSettings;}),"ammo edits never request shooting simulation");
    // Existing text, scene and icon workflows are reused for the selected ammo record.
    s.tab=lab::WeaponTab::Texts;s.forceTab=true;step();step();require(findWindow("WrappedDescriptionEditor")!=nullptr,"ammunition uses wrapped text editor");
    s.tab=lab::WeaponTab::Scene;s.forceTab=true;s.meshes={{"Ammo box",true}};step();step();require(s.meshes.size()==1,"ammo scene retains model parts");
    s.iconEditorOpen=true;for(int i=0;i<3;++i)step();auto* icons=findWindow("IconControls");
    click(hover(icons,icons->GetID("Сделать icon_snapshot")));
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::CaptureIcon,0))!=actions.end(),"ammo model can create icon snapshot");
    click(hover(icons,icons->GetID("Выбрать иконку для патронов")));
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::ApplyIconRect,0))!=actions.end(),"ammo icon rectangle uses same apply workflow");
}
void hudAndGroups(){
    Context ctx;auto s=fixture();s.hudAvailable=true;s.hudSection="wpn_ak74_hud";
    auto pose=lab::MakeWeaponParameter("position","0, 0, 0");pose.step=.001;pose.minimum=-1;pose.maximum=1;pose.integer=false;s.hudParameters={pose};
    auto& io=ImGui::GetIO();std::vector<std::pair<lab::WeaponAction,int>> actions;
    auto step=[&]{frame(s,1490,900,[&](lab::WeaponAction a,int i){actions.emplace_back(a,i);});};
    for(int i=0;i<4;++i)step();
    auto hover=[&](ImGuiWindow* w,ImGuiID id){require(w!=nullptr,"HUD target window exists");for(float y=w->Pos.y+5;y<w->Pos.y+w->Size.y-5;y+=3)for(float x=w->Pos.x+8;x<w->Pos.x+w->Size.x-8;x+=12){io.AddMousePosEvent(x,y);step();if(GImGui->HoveredId==id)return ImVec2(x,y);}throw std::runtime_error("HUD/group control not found");};
    auto click=[&](ImVec2 p){io.AddMousePosEvent(p.x,p.y);step();io.AddMouseButtonEvent(0,true);step();io.AddMouseButtonEvent(0,false);step();};
    auto* inspector=findWindow("InspectorScroll");auto group=inspector->GetID("###Group0Item");
    click(hover(inspector,group));require(inspector->StateStorage.GetInt(group,1)==0,"damage group collapses");
    click(hover(inspector,group));require(inspector->StateStorage.GetInt(group,0)==1,"damage group expands");
    auto* viewport=findWindow("ViewportPane");actions.clear();click(hover(viewport,viewport->GetID("HUD")));
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::SelectModelView,int(lab::WeaponView::Hud)))!=actions.end(),"HUD requests shell switch");
    require(s.view==lab::WeaponView::Model,"UI waits for shell acceptance of model switch");
    s.view=lab::WeaponView::Hud;for(int i=0;i<3;++i)step();inspector=findWindow("InspectorScroll");int zero=0;auto row=inspector->GetID(zero);auto component=ImHashData(&zero,sizeof(zero),row);auto slider=ImHashStr("##StepSlider",0,component);
    hover(inspector,slider);actions.clear();io.AddMouseWheelEvent(0,1);step();step();require(std::abs(s.hudParameters[0].numbers[0]-.001)<1e-8,"HUD position increments millimetre step");
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::ChangeHudParameter,0))!=actions.end(),"HUD dispatch is separate from world");
    require(s.parameters[0].value=="0.4"&&s.view==lab::WeaponView::Hud,"HUD edit preserves world parameter and first person view");
    s.effects={{"flame_particles","Пламя","fire_point","weapons\\test","weapons\\test",true,false}};
    s.tab=lab::WeaponTab::Effects;s.forceTab=true;step();step();inspector=findWindow("InspectorScroll");row=inspector->GetID(zero);actions.clear();click(hover(inspector,ImHashStr("Показывать в сцене",0,row)));
    require(s.effects[0].visible&&s.effects[0].enabled,"preview visibility does not disable game effect");
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::EffectVisibility,0))!=actions.end(),"preview visibility action");
    s.effectNames={"weapons\\test","weapons\\second"};s.effectSlot=0;s.effectPickerOpen=true;step();step();step();auto* list=findWindow("EffectList");actions.clear();click(hover(list,list->GetID(s.effectNames[1].c_str())));
    require(std::find(actions.begin(),actions.end(),std::make_pair(lab::WeaponAction::PreviewEffect,1))!=actions.end(),"picker requests candidate preview only");
    require(s.effects[0].name=="weapons\\test","browsing candidates leaves configured effect unchanged");
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
        if(argc>5&&std::string(argv[5]).find("ammo")==0){s=ammunitionFixture();
            if(std::string(argv[5])=="ammo-text"){s.tab=lab::WeaponTab::Texts;s.forceTab=true;}
            if(std::string(argv[5])=="ammo-icons")s.iconEditorOpen=true;}
        if(argc>5&&std::string(argv[5])=="icons")s.iconEditorOpen=true;
        if(argc>5&&std::string(argv[5])=="text"){s.tab=lab::WeaponTab::Texts;s.forceTab=true;for(int i=0;i<6;++i)s.description+=" Длинное описание оружия без искусственных переносов: точность и состояние зависят от конфигурации.";}
        if(argc>5&&std::string(argv[5])=="spread"){s.simulationAvailable=true;s.view=lab::WeaponView::Spread;s.ammunition={{"ammo_545","5.45 обычный"}};s.simulation.supported=s.simulation.spreadValid=true;s.simulation.magazineSize=30;s.simulation.roundsFired=30;s.simulation.rpm=600;s.simulation.maxConeRadius=.25;s.simulation.fireModes={1,3,-1};for(int i=0;i<30;++i)s.simulation.spread.push_back({.2*std::sin(i*3.2),.2*std::cos(i*1.6),i*.1,1,i,0});}
        if(argc>5&&std::string(argv[5])=="trajectory"){s.simulationAvailable=true;s.view=lab::WeaponView::Trajectory;s.ammunition={{"ammo_545","5.45 обычный"}};s.simulation.supported=s.simulation.trajectoryValid=true;s.simulation.fireModes={1,3,-1};s.simulation.magazineSize=30;s.simulation.rpm=600;for(int i=0;i<=200;i+=5)s.simulation.trajectory.push_back({double(i),-.00003*i*i,i/800.,800.});}
        if(argc>5&&std::string(argv[5])=="scene"){s.tab=lab::WeaponTab::Scene;s.forceTab=true;s.meshes={{"Основная модель",true},{"Дополнительный меш",false}};}
        if(argc>5&&(std::string(argv[5])=="hud"||std::string(argv[5])=="effects"||std::string(argv[5])=="effect-picker")){
            s.hudAvailable=true;s.view=lab::WeaponView::Hud;s.hudSection="wpn_ak74_hud";s.hudMotions={"idle","idle_aim","reload"};s.hudMotionIndex=0;
            for(const auto& key: {"position","orientation","fire_point","shell_point"})s.hudParameters.push_back(lab::MakeWeaponParameter(key,"0.01, -0.02, 0.03","w_ak74.ltx [wpn_ak74_hud]"));
            s.hudWarning="Секция HUD может использоваться несколькими оружиями.";s.markers={{"fire_point",.6f,.4f,true}};
            if(std::string(argv[5])!="hud"){s.tab=lab::WeaponTab::Effects;s.forceTab=true;s.particleLibraryStatus="particles.xr: 240 эффектов и групп.";s.effects={{"flame_particles","Пламя","fire_point","weapons\\generic_weapon01","",true,true},{"smoke_particles","Дым","fire_point","weapons\\generic_shoot_00","",true,false}};}
            if(std::string(argv[5])=="effect-picker"){s.effectPickerOpen=true;s.effectSlot=0;s.effectNames={"weapons\\generic_weapon01","weapons\\generic_shoot_00","weapons\\shells_545x39"};s.effectCandidate=s.effectNames[0];s.effectStatus="Статичный кадр. Подключение — отдельной кнопкой.";}}
        for(int i=0;i<5;++i)frame(s,w,h);
        snapshot(argv[2],w,h);return 0;
    }
    numeric();geometry();controls();atlas();description();meshVisibility();simulation();ammunitionCatalog();hudAndGroups();std::cout<<"PASS: WeaponUI bidirectional wheel, wrapped UTF8 editing, arbitrary atlas, mesh visibility, simulation panes, ammunition catalog\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
