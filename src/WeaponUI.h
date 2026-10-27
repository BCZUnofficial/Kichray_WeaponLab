#pragma once
#include "imgui.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace lab {
enum class WeaponMode { Fly, Orbit, Pan, Light, Model };
enum class WeaponTab { Parameters, Texts, Scene };
enum class WeaponAction {
    SelectWeapon, SaveData, DiscardData, ReloadCatalog,
    DefaultPaths, BrowsePath, ApplyPaths,
    ChangeParameter, ChangeTexts, SetMode, Frame, ResetView, AxisView,
    LightSettings, BackgroundSettings, Wireframe,
    ApplyIconRect, CaptureIcon, PreviewSnapshot, StageIcon, SaveAtlas, DiscardAtlas, ExportSnapshot
};
// Path indices: gamedata, config/weapons, string table XML, icon atlas DDS,
// meshes root, textures root. Paths are UTF-8; shell converts to native paths.
struct WeaponPathState {
    std::array<std::string,6> values;
};
struct WeaponRow { std::string section,label,source; bool missingModel=false; };
struct WeaponParameter {
    std::string key,value,source;
    bool inherited=false,pending=false;
    // Empty for textual properties. All components of a numeric vector use
    // this row's keyboard-editable step and range; exact text remains editable.
    std::vector<double> numbers;
    double step=.01,minimum=0,maximum=1;
    bool integer=false,rangeOpen=false,numericControlsInitialized=false;
};
struct WeaponRect {float x=0,y=0,w=0,h=0;};
struct WeaponUIState {
    WeaponPathState paths;
    bool setupOpen=true,iconEditorOpen=false;
    std::string search,parameterSearch;
    std::vector<WeaponRow> weapons;
    int selected=-1;
    std::vector<WeaponParameter> parameters;
    std::string name,description,nameId,descriptionId,modelName;
    bool dataDirty=false,atlasDirty=false,hasModel=false;
    bool canEditName=true,canEditDescription=true;
    std::string status="Укажите папку gamedata и источники.",warning;
    WeaponTab tab=WeaponTab::Parameters;
    bool forceTab=false;
    WeaponMode mode=WeaponMode::Fly;
    bool directedLight=true,wireframe=false;
    float lightIntensity=1.f,lightAzimuth=52.f,lightElevation=37.f;
    float lightColor[3]={1,.93f,.78f},ambient[3]={.21f,.24f,.29f};
    float environment[3]={0,0,0},hemisphere=1.f,exposure=1.f,glossFactor=1.f;
    float background[3]={.095f,.105f,.12f};
    ImTextureID viewportTexture=0,atlasTexture=0,snapshotTexture=0;
    int atlasWidth=0,atlasHeight=0,cellPixels=50;
    int iconX=0,iconY=0,iconW=1,iconH=1;
    // Current stored/configured rectangle is separate from the editing selection.
    int currentIconX=0,currentIconY=0,currentIconW=1,currentIconH=1;
    int snapshotWidth=0,snapshotHeight=0,iconPadding=2;
    bool hasSnapshot=false;
    float atlasZoom=1.f;
    WeaponRect viewportRect,toolbarRect,leftRect,inspectorRect,atlasRect;
    bool viewportHovered=false,viewportClicked=false,viewportActive=false;
    int renderedPaneCount=0;
    float uiScale=1.f;
    int atlasDragX=-1,atlasDragY=-1;
};
using WeaponUICallback=std::function<void(WeaponAction,int)>;
WeaponParameter MakeWeaponParameter(std::string key,std::string value,std::string source={},bool inherited=false);
// Reparse a raw edit while preserving explicitly chosen numeric controls.
void RefreshWeaponParameterNumbers(WeaponParameter& parameter);
// Shared by the UI and its regression tests; finite, bounded and exact in steps.
double WeaponSteppedValue(double minimum,double maximum,double step,int tick);
void ConfigureWeaponUIStyle(float scale);
void DrawWeaponUI(WeaponUIState& state,const WeaponUICallback& action);
}
