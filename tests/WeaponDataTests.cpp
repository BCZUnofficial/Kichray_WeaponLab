#include "WeaponData.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
namespace fs=std::filesystem;
static void write(const fs::path& path,const std::string& value){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<value;assert(out.good());}
static std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});}
static const lab::WeaponValue* value(const lab::WeaponRecord& r,const std::string& key){for(const auto& v:r.values)if(v.key==key)return &v;return nullptr;}
int main(){
 auto root=fs::temp_directory_path()/fs::path("weaponlab-data-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto weapons=root/"gamedata/config/weapons";
 const auto xml=root/"gamedata/config/text/rus/string_table_enc_weapons.xml";
 const auto base=root/"gamedata/config/shared.ltx";
 const std::string baseText="[weapon_base]\r\ncost = 100 ; inherited comment\r\nhit_power = 0.5, 0.4, 0.3, 0.2\r\nvisual = weapons\\ak74\\wpn_ak74.ogf\r\n";
 const std::string firstText="; original header\r\n#include \"..\\shared.ltx\"\r\n[wpn_ak74]:weapon_base\r\n inv_name = st_wpn_ak74\r\ninv_name_short = st_wpn_ak74\r\ndescription = st_wpn_ak74_descr\r\ninv_grid_x = 3\r\ninv_grid_y = 4\r\ninv_grid_width = 5\r\ninv_grid_height = 2\r\nrpm = 600  ; preserve spaces\r\nsound = \"sounds;slash//test\"\r\n\r\n[wpn_ak74_hud]\r\nvisual = weapons\\ak74\\wpn_ak74_hud.ogf\r\n\r\n[ammo_545]\r\ninv_name = ammo\r\nvisual = ammo.ogf\r\n";
 write(base,baseText);write(weapons/"w_ak74.ltx",firstText);
 const std::string xmlText="<?xml version=\"1.0\" encoding=\"windows-1251\"?>\r\n<string_table>\r\n<!-- <string id=\"fake\"><text>fake</text></string> -->\r\n<string id=\"st_wpn_ak74\"><text>\xc0\xca-74 &amp; test</text></string>\r\n<string id='st_wpn_ak74_descr'><text><![CDATA[Old <description>]]></text></string>\r\n<string id=\"unused\"><text>Leave &#x410; &quot;alone&quot;</text></string>\r\n</string_table>\r\n";
 write(xml,xmlText);std::string error;lab::WeaponDatabase db;assert(db.load(weapons,xml,error));assert(error.empty());assert(!db.dirty());assert(db.weapons().size()==1);auto r=*db.find("wpn_ak74");assert(r.label==u8"АК-74 & test");assert(r.visual=="weapons\\ak74\\wpn_ak74.ogf");assert(r.description=="Old <description>");assert(r.iconX==3&&r.iconY==4&&r.iconWidth==5&&r.iconHeight==2);assert(value(r,"cost")->inherited);assert(value(r,"cost")->sourceSection=="weapon_base");assert(value(r,"sound")->value=="\"sounds;slash//test\"");
 assert(db.setValue(r.section,"rpm","650",error));assert(db.setValue(r.section,"cost","125",error));assert(db.setText(r.nameId,u8"Новое имя & <АК>",error));assert(db.setText(r.descriptionId,u8"Описание\nВторая строка",error));assert(db.dirty());assert(db.changedFiles().size()==2);assert(db.save(error));assert(!db.dirty());assert(read(base)==baseText);auto edited=read(weapons/"w_ak74.ltx");assert(edited.find("rpm = 650  ; preserve spaces\r\n")!=std::string::npos);assert(edited.find("cost = 125\r\n[wpn_ak74_hud]")!=std::string::npos);assert(edited.find("; original header\r\n")!=std::string::npos);auto editedXML=read(xml);assert(editedXML.find("Leave &#x410; &quot;alone&quot;")!=std::string::npos);assert(editedXML.find("&amp; &lt;")!=std::string::npos);assert(editedXML.find("windows-1251")!=std::string::npos);assert(editedXML.find(u8"Новое")==std::string::npos);assert(db.reload(error));assert(db.find(r.section)->name==u8"Новое имя & <АК>");assert(db.find(r.section)->description==u8"Описание\nВторая строка");
 size_t backupCount=0;for(auto& p:fs::recursive_directory_iterator(root))if(p.path().filename().string().find(".weaponlab.bak.")!=std::string::npos)++backupCount;assert(backupCount==2);
 // CP1251 cannot silently lose characters.
 assert(!db.setText(r.nameId,u8"Имя 😀",error));assert(!db.dirty());assert(db.find(r.section)->name==u8"Новое имя & <АК>");
 // External modifications prevent every write, including XML part of a save.
 assert(db.setValue(r.section,"rpm","700",error));assert(db.setText(r.nameId,u8"Иное имя",error));write(weapons/"w_ak74.ltx",edited+"; external\r\n");assert(!db.save(error));assert(read(xml)==editedXML);assert(read(weapons/"w_ak74.ltx")==edited+"; external\r\n");assert(db.reload(error));assert(!db.dirty());
 // Repeated changes and reload must not duplicate local inherited overrides.
 assert(db.setValue(r.section,"cost","200",error));assert(db.setValue(r.section,"cost","250",error));assert(db.save(error));auto current=read(weapons/"w_ak74.ltx");auto start=current.find("cost = 250");assert(start!=std::string::npos&&current.find("cost = ",start+1)==std::string::npos);
 // Invalid injection cannot add sections or inline comments.
 assert(!db.setValue(r.section,"rpm","1\n[evil]",error));assert(!db.setValue(r.section,"rpm","1; comment",error));assert(!db.dirty());
 // Missing localization IDs are explicit and can be created; XML entities roundtrip.
 assert(db.text("new_id")=="new_id");assert(!db.hasText("new_id"));assert(db.setText("new_id",u8"Новая <строка> & \"quote\"",error));assert(db.hasText("new_id"));assert(db.save(error));assert(db.reload(error));assert(db.text("new_id")==u8"Новая <строка> & \"quote\"");
 // UTF-8 BOM and LF remain UTF-8 BOM and LF.
 const auto utfxml=root/"utf8.xml";write(utfxml,"\xef\xbb\xbf<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<string_table>\n<string id=\"st_wpn_ak74\"><text>АК UTF8</text></string>\n</string_table>\n");lab::WeaponDatabase utf;assert(utf.load(weapons,utfxml,error));assert(utf.setText("st_wpn_ak74",u8"UTF8 😀",error));assert(utf.save(error));assert(read(utfxml).compare(0,3,"\xef\xbb\xbf")==0);assert(read(utfxml).find(u8"😀")!=std::string::npos);assert(read(utfxml).find('\r')==std::string::npos);
 // Missing shared parents produce diagnostics; included duplicates load only once.
 write(weapons/"extra.ltx","#include \"w_ak74.ltx\"\n[wpn_copy]:wpn_ak74\nvisual=copy.ogf\n[wpn_missing]:missing_base\ninv_name=st_wpn_ak74\nvisual=missing.ogf\n");assert(utf.reload(error));assert(utf.weapons().size()==3);assert(!utf.warnings().empty());
 // Duplicate definitions are displayed but protected from ambiguous writes.
 write(weapons/"duplicate.ltx","[wpn_ak74]\ninv_name = st_wpn_ak74\nvisual = duplicate.ogf\n");assert(utf.reload(error));assert(!utf.setValue("wpn_ak74","cost","1",error));assert(!utf.dirty());
 // A model/name edit must never remove the current weapon from the editor.
 assert(utf.setValue("wpn_copy","visual","",error));assert(utf.find("wpn_copy"));assert(utf.setValue("wpn_copy","inv_name","",error));assert(utf.find("wpn_copy"));assert(utf.reload(error));
 // Grid coordinates are independent of the visible20x20 window and use nonnegative integers.
 for(const auto& bad:{"-1","2147483648","0.5","NaN"})assert(!utf.setValue("wpn_copy","inv_grid_x",bad,error));
 for(const auto& bad:{"-1","0","2147483648","0.5"})assert(!utf.setValue("wpn_copy","inv_grid_width",bad,error));
 assert(!utf.dirty());
 assert(utf.setValue("wpn_copy","inv_grid_x","39",error));assert(utf.setValue("wpn_copy","inv_grid_width","21",error));
 assert(utf.find("wpn_copy")->iconX==39&&utf.find("wpn_copy")->iconWidth==21);assert(utf.reload(error));
 // Legacy BOMless ASCII LTX is ANSI/CP1251, including the first Cyrillic edit.
 assert(utf.setValue("wpn_copy","description",u8"Текст",error));assert(utf.save(error));auto rawLTX=read(weapons/"extra.ltx");assert(rawLTX.find(u8"Текст")==std::string::npos);assert(rawLTX.find("\xd2\xe5\xea\xf1\xf2")!=std::string::npos);assert(utf.reload(error));assert(value(*utf.find("wpn_copy"),"description")->value==u8"Текст");
 // Invalid XML does not replace an already loaded database.
 auto oldSize=utf.weapons().size();write(root/"bad.xml","<string_table><string id=\"a\"><text>x</text></string><string id=\"a\"><text>x</text></string></string_table>");assert(!utf.load(weapons,root/"bad.xml",error));assert(utf.weapons().size()==oldSize);
 // Self-closing empty localization entries can be edited without touching peers.
 const auto emptyXML=root/"empty.xml";write(emptyXML,"<string_table><string id=\"st_wpn_ak74\"><!-- <text>ignore</text> --><text /></string></string_table>");lab::WeaponDatabase empty;assert(empty.load(weapons,emptyXML,error));assert(empty.text("st_wpn_ak74").empty());assert(empty.setText("st_wpn_ak74","Filled",error));assert(empty.save(error));assert(read(emptyXML).find("<!-- <text>ignore</text> --><text>Filled</text>")!=std::string::npos);
 // Ammunition and bullet_manager are effective sections, not catalog weapons.
 const auto labDir=root/"lab/config/weapons";const auto labXML=root/"lab/config/text/rus/weapons.xml";
 write(labXML,"<string_table><string id=\"ammo-label\"><text>FMJ name</text></string></string_table>");
 write(root/"lab/config/system.ltx","#include \"weapons\\weapons.ltx\"\n[bullet_manager]\ngravity_const=9.8\nair_resistance_k=0.3\n");
 write(labDir/"weapons.ltx","[ammo_base]\nclass=AMMO\nk_disp=1.2\ninv_name=ammo-label\n[ammo_a]:ammo_base\nk_hit=1.1\n[ammo_b]:ammo_base\nk_disp=0.7\n[bad_ammo]:missing_ammo_parent\nk_disp=1\n[loop_a]:loop_b\n[loop_b]:loop_a\n");
 write(labDir/"w_test.ltx","[wpn_test]\ninv_name=gun\nvisual=gun.ogf\nammo_class=ammo_b, ammo_a, missing_round, bad_ammo\n");
 lab::WeaponDatabase ammoDB;assert(ammoDB.load(labDir,labXML,error,root/"lab/config/system.ltx"));
 lab::WeaponConfigValues effective;assert(ammoDB.sectionValues("ammo_a",effective,error));assert(effective.at("k_disp")=="1.2"&&effective.at("k_hit")=="1.1");assert(!effective.count("k_speed"));
 assert(ammoDB.sectionValues("bullet_manager",effective,error));assert(effective.at("gravity_const")=="9.8");
 assert(!ammoDB.sectionValues("missing_round",effective,error)&&effective.empty()&&!error.empty());
 assert(!ammoDB.sectionValues("bad_ammo",effective,error));assert(!ammoDB.sectionValues("loop_a",effective,error));
 auto ammoEntries=ammoDB.ammunitionForWeapon("wpn_test");assert(ammoEntries.size()==4);
 assert(ammoEntries[0].section=="ammo_b"&&ammoEntries[0].available&&ammoEntries[0].values.at("k_disp")=="0.7");
 assert(ammoEntries[1].section=="ammo_a"&&ammoEntries[1].label=="FMJ name");
 assert(!ammoEntries[2].available&&!ammoEntries[2].error.empty());assert(!ammoEntries[3].available);
 assert(ammoDB.weapons().size()==1); // Ammo never masquerades as a firearm row.
 assert(ammoDB.setValue("ammo_a","k_disp","2",error));assert(ammoDB.sectionValues("ammo_a",effective,error)&&effective.at("k_disp")=="2");
 assert(ammoDB.ammunitionForWeapon("wpn_test")[1].values.at("k_disp")=="2");
 // Numeric parsing is strict, locale independent, finite and vector aware.
 std::vector<double> numbers;assert(lab::ParseWeaponNumbers(" 1e-3, -2.5, +7, 0 ",numbers)&&numbers.size()==4);assert(numbers[0]==.001&&numbers[1]==-2.5);for(const auto& invalid:{"","1,","1 ; note","nan","inf","1e999","1abc","1 2"})assert(!lab::ParseWeaponNumbers(invalid,numbers));assert(std::abs(lab::QuantizeWeaponNumber(.26,.1)-.3)<1e-12);assert(std::abs(lab::QuantizeWeaponNumber(-.26,.1)+.3)<1e-12);assert(lab::ParseWeaponNumbers(lab::FormatWeaponNumbers({.1,1.,1e-9}),numbers)&&numbers.size()==3);
 fs::remove_all(root);std::cout<<"WeaponDataTests: PASS (LTX inheritance/includes, exact writes, CP1251/UTF8 XML, backups/conflicts, numeric stepping)\n";
}
