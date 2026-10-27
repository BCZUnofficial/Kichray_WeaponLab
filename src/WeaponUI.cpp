#include "WeaponUI.h"
#include "WeaponData.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace lab {
namespace {
std::vector<double> parseNumbers(const std::string& value) {
    std::vector<double> result;
    if(!ParseWeaponNumbers(value,result))result.clear();
    return result;
}
std::string numberText(double value,bool integer) {
    std::ostringstream out;out.imbue(std::locale::classic());
    if(integer&&std::floor(value)==value&&std::abs(value)<9e15)out<<std::fixed<<std::setprecision(0)<<value;
    else out<<std::setprecision(12)<<value;
    return out.str();
}
std::string shortNumber(double value,int precision=2) {std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(precision)<<value;return out.str();}
void rebuild(WeaponParameter& row) {
    row.value.clear();for(double value:row.numbers){if(!row.value.empty())row.value+=", ";row.value+=numberText(value,row.integer);}
}
int inputResize(ImGuiInputTextCallbackData* data) {
    if(data->EventFlag==ImGuiInputTextFlags_CallbackResize){auto* s=static_cast<std::string*>(data->UserData);s->resize(size_t(data->BufTextLen));data->Buf=s->data();}return 0;
}
bool textInput(const char* label,std::string& text,ImGuiInputTextFlags flags=0) {
    return ImGui::InputText(label,text.data(),text.capacity()+1,flags|ImGuiInputTextFlags_CallbackResize,inputResize,&text);
}
size_t nextCodepoint(const std::string& text,size_t at) {
    if(at>=text.size())return text.size();
    ++at;while(at<text.size()&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;return at;
}
size_t previousCodepoint(const std::string& text,size_t at) {
    if(!at)return 0;--at;while(at&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;return at;
}
float textWidth(const std::string& text,size_t first,size_t last) {
    return ImGui::CalcTextSize(text.data()+first,text.data()+last,false).x;
}
size_t positionAtX(const std::string& text,const WrappedTextLine& line,float x) {
    for(size_t at=line.begin;at<line.end;){const size_t next=nextCodepoint(text,at);
        if(x<(textWidth(text,line.begin,at)+textWidth(text,line.begin,next))*.5f)return at;at=next;}
    return line.end;
}
size_t cursorLine(const std::vector<WrappedTextLine>& lines,size_t cursor) {
    for(size_t i=0;i<lines.size();++i)if(cursor<lines[i].end||i+1==lines.size()||cursor<lines[i+1].begin)return i;
    return 0;
}
bool wrappedDescription(WeaponUIState& state,ImVec2 size) {
    auto& text=state.description;auto& edit=state.descriptionEdit;auto& io=ImGui::GetIO();
    const bool enabled=state.canEditDescription&&state.selected>=0;
    if(edit.observed!=text){edit.cursor=edit.anchor=0;edit.undo.clear();edit.redo.clear();edit.observed=text;}
    edit.cursor=std::min(edit.cursor,text.size());edit.anchor=std::min(edit.anchor,text.size());
    const ImGuiID id=ImGui::GetID("##WrappedDescriptionEditor");
    ImGui::BeginChild("WrappedDescriptionEditor",size,ImGuiChildFlags_Borders,ImGuiWindowFlags_None);
    const auto origin=ImGui::GetCursorScreenPos();const float width=std::max(8.f,ImGui::GetContentRegionAvail().x);
    auto lines=LayoutWeaponDescription(text,width);const float lineHeight=ImGui::GetTextLineHeightWithSpacing();
    const bool windowHovered=ImGui::IsWindowHovered();
    ImGui::InvisibleButton("##DescriptionCanvas",ImVec2(width,std::max(ImGui::GetContentRegionAvail().y,float(lines.size())*lineHeight)));
    const bool canvasHovered=ImGui::IsItemHovered();const auto* window=ImGui::GetCurrentWindow();
    if(enabled&&canvasHovered&&ImGui::IsMouseClicked(0)){
        const int row=std::clamp(int((io.MousePos.y-origin.y)/lineHeight),0,int(lines.size())-1);
        edit.cursor=positionAtX(text,lines[size_t(row)],io.MousePos.x-origin.x);if(!io.KeyShift)edit.anchor=edit.cursor;
        edit.focused=true;edit.dragging=true;ImGui::SetWindowFocus();
    }else if(ImGui::IsMouseClicked(0)&&!windowHovered){edit.focused=false;edit.dragging=false;}
    if(edit.dragging&&ImGui::IsMouseDown(0)){
        const int row=std::clamp(int((io.MousePos.y-origin.y)/lineHeight),0,int(lines.size())-1);
        edit.cursor=positionAtX(text,lines[size_t(row)],io.MousePos.x-origin.x);
    }else edit.dragging=false;
    bool changed=false,moved=false;
    auto checkpoint=[&]{if(edit.undo.size()>=64)edit.undo.erase(edit.undo.begin());edit.undo.push_back(text);edit.redo.clear();};
    auto replaceSelection=[&](const std::string& insert){const size_t first=std::min(edit.anchor,edit.cursor),last=std::max(edit.anchor,edit.cursor);
        text.replace(first,last-first,insert);edit.cursor=edit.anchor=first+insert.size();changed=moved=true;};
    auto move=[&](size_t position){edit.cursor=position;if(!io.KeyShift)edit.anchor=position;moved=true;};
    if(edit.focused&&enabled&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)){
        ImGui::SetActiveID(id,ImGui::GetCurrentWindow());ImGui::KeepAliveID(id);
        GImGui->WantTextInputNextFrame=1;GImGui->WantCaptureKeyboardNextFrame=1;io.WantTextInput=true;io.WantCaptureKeyboard=true;
        const bool ctrl=io.KeyCtrl||io.KeySuper;
        if(ctrl&&ImGui::IsKeyPressed(ImGuiKey_A,false)){edit.anchor=0;edit.cursor=text.size();moved=true;}
        if(ctrl&&(ImGui::IsKeyPressed(ImGuiKey_C,false)||ImGui::IsKeyPressed(ImGuiKey_X,false))&&edit.cursor!=edit.anchor){
            const auto part=text.substr(std::min(edit.cursor,edit.anchor),std::max(edit.cursor,edit.anchor)-std::min(edit.cursor,edit.anchor));ImGui::SetClipboardText(part.c_str());
            if(ImGui::IsKeyPressed(ImGuiKey_X,false)){checkpoint();replaceSelection("");}}
        if(ctrl&&ImGui::IsKeyPressed(ImGuiKey_V,false)){const char* paste=ImGui::GetClipboardText();if(paste&&*paste){checkpoint();replaceSelection(paste);}}
        if(ctrl&&(ImGui::IsKeyPressed(ImGuiKey_Z,false)||ImGui::IsKeyPressed(ImGuiKey_Y,false))){
            const bool redo=io.KeyShift||ImGui::IsKeyPressed(ImGuiKey_Y,false);auto& from=redo?edit.redo:edit.undo;auto& to=redo?edit.undo:edit.redo;
            if(!from.empty()){to.push_back(text);text=from.back();from.pop_back();edit.cursor=std::min(edit.cursor,text.size());
                while(edit.cursor>0&&edit.cursor<text.size()&&(static_cast<unsigned char>(text[edit.cursor])&0xc0)==0x80)--edit.cursor;
                edit.anchor=edit.cursor;changed=moved=true;}}
        if(changed)lines=LayoutWeaponDescription(text,width);
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)){move(!io.KeyShift&&edit.cursor!=edit.anchor?std::min(edit.cursor,edit.anchor):previousCodepoint(text,edit.cursor));}
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)){move(!io.KeyShift&&edit.cursor!=edit.anchor?std::max(edit.cursor,edit.anchor):nextCodepoint(text,edit.cursor));}
        if(ImGui::IsKeyPressed(ImGuiKey_Home))move(ctrl?0:lines[cursorLine(lines,edit.cursor)].begin);
        if(ImGui::IsKeyPressed(ImGuiKey_End))move(ctrl?text.size():lines[cursorLine(lines,edit.cursor)].end);
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)||ImGui::IsKeyPressed(ImGuiKey_DownArrow)){
            const auto row=cursorLine(lines,edit.cursor);const float x=textWidth(text,lines[row].begin,edit.cursor);
            const size_t target=ImGui::IsKeyPressed(ImGuiKey_UpArrow)?(row?row-1:0):std::min(row+1,lines.size()-1);move(positionAtX(text,lines[target],x));}
        if(ImGui::IsKeyPressed(ImGuiKey_Backspace)||ImGui::IsKeyPressed(ImGuiKey_Delete)){
            const bool back=ImGui::IsKeyPressed(ImGuiKey_Backspace);
            if(edit.cursor!=edit.anchor||(back?edit.cursor>0:edit.cursor<text.size())){
                checkpoint();if(edit.cursor==edit.anchor){if(back)edit.anchor=previousCodepoint(text,edit.cursor);else edit.anchor=nextCodepoint(text,edit.cursor);}replaceSelection("");}}
        if(!ctrl&&!io.KeyAlt){std::string typed;
            for(ImWchar cp:io.InputQueueCharacters)if(cp>=32&&cp!=127){char encoded[5]{};ImTextCharToUtf8(encoded,cp);typed.append(encoded);}
            if(!typed.empty()){checkpoint();replaceSelection(typed);}
            if(ImGui::IsKeyPressed(ImGuiKey_Enter)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)){checkpoint();replaceSelection("\n");}}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){edit.focused=false;ImGui::ClearActiveID();}
    }else if(GImGui->ActiveId==id)ImGui::ClearActiveID();
    if(changed){edit.observed=text;lines=LayoutWeaponDescription(text,width);}
    auto* draw=ImGui::GetWindowDrawList();const size_t first=std::min(edit.anchor,edit.cursor),last=std::max(edit.anchor,edit.cursor);
    for(size_t row=0;row<lines.size();++row){const auto line=lines[row];const float y=origin.y+float(row)*lineHeight;
        if(y+lineHeight<window->InnerClipRect.Min.y||y>window->InnerClipRect.Max.y)continue;
        if(edit.focused&&first<last&&last>line.begin&&first<line.end){const size_t a=std::max(first,line.begin),b=std::min(last,line.end);
            draw->AddRectFilled(ImVec2(origin.x+textWidth(text,line.begin,a),y),ImVec2(origin.x+textWidth(text,line.begin,b),y+lineHeight),ImGui::GetColorU32(ImGuiCol_TextSelectedBg));}
        draw->AddText(ImVec2(origin.x,y),ImGui::GetColorU32(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled),text.data()+line.begin,text.data()+line.end);
    }
    if(edit.focused){const size_t row=cursorLine(lines,edit.cursor);const float y=origin.y+float(row)*lineHeight,x=origin.x+textWidth(text,lines[row].begin,edit.cursor);
        if(std::fmod(ImGui::GetTime(),1.1)<.8)draw->AddLine(ImVec2(x,y),ImVec2(x,y+ImGui::GetTextLineHeight()),ImGui::GetColorU32(ImGuiCol_Text));
        if(moved){const float local=float(row)*lineHeight,top=ImGui::GetScrollY(),bottom=top+ImGui::GetWindowHeight()-lineHeight*2;
            if(local<top)ImGui::SetScrollY(local);else if(local>bottom)ImGui::SetScrollY(local-ImGui::GetWindowHeight()+lineHeight*2);}}
    ImGui::EndChild();return changed;
}
void tip(const std::string& text) {
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*30);ImGui::TextUnformatted(text.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
}
bool contains(const std::string& text,const std::string& pattern) {
    // ASCII case folding keeps UTF-8 intact. Cyrillic queries are matched literally.
    auto fold=[](std::string s){for(char& c:s)if(c>='A'&&c<='Z')c=char(c+'a'-'A');return s;};
    return pattern.empty()||fold(text).find(fold(pattern))!=std::string::npos;
}
bool fullButton(const char* label){return ImGui::Button(label,ImVec2(-1,0));}
void checker(ImDrawList* draw,ImVec2 p,ImVec2 size,float cell) {
    draw->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),IM_COL32(48,51,57,255));
    const int nx=int(std::ceil(size.x/cell)),ny=int(std::ceil(size.y/cell));
    for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)if((x+y)&1)
        draw->AddRectFilled(ImVec2(p.x+x*cell,p.y+y*cell),ImVec2(p.x+std::min(size.x,(x+1)*cell),p.y+std::min(size.y,(y+1)*cell)),IM_COL32(70,74,80,255));
}
void imagePreview(ImTextureID texture,int width,int height,float maxWidth,float maxHeight,ImVec2 uv0=ImVec2(0,0),ImVec2 uv1=ImVec2(1,1)) {
    if(!texture||width<1||height<1)return;
    const float scale=std::min(maxWidth/width,maxHeight/height);
    ImVec2 size(std::max(1.f,width*scale),std::max(1.f,height*scale)),p=ImGui::GetCursorScreenPos();
    checker(ImGui::GetWindowDrawList(),p,size,10.f);ImGui::Image(texture,size,uv0,uv1);
}
int columns(const WeaponUIState& s){return s.cellPixels>0?std::max(0,s.atlasWidth/s.cellPixels):0;}
int rows(const WeaponUIState& s){return s.cellPixels>0?std::max(0,s.atlasHeight/s.cellPixels):0;}
bool validRect(const WeaponUIState& s){return s.iconX>=0&&s.iconY>=0&&s.iconW>0&&s.iconH>0&&s.iconX<=columns(s)&&s.iconY<=rows(s)&&s.iconW<=columns(s)-s.iconX&&s.iconH<=rows(s)-s.iconY;}
void currentIcon(WeaponUIState& s) {
    if(!s.atlasTexture||s.atlasWidth<=0||s.atlasHeight<=0)return;
    const int x=s.currentIconX,y=s.currentIconY,w=s.currentIconW,h=s.currentIconH;
    if(x<0||y<0||w<1||h<1||x>columns(s)||y>rows(s)||w>columns(s)-x||h>rows(s)-y){
        ImGui::TextColored(ImVec4(1,.65f,.3f,1),"Координаты вне атласа");return;}
    const float cell=float(s.cellPixels);
    imagePreview(s.atlasTexture,w*s.cellPixels,h*s.cellPixels,ImGui::GetContentRegionAvail().x,90*s.uiScale,
        ImVec2(x*cell/s.atlasWidth,y*cell/s.atlasHeight),ImVec2((x+w)*cell/s.atlasWidth,(y+h)*cell/s.atlasHeight));
}
void catalog(WeaponUIState& s,const WeaponUICallback& action) {
    ImGui::TextUnformatted("ОРУЖИЕ");ImGui::SetNextItemWidth(-1);textInput("##WeaponSearch",s.search);tip("Поиск по названию и секции");
    ImGui::TextDisabled("Секций: %d",int(s.weapons.size()));
    const float iconHeight=s.selected>=0?155*s.uiScale:0;
    ImGui::BeginChild("WeaponList",ImVec2(0,std::max(80.f,ImGui::GetContentRegionAvail().y-iconHeight)),ImGuiChildFlags_Borders);
    for(size_t i=0;i<s.weapons.size();++i){const auto& row=s.weapons[i];if(!contains(row.label+" "+row.section,s.search))continue;
        ImGui::PushID(int(i));const std::string label=row.label+"\n["+row.section+"]";
        if(ImGui::Selectable(label.c_str(),s.selected==int(i),0,ImVec2(0,ImGui::GetTextLineHeight()*2+6)))action(WeaponAction::SelectWeapon,int(i));
        tip(row.source+(row.missingModel?"\nWorld-модель не найдена":""));ImGui::PopID();}
    ImGui::EndChild();
    if(s.selected>=0){ImGui::SeparatorText("Иконка");currentIcon(s);if(fullButton("Редактор иконки…"))s.iconEditorOpen=true;}
}
void parameterPane(WeaponUIState& s,const WeaponUICallback& action) {
    ImGui::SetNextItemWidth(-1);textInput("##ParameterSearch",s.parameterSearch);tip("Фильтр параметров по имени");
    ImGui::TextWrapped("Ползунки изменяют значение дискретно. Шаг и границы задаются с клавиатуры.");
    for(size_t i=0;i<s.parameters.size();++i){auto& p=s.parameters[i];if(!contains(p.key,s.parameterSearch))continue;
        ImGui::PushID(int(i));ImGui::Separator();ImGui::TextWrapped("%s%s",p.key.c_str(),p.inherited?"  [наследуется]":"");tip(p.source);
        const auto meaning=DescribeSimulationParameter(p.key);
        if(meaning.relevant){ImGui::TextWrapped("%s",meaning.title.c_str());tip(meaning.description);ImGui::TextWrapped("%s",meaning.effect.c_str());}
        bool changed=false,activated=false;
        if(!p.numbers.empty()){
            if(!(p.step>0&&std::isfinite(p.step)))p.step=p.integer?1:.01;
            if(!std::isfinite(p.minimum)||!std::isfinite(p.maximum)||p.maximum<=p.minimum){p.minimum=0;p.maximum=1;}
            const double count=std::floor((p.maximum-p.minimum)/p.step+1e-9);
            const bool sliderAvailable=count>=1&&count<=1000000;
            for(size_t component=0;component<p.numbers.size();++component){
                ImGui::PushID(int(component));const double value=p.numbers[component];
                if(sliderAvailable){const int ticks=int(count);int tick=int(std::clamp(std::round((value-p.minimum)/p.step),0.0,double(ticks)));
                    const std::string display=numberText(value,p.integer);
                    ImGui::SetNextItemWidth(-1);
                    if(ImGui::SliderInt("##StepSlider",&tick,0,ticks,display.c_str(),ImGuiSliderFlags_NoInput)){
                        p.numbers[component]=WeaponSteppedValue(p.minimum,p.maximum,p.step,tick);changed=true;
                    }
                    activated|=ImGui::IsItemActivated();
                    if(ImGui::IsItemHovered()){s.inspectorWheelUsed=true;ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);const float wheel=ImGui::GetIO().MouseWheel;
                        if(wheel!=0){activated=true;tick=std::clamp(tick+(wheel>0?1:-1),0,ticks);const double next=WeaponSteppedValue(p.minimum,p.maximum,p.step,tick);changed|=next!=value;p.numbers[component]=next;}}
                    if(value<p.minimum||value>p.maximum)ImGui::TextDisabled("Вне диапазона: %s",display.c_str());
                } else ImGui::TextDisabled("Слишком много делений; увеличьте шаг.");
                ImGui::PopID();
            }
            if(changed)rebuild(p);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);
            if(ImGui::InputDouble("Шаг",&p.step,0,0,"%.10g")){
                if(!std::isfinite(p.step)||p.step<=0)p.step=p.integer?1:.01;
                // Fractional steps are valid even for an initially integral value.
                if(std::floor(p.step)!=p.step)p.integer=false;
            }
            if(ImGui::TreeNode("Диапазон")){
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputDouble("От",&p.minimum,0,0,"%.10g");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputDouble("До",&p.maximum,0,0,"%.10g");ImGui::TreePop();}
        }
        ImGui::SetNextItemWidth(-1);
        if(textInput("##RawValue",p.value)){
            RefreshWeaponParameterNumbers(p);changed=true;
        }
        activated|=ImGui::IsItemActivated();
        tip("Точное значение. Числовые векторы разделяются запятыми. Ошибочный ввод остаётся черновиком и блокирует сохранение.");
        if(meaning.relevant&&s.simulationAvailable&&(activated||changed)){
            s.view=(p.key=="bullet_speed"||p.key=="fire_distance")?WeaponView::Trajectory:WeaponView::Spread;
            action(WeaponAction::FocusParameter,int(i));
        }
        if(changed){p.pending=true;action(WeaponAction::ChangeParameter,int(i));}
        ImGui::PopID();
    }
}
void textPane(WeaponUIState& s,const WeaponUICallback& action) {
    ImGui::TextWrapped("Тексты записываются по ID в выбранную XML-таблицу. Один ID может использоваться несколькими секциями.");
    ImGui::SeparatorText("Название");ImGui::TextWrapped("ID: %s",s.nameId.c_str());
    ImGui::BeginDisabled(!s.canEditName);ImGui::SetNextItemWidth(-1);
    bool changed=textInput("##WeaponName",s.name);ImGui::EndDisabled();
    ImGui::SeparatorText("Описание");ImGui::TextWrapped("ID: %s",s.descriptionId.c_str());
    ImGui::BeginDisabled(!s.canEditDescription);
    changed|=wrappedDescription(s,ImVec2(-1,std::max(160.f,ImGui::GetContentRegionAvail().y-70*s.uiScale)));ImGui::EndDisabled();
    if(changed)action(WeaponAction::ChangeTexts,0);
    ImGui::TextWrapped("Изменения сохраняются кнопкой «Сохранить» вместе с конфигами.");
}
void scenePane(WeaponUIState& s,const WeaponUICallback& action) {
    ImGui::SeparatorText("Освещение");
    bool changed=ImGui::Checkbox("Направленный свет",&s.directedLight);
    ImGui::BeginDisabled(!s.directedLight);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);changed|=ImGui::SliderFloat("Сила света",&s.lightIntensity,0,8,"%.2f");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);changed|=ImGui::SliderFloat("Азимут",&s.lightAzimuth,0,360,"%.0f°");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);changed|=ImGui::SliderFloat("Высота",&s.lightElevation,-80,80,"%.0f°");
    changed|=ImGui::ColorEdit3("Цвет света",s.lightColor,ImGuiColorEditFlags_NoInputs);ImGui::EndDisabled();
    changed|=ImGui::ColorEdit3("Ambient",s.ambient,ImGuiColorEditFlags_NoInputs);
    changed|=ImGui::ColorEdit3("Окружение",s.environment,ImGuiColorEditFlags_NoInputs);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);changed|=ImGui::SliderFloat("Полусфера",&s.hemisphere,0,1,"%.2f");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);changed|=ImGui::SliderFloat("Экспозиция",&s.exposure,0,4,"%.2f");
    if(changed)action(WeaponAction::LightSettings,0);
    if(ImGui::ColorEdit3("Фон окна",s.background,ImGuiColorEditFlags_NoInputs))action(WeaponAction::BackgroundSettings,0);
    ImGui::TextWrapped("Для icon_snapshot можно выключить направленный свет и оставить только ambient и окружение. Фон окна в PNG не попадает.");
    ImGui::SeparatorText("Части модели");
    if(!s.meshes.empty()){
        if(fullButton("Показать все части"))action(WeaponAction::ShowAllParts,0);
        for(size_t i=0;i<s.meshes.size();++i){ImGui::PushID(int(i));
            if(ImGui::Checkbox(s.meshes[i].label.c_str(),&s.meshes[i].visible))action(WeaponAction::PartVisibility,int(i));
            ImGui::PopID();}
        ImGui::TextWrapped("Видимость действует и на icon_snapshot; исходный OGF не меняется.");
    }
    ImGui::SeparatorText("Вид");
    if(ImGui::Checkbox("Каркас",&s.wireframe))action(WeaponAction::Wireframe,0);
    if(fullButton("Вписать модель"))action(WeaponAction::Frame,0);
    if(fullButton("Сбросить вид"))action(WeaponAction::ResetView,0);
    ImGui::TextWrapped("%s",s.modelName.c_str());
}
void modelViewport(WeaponUIState& s,const WeaponUICallback& action) {
    const ImVec2 start=ImGui::GetCursorScreenPos();
    const char* names[]={"FLY","ORB","PAN","LIGHT","MODEL"};
    const char* tips[]={"Свободная камера: ПКМ — обзор, WASD/QE — движение, Shift — быстрее","Орбита: ЛКМ или ПКМ — вращение камеры","Сдвиг камеры: ЛКМ — перетаскивание; средняя кнопка — сдвиг в любом режиме","Свет: ЛКМ — направление; ПКМ — камера","Модель: ЛКМ — вращение; ПКМ — камера"};
    for(int i=0;i<5;++i){if(i)ImGui::SameLine();const bool active=int(s.mode)==i;if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.20f,.42f,.57f,1));
        if(ImGui::Button(names[i])){s.mode=WeaponMode(i);action(WeaponAction::SetMode,i);}tip(tips[i]);if(active)ImGui::PopStyleColor();}
    const char* axes[]={"+X","+Y","+Z","-X","-Y","-Z"};
    for(int i=0;i<6;++i){if(i)ImGui::SameLine();if(ImGui::Button(axes[i]))action(WeaponAction::AxisView,i);}
    ImGui::SameLine();if(ImGui::Button("Вписать"))action(WeaponAction::Frame,0);
    s.toolbarRect={start.x,start.y,ImGui::GetContentRegionAvail().x,ImGui::GetCursorScreenPos().y-start.y};
    const ImVec2 p=ImGui::GetCursorScreenPos();ImVec2 size=ImGui::GetContentRegionAvail();size.x=std::max(1.f,size.x);size.y=std::max(1.f,size.y);
    ImGui::InvisibleButton("WeaponViewport",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
    s.viewportRect={p.x,p.y,size.x,size.y};s.viewportHovered=ImGui::IsItemHovered();s.viewportClicked=ImGui::IsItemClicked();s.viewportActive=ImGui::IsItemActive();
    auto* draw=ImGui::GetWindowDrawList();
    if(s.viewportTexture)draw->AddImage(s.viewportTexture,p,ImVec2(p.x+size.x,p.y+size.y));else draw->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),IM_COL32(24,27,31,255));
    if(!s.hasModel)draw->AddText(ImVec2(p.x+16,p.y+16),IM_COL32(180,188,198,255),"Выберите оружие из списка");
}
std::string fireModeName(int mode){return mode<0?"Автоматический":mode==1?"Одиночный":"Очередь "+std::to_string(mode);}
void simulationViewport(WeaponUIState& s,const WeaponUICallback& action) {
    s.viewportHovered=s.viewportClicked=s.viewportActive=false;
    bool changed=false;
    ImGui::BeginChild("SimulationScroll",ImVec2(0,0));
    ImGui::TextWrapped("Лабораторный стенд: ствол неподвижен, без движения стрелка и увода камеры.");
    const char* ammo=s.selectedAmmoIndex>=0&&size_t(s.selectedAmmoIndex)<s.ammunition.size()?s.ammunition[size_t(s.selectedAmmoIndex)].label.c_str():"Боеприпас не найден";
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##Ammo",ammo)){
        for(size_t i=0;i<s.ammunition.size();++i)if(ImGui::Selectable(s.ammunition[i].label.c_str(),s.selectedAmmoIndex==int(i))){s.selectedAmmoIndex=int(i);changed=true;}
        ImGui::EndCombo();}
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##FireMode",fireModeName(s.selectedFireMode).c_str())){
        for(int mode:s.simulation.fireModes)if(ImGui::Selectable(fireModeName(mode).c_str(),s.selectedFireMode==mode)){s.selectedFireMode=mode;changed=true;}
        ImGui::EndCombo();}
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.65f);
    changed|=ImGui::SliderFloat("Состояние",&s.condition,0,1,"%.3f");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.65f);
    changed|=ImGui::InputFloat("Пауза, с",&s.triggerPause,.05f,.25f,"%.2f");s.triggerPause=std::clamp(s.triggerPause,0.f,60.f);
    tip("Задаваемая пауза между нажатиями спуска. Внутри очереди интервал задаётся rpm. Это условие стенда, не параметр LTX.");
    if(s.view==WeaponView::Spread){ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.65f);changed|=ImGui::SliderFloat("Дальность, м",&s.spreadDistance,10,100,"%.0f");}
    else {ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.65f);changed|=ImGui::SliderFloat("Дальность, м",&s.trajectoryDistance,100,300,"%.0f");}
    if(s.view==WeaponView::Spread){
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.65f);ImGui::InputFloat("Радиус, см",&s.targetRadiusCm,5,25,"%.1f");
        if(!std::isfinite(s.targetRadiusCm))s.targetRadiusCm=50;s.targetRadiusCm=std::clamp(s.targetRadiusCm,.1f,10000.f);
        tip("Фиксированный размер мишени: при изменении параметров масштаб не меняется, поэтому рост разброса виден напрямую.");
        if(ImGui::Button("Вписать группу")){double extent=s.simulation.maxConeRadius;for(const auto& p:s.simulation.spread)extent=std::max(extent,std::hypot(p.x,p.y));s.targetRadiusCm=float(std::clamp(extent*105.0,.1,10000.0));}
    }
    if(ImGui::Button("Новый образец")){++s.seed;changed=true;}ImGui::SameLine();ImGui::TextDisabled("Полный магазин");
    if(changed)action(WeaponAction::SimulationSettings,0);
    const auto& r=s.simulation;
    if(!r.error.empty())ImGui::TextWrapped("%s",r.error.c_str());
    const auto top=ImGui::GetCursorScreenPos();const float available=std::max(100.f,ImGui::GetContentRegionAvail().x);
    const float chartHeight=std::clamp(std::min(available*.65f,ImGui::GetContentRegionAvail().y-20.f),160.f,380.f);const ImVec2 size(available,chartHeight);
    ImGui::InvisibleButton("LaboratoryChart",size);s.viewportRect={top.x,top.y,size.x,size.y};
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(top,ImVec2(top.x+size.x,top.y+size.y),IM_COL32(20,26,32,255),4.f);
    draw->PushClipRect(top,ImVec2(top.x+size.x,top.y+size.y),true);
    if(s.view==WeaponView::Spread&&r.spreadValid){
        const float radius=std::max(10.f,std::min(size.x,size.y)*.39f);const ImVec2 center(top.x+size.x*.5f,top.y+size.y*.51f);
        const double extent=std::max(.001,double(s.targetRadiusCm)*.01);size_t outside=0;
        for(int ring=1;ring<=4;++ring)draw->AddCircle(center,radius*ring/4,IM_COL32(83,103,118,180),64);
        draw->AddLine(ImVec2(center.x-radius,center.y),ImVec2(center.x+radius,center.y),IM_COL32(83,103,118,200));
        draw->AddLine(ImVec2(center.x,center.y-radius),ImVec2(center.x,center.y+radius),IM_COL32(83,103,118,200));
        for(const auto& p:r.spread){if(std::hypot(p.x,p.y)>extent){++outside;continue;}const float fraction=r.roundsFired>1?float(p.shot)/float(r.roundsFired-1):0;
            const ImU32 color=ImGui::ColorConvertFloat4ToU32(ImVec4(.35f+.65f*fraction,.82f-.44f*fraction,1.f-.55f*fraction,.85f));
            draw->AddCircleFilled(ImVec2(center.x+float(p.x/extent)*radius,center.y-float(p.y/extent)*radius),2.3f*s.uiScale,color);}
        const std::string label="Радиус внешнего круга: "+shortNumber(extent*100,1)+" см";
        draw->AddText(ImVec2(top.x+10,top.y+8),IM_COL32(220,230,240,255),label.c_str());
        const std::string footer=outside?"За мишенью: "+std::to_string(outside)+" / "+std::to_string(r.spread.size()):"Синий → оранжевый: порядок выстрелов";
        draw->AddText(ImVec2(top.x+10,top.y+size.y-24),IM_COL32(180,196,210,255),footer.c_str());
    }else if(s.view==WeaponView::Trajectory&&r.trajectoryValid&&!r.trajectory.empty()){
        const float left=42,right=size.x-16,upper=32,lower=size.y-32;
        double minY=0,maxY=0;for(const auto& p:r.trajectory){minY=std::min(minY,p.height);maxY=std::max(maxY,p.height);}
        const double span=std::max(.05,maxY-minY);minY-=span*.08;maxY+=span*.15;
        const double xMax=std::max(1.f,s.trajectoryDistance);
        auto point=[&](double x,double y){return ImVec2(top.x+left+float(x/xMax)*(right-left),top.y+upper+float((maxY-y)/(maxY-minY))*(lower-upper));};
        for(int tick=0;tick<=4;++tick){const float x=left+(right-left)*tick/4;
            draw->AddLine(ImVec2(top.x+x,top.y+upper),ImVec2(top.x+x,top.y+lower),IM_COL32(65,81,96,180));
            const auto text=numberText(xMax*tick/4,true);draw->AddText(ImVec2(top.x+x-10,top.y+lower+6),IM_COL32(180,196,210,255),text.c_str());}
        draw->AddLine(point(0,0),point(xMax,0),IM_COL32(190,192,180,220),1.5f);
        for(size_t i=1;i<r.trajectory.size();++i)draw->AddLine(point(r.trajectory[i-1].distance,r.trajectory[i-1].height),point(r.trajectory[i].distance,r.trajectory[i].height),IM_COL32(241,189,83,255),2.f);
        draw->AddText(ImVec2(top.x+10,top.y+8),IM_COL32(220,230,240,255),"Снижение от линии выстрела");
        draw->AddText(ImVec2(top.x+10,top.y+32),IM_COL32(180,196,210,255),(shortNumber(maxY,2)+" м").c_str());
        draw->AddText(ImVec2(top.x+10,top.y+lower-16),IM_COL32(180,196,210,255),(shortNumber(minY,2)+" м").c_str());
    }else draw->AddText(ImVec2(top.x+12,top.y+12),IM_COL32(235,187,114,255),"Нет достаточных данных для расчёта");
    draw->PopClipRect();
    ImGui::TextWrapped("Магазин: %d патр. · %d попаданий/дробин · %.0f выстр./мин",r.magazineSize,int(r.spread.size()),r.rpm);
    ImGui::TextWrapped("Первый → последний выстрел: %.3f с; состояние %.4f → %.4f (износ %.4f).",r.magazineDuration,r.initialCondition,r.conditionAfterMagazine,r.magazineWear);
    if(r.lifetimeCapped)ImGui::TextWrapped("Расчёт ресурса остановлен по ограничению стенда.");
    else if(r.lifetimeFinite)ImGui::TextWrapped("До состояния 0: %llu выстр.; %llu полных магазинов + %d патр.",static_cast<unsigned long long>(r.shotsToZero),static_cast<unsigned long long>(r.fullMagazinesToZero),r.remainingRoundsToZero);

    else ImGui::TextWrapped("При заданном износе конечный ресурс до состояния 0 не определён.");
    if(!r.shots.empty())ImGui::TextWrapped("Износ за выстрел: %.6f → %.6f (первый → последний).",r.shots.front().conditionBefore-r.shots.front().conditionAfter,r.shots.back().conditionBefore-r.shots.back().conditionAfter);
    if(!r.shots.empty())ImGui::TextWrapped("Вероятность осечки: %.3f%% → %.3f%% (первый → последний).",r.shots.front().misfireProbability*100,r.shots.back().misfireProbability*100);
    ImGui::TextWrapped("Выстрелы до состояния 0 — расчёт износа, не запрет дальнейшей стрельбы и не гарантия отсутствия осечек.");
    if(s.view==WeaponView::Trajectory){
        if(r.trajectoryReached)ImGui::TextWrapped("На %.0f м: снижение %.3f м; время %.3f с; скорость %.1f м/с.",s.trajectoryDistance,r.dropAtTarget,r.timeAtTarget,r.speedAtTarget);
        else ImGui::TextWrapped("Пуля не достигла выбранной дальности: %s",r.stopReason.c_str());
    }
    for(const auto& warning:r.warnings)ImGui::TextWrapped("%s",warning.c_str());
    ImGui::EndChild();
}
void viewport(WeaponUIState& s,const WeaponUICallback& action) {
    s.viewportHovered=s.viewportActive=s.viewportClicked=false;
    const char* names[]={"Модель","Разброс","Траектория"};
    for(int i=0;i<3;++i){if(i)ImGui::SameLine();ImGui::BeginDisabled(i!=0&&!s.simulationAvailable);
        if(ImGui::Button(names[i])){s.view=WeaponView(i);action(WeaponAction::SimulationView,i);}ImGui::EndDisabled();}
    if(!s.simulationAvailable)s.view=WeaponView::Model;
    if(s.view==WeaponView::Model)modelViewport(s,action);else simulationViewport(s,action);
}
void setup(WeaponUIState& s,const WeaponUICallback& action) {
    if(s.setupOpen&&!ImGui::IsPopupOpen("Источники WeaponLab"))ImGui::OpenPopup("Источники WeaponLab");
    const auto& io=ImGui::GetIO();ImGui::SetNextWindowSize(ImVec2(std::min(760.f*s.uiScale,io.DisplaySize.x-24),std::min(620.f*s.uiScale,io.DisplaySize.y-24)),ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Источники WeaponLab",&s.setupOpen,ImGuiWindowFlags_NoSavedSettings)){
        ImGui::TextWrapped("Выберите gamedata. Можно использовать стандартные файлы ТЧ 1.0006 или назначить свои пути ниже. Источники доступны для переназначения в любой момент.");
        const char* labels[]={"gamedata","Конфиги оружия","Названия и описания (XML)","Атлас иконок (DDS)","World-модели (meshes)","Текстуры (textures)"};
        for(int i=0;i<6;++i){ImGui::PushID(i);ImGui::TextUnformatted(labels[i]);ImGui::SetNextItemWidth(std::max(80.f,ImGui::GetContentRegionAvail().x-46*s.uiScale));textInput("##Path",s.paths.values[size_t(i)]);ImGui::SameLine();if(ImGui::Button("…"))action(WeaponAction::BrowsePath,i);ImGui::PopID();
            if(i==0&&fullButton("Использовать стандартные файлы этой gamedata"))action(WeaponAction::DefaultPaths,0);}
        ImGui::Spacing();
        if(fullButton("Применить пути и прочитать оружие"))action(WeaponAction::ApplyPaths,0);
        ImGui::TextWrapped("%s",s.status.c_str());
        if(!s.warning.empty())ImGui::TextWrapped("%s",s.warning.c_str());
        ImGui::EndPopup();
    }
}
void iconEditor(WeaponUIState& s,const WeaponUICallback& action) {
    if(!s.iconEditorOpen)return;
    const std::array<int,5> before{{s.iconX,s.iconY,s.iconW,s.iconH,s.iconPadding}};
    const auto& io=ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(std::min(1050.f*s.uiScale,io.DisplaySize.x-28),std::min(760.f*s.uiScale,io.DisplaySize.y-28)),ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(14,14),ImGuiCond_Appearing);
    if(!ImGui::Begin("Редактор иконки — WeaponLab",&s.iconEditorOpen,ImGuiWindowFlags_NoSavedSettings)){ImGui::End();return;}
    const float panel=std::min(290*s.uiScale,ImGui::GetContentRegionAvail().x*.38f);
    ImGui::BeginChild("IconControls",ImVec2(panel,0),ImGuiChildFlags_Borders);
    ImGui::TextWrapped("Атлас: %d×%d px\nЯчейка: %d×%d px; сетка: %d×%d",s.atlasWidth,s.atlasHeight,s.cellPixels,s.cellPixels,columns(s),rows(s));
    ImGui::TextWrapped("Ячейки не растягиваются. Прокрутка открывает весь атлас; в окне видно до 20×20 клеток. ЛКМ задаёт прямоугольник.");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputInt("X",&s.iconX);ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputInt("Y",&s.iconY);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputInt("Ширина",&s.iconW);ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputInt("Высота",&s.iconH);
    const bool valid=validRect(s);
    if(!valid)ImGui::TextColored(ImVec4(1,.55f,.35f,1),"Прямоугольник вне сетки");
    ImGui::BeginDisabled(!valid||s.selected<0);
    if(fullButton("Выбрать эту иконку для оружия"))action(WeaponAction::ApplyIconRect,0);
    ImGui::EndDisabled();
    ImGui::SeparatorText("icon_snapshot");
    ImGui::TextWrapped("Расположите модель в основном окне. Окно редактора можно переместить или свернуть.");
    if(ImGui::Checkbox("Направленный свет",&s.directedLight))action(WeaponAction::LightSettings,0);
    bool light=ImGui::ColorEdit3("Ambient",s.ambient,ImGuiColorEditFlags_NoInputs);light|=ImGui::ColorEdit3("Окружение",s.environment,ImGuiColorEditFlags_NoInputs);if(light)action(WeaponAction::LightSettings,0);
    ImGui::BeginDisabled(!s.hasModel);if(fullButton("Сделать icon_snapshot"))action(WeaponAction::CaptureIcon,0);ImGui::EndDisabled();
    if(s.hasSnapshot){imagePreview(s.snapshotTexture,s.snapshotWidth,s.snapshotHeight,ImGui::GetContentRegionAvail().x,100*s.uiScale);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::InputInt("Поля, px",&s.iconPadding);s.iconPadding=std::clamp(s.iconPadding,0,50);
        ImGui::BeginDisabled(!valid);if(fullButton("Вставить снимок в выделение"))action(WeaponAction::StageIcon,0);ImGui::EndDisabled();
        if(fullButton("Сохранить снимок PNG…"))action(WeaponAction::ExportSnapshot,0);}
    ImGui::TextWrapped("Вставка очищает весь выделенный прямоугольник. Соседние ячейки сохраняются. Размер модели подгоняется с сохранением пропорций.");
    ImGui::BeginDisabled(!s.atlasDirty);if(fullButton("Сохранить DDS-атлас"))action(WeaponAction::SaveAtlas,0);if(fullButton("Отменить правки атласа"))action(WeaponAction::DiscardAtlas,0);ImGui::EndDisabled();
    ImGui::TextWrapped("DDS сохраняется без сжатия: RGBA32, один mip-уровень. Это сохраняет соседние пиксели атласа без повторного BC-сжатия. Координаты сохраняются кнопкой «Сохранить» главного окна.");
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("AtlasArea",ImVec2(0,0),ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.6f);ImGui::SliderFloat("Масштаб",&s.atlasZoom,.25f,2.f,"%.2fx");
    const float maxVisible=20.f*s.cellPixels*s.atlasZoom;
    const ImVec2 atlasAvailable=ImGui::GetContentRegionAvail();
    ImGui::BeginChild("AtlasScroll",ImVec2(std::min(atlasAvailable.x,maxVisible+ImGui::GetStyle().ScrollbarSize),std::min(atlasAvailable.y,maxVisible+ImGui::GetStyle().ScrollbarSize)),0,ImGuiWindowFlags_HorizontalScrollbar);
    const int nx=columns(s),ny=rows(s);
    if(s.atlasTexture&&nx>0&&ny>0){
        const float cell=s.cellPixels*s.atlasZoom;const ImVec2 p=ImGui::GetCursorScreenPos(),size(nx*cell,ny*cell);
        checker(ImGui::GetWindowDrawList(),p,size,std::max(4.f,cell*.5f));
        ImGui::InvisibleButton("AtlasGrid",size);s.atlasRect={p.x,p.y,size.x,size.y};
        auto* draw=ImGui::GetWindowDrawList();draw->AddImage(s.atlasTexture,p,ImVec2(p.x+size.x,p.y+size.y),ImVec2(0,0),ImVec2(float(nx*s.cellPixels)/s.atlasWidth,float(ny*s.cellPixels)/s.atlasHeight));
        for(int x=0;x<=nx;++x)draw->AddLine(ImVec2(p.x+x*cell,p.y),ImVec2(p.x+x*cell,p.y+size.y),IM_COL32(180,193,209,105));
        for(int y=0;y<=ny;++y)draw->AddLine(ImVec2(p.x,p.y+y*cell),ImVec2(p.x+size.x,p.y+y*cell),IM_COL32(180,193,209,105));
        if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            s.atlasDragX=std::clamp(int((io.MousePos.x-p.x)/cell),0,nx-1);s.atlasDragY=std::clamp(int((io.MousePos.y-p.y)/cell),0,ny-1);
            s.iconX=s.atlasDragX;s.iconY=s.atlasDragY;s.iconW=s.iconH=1;
        }
        if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left)&&s.atlasDragX>=0){
            const int x=std::clamp(int((io.MousePos.x-p.x)/cell),0,nx-1),y=std::clamp(int((io.MousePos.y-p.y)/cell),0,ny-1);
            s.iconX=std::min(x,s.atlasDragX);s.iconY=std::min(y,s.atlasDragY);s.iconW=std::abs(x-s.atlasDragX)+1;s.iconH=std::abs(y-s.atlasDragY)+1;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))s.atlasDragX=s.atlasDragY=-1;
        if(validRect(s))draw->AddRect(ImVec2(p.x+s.iconX*cell,p.y+s.iconY*cell),ImVec2(p.x+(s.iconX+s.iconW)*cell,p.y+(s.iconY+s.iconH)*cell),IM_COL32(255,201,77,255),0,0,3.f);
    }else ImGui::TextWrapped("Атлас не загружен. Назначьте DDS в «Источниках».");
    ImGui::EndChild();ImGui::EndChild();ImGui::End();
    if(s.hasSnapshot&&validRect(s)&&before!=std::array<int,5>{{s.iconX,s.iconY,s.iconW,s.iconH,s.iconPadding}})action(WeaponAction::PreviewSnapshot,0);
}
} // namespace
std::vector<WrappedTextLine> LayoutWeaponDescription(const std::string& text,float maxWidth) {
    maxWidth=std::max(1.f,maxWidth);std::vector<WrappedTextLine> lines;
    size_t first=0,at=0,lastBreak=0;float width=0;
    while(at<text.size()){
        if(text[at]=='\n'){lines.push_back({first,at});first=++at;lastBreak=first;width=0;continue;}
        const size_t next=nextCodepoint(text,at);const float advance=text[at]=='\r'?0:textWidth(text,at,next);
        if(width+advance>maxWidth&&at>first){const size_t end=lastBreak>first?lastBreak:at;lines.push_back({first,end});first=at=end;lastBreak=first;width=0;continue;}
        width+=advance;at=next;if(text[next-1]==' '||text[next-1]=='\t')lastBreak=next;
    }
    lines.push_back({first,text.size()});return lines;
}
WeaponParameter MakeWeaponParameter(std::string key,std::string value,std::string source,bool inherited) {
    WeaponParameter result;result.key=std::move(key);result.value=std::move(value);result.source=std::move(source);result.inherited=inherited;
    result.numbers=parseNumbers(result.value);result.numericControlsInitialized=!result.numbers.empty();
    result.integer=!result.numbers.empty()&&result.value.find_first_of(".eE")==std::string::npos;
    result.step=result.integer?1:.01;
    if(!result.numbers.empty()){
        const auto range=std::minmax_element(result.numbers.begin(),result.numbers.end());
        const double magnitude=std::max({std::abs(*range.first),std::abs(*range.second),1.0});
        result.minimum=*range.first<0?-magnitude*2:0;result.maximum=magnitude*2;
        if(result.maximum-result.minimum>result.step*1000000)result.step=std::pow(10.0,std::ceil(std::log10((result.maximum-result.minimum)/1000000)));
    }
    return result;
}
void RefreshWeaponParameterNumbers(WeaponParameter& p) {
    p.numbers=parseNumbers(p.value);
    if(p.numbers.empty()){p.integer=false;return;}
    if(!p.numericControlsInitialized){
        const auto initialized=MakeWeaponParameter(p.key,p.value,p.source,p.inherited);
        p.step=initialized.step;p.minimum=initialized.minimum;p.maximum=initialized.maximum;
        p.numericControlsInitialized=true;
    }
    // Transient empty/invalid input never erases a previously chosen step.
    p.integer=p.value.find_first_of(".eE")==std::string::npos&&std::floor(p.step)==p.step&&
        std::all_of(p.numbers.begin(),p.numbers.end(),[](double value){return std::floor(value)==value;});
    for(double value:p.numbers){p.minimum=std::min(p.minimum,value);p.maximum=std::max(p.maximum,value);}
}
double WeaponSteppedValue(double minimum,double maximum,double step,int tick) {
    if(!std::isfinite(minimum)||!std::isfinite(maximum)||!std::isfinite(step)||step<=0||maximum<minimum)return minimum;
    const double count=std::floor((maximum-minimum)/step+1e-9);
    const double bounded=std::clamp(double(tick),0.0,std::max(0.0,count));
    return std::clamp(minimum+bounded*step,minimum,maximum);
}
void ConfigureWeaponUIStyle(float scale) {
    ImGui::GetStyle()=ImGuiStyle{};ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();
    style.WindowRounding=0;style.ChildRounding=6;style.FrameRounding=4;style.GrabRounding=4;
    style.WindowPadding=ImVec2(12,10);style.ItemSpacing=ImVec2(7,7);style.FramePadding=ImVec2(7,5);style.HoverDelayNormal=.6f;
    style.ScaleAllSizes(std::max(.5f,scale));
}
void DrawWeaponUI(WeaponUIState& s,const WeaponUICallback& action) {
    const auto& io=ImGui::GetIO();s.renderedPaneCount=0;s.inspectorWheelUsed=false;
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Kichray WeaponLab",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollWithMouse);
    if(ImGui::Button("Источники…"))s.setupOpen=true;
    ImGui::SameLine();
    if(ImGui::Button("Обновить список"))action(WeaponAction::ReloadCatalog,0);
    ImGui::SameLine();
    const bool pending=std::any_of(s.parameters.begin(),s.parameters.end(),[](const WeaponParameter& p){return p.pending;});
    ImGui::BeginDisabled(!(s.dataDirty||s.atlasDirty||pending));if(ImGui::Button("Сохранить  Ctrl+S"))action(WeaponAction::SaveData,0);ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!(s.dataDirty||pending));if(ImGui::Button("Отменить правки"))action(WeaponAction::DiscardData,0);ImGui::EndDisabled();
    ImGui::SameLine();ImGui::TextDisabled("%s",s.dataDirty||s.atlasDirty||pending?"Есть правки":"WeaponLab 0.2.0");
    const float width=ImGui::GetContentRegionAvail().x,spacing=ImGui::GetStyle().ItemSpacing.x;
    const float left=std::clamp(width*.20f,175.f,270.f*s.uiScale),right=std::clamp(width*.28f,280.f,390.f*s.uiScale);
    const float center=std::max(100.f,width-left-right-spacing*2),height=std::max(100.f,ImGui::GetContentRegionAvail().y-52*s.uiScale);
    ImVec2 p=ImGui::GetCursorScreenPos();s.leftRect={p.x,p.y,left,height};
    ImGui::BeginChild("CatalogPane",ImVec2(left,height),ImGuiChildFlags_Borders);++s.renderedPaneCount;catalog(s,action);ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("ViewportPane",ImVec2(center,height),0,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);++s.renderedPaneCount;viewport(s,action);ImGui::EndChild();ImGui::SameLine();
    p=ImGui::GetCursorScreenPos();s.inspectorRect={p.x,p.y,right,height};
    ImGui::BeginChild("InspectorPane",ImVec2(right,height),ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollWithMouse);++s.renderedPaneCount;
    if(ImGui::BeginTabBar("WeaponInspector",ImGuiTabBarFlags_FittingPolicyResizeDown)){
        const char* tabs[]={"Параметры","Тексты","Сцена"};const auto requested=s.tab;
        for(int i=0;i<3;++i)if(ImGui::BeginTabItem(tabs[i],nullptr,s.forceTab&&int(requested)==i?ImGuiTabItemFlags_SetSelected:0)){
            s.tab=WeaponTab(i);ImGui::BeginChild("InspectorScroll",ImVec2(0,0),0,ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::BeginDisabled(i!=2&&s.selected<0);if(i==0)parameterPane(s,action);if(i==1)textPane(s,action);if(i==2)scenePane(s,action);ImGui::EndDisabled();
            // Scroll only after controls have had a chance to own this frame's wheel.
            // ImGui's default scroll runs in NewFrame and races a first wheel tick over a slider.
            if(ImGui::IsWindowHovered()&&!s.inspectorWheelUsed&&io.MouseWheel!=0)
                ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()-io.MouseWheel*ImGui::GetTextLineHeight()*5,0.f,ImGui::GetScrollMaxY()));
            ImGui::EndChild();ImGui::EndTabItem();}
        ImGui::EndTabBar();}
    if(s.tab!=WeaponTab::Texts){s.descriptionEdit.focused=false;s.descriptionEdit.dragging=false;}
    s.forceTab=false;ImGui::EndChild();ImGui::TextWrapped("%s",s.status.c_str());
    if(!s.warning.empty())ImGui::TextColored(ImVec4(1,.7f,.35f,1),"%s",s.warning.c_str());
    ImGui::End();
    setup(s,action);iconEditor(s,action);
}
} // namespace lab
