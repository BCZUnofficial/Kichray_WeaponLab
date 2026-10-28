#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lab {
// All UI strings and values use UTF-8. On-disk files retain their original
// Windows-1251 / UTF-8 encoding, BOM, line endings and unrelated content.
struct WeaponValue {
    std::string key,value,sourceSection;
    std::filesystem::path sourceFile;
    bool inherited=false;
};
enum class WeaponItemKind { Weapon,Ammunition };
struct WeaponRecord {
    WeaponItemKind kind=WeaponItemKind::Weapon;
    std::string section,label,visual,nameId,shortNameId,descriptionId;
    std::string name,description;
    std::filesystem::path sourceFile;
    int iconX=0,iconY=0,iconWidth=1,iconHeight=1;
    std::vector<WeaponValue> values;
};
// A copy of effective values: strings remain as written in LTX (including
// quotes), and absent keys are absent rather than filled with laboratory defaults.
using WeaponConfigValues=std::map<std::string,std::string>;
struct WeaponAmmoRecord {
    std::string section,label;
    WeaponConfigValues values;
    bool available=false;
    std::string error;
};
// Supports finite scalars and comma-separated numeric vectors, and rejects
// partial parses, NaN/Inf and locale-dependent decimal commas.
bool ParseWeaponNumbers(const std::string& text,std::vector<double>& numbers);
std::string FormatWeaponNumbers(const std::vector<double>& numbers);
double QuantizeWeaponNumber(double value,double step);

class WeaponDatabase {
public:
    WeaponDatabase();
    ~WeaponDatabase();
    WeaponDatabase(WeaponDatabase&&) noexcept;
    WeaponDatabase& operator=(WeaponDatabase&&) noexcept;
    WeaponDatabase(const WeaponDatabase&)=delete;
    WeaponDatabase& operator=(const WeaponDatabase&)=delete;
    // Root files are all *.ltx below weaponsDirectory. Includes are resolved
    // relative to their file then config root; optional extraRoot imports
    // shared parent definitions (typically config/system.ltx).
    bool load(const std::filesystem::path& weaponsDirectory,
              const std::filesystem::path& stringTable,std::string& error,
              const std::filesystem::path& extraRoot={});
    // Disjoint catalogs, sorted by localized label. items() contains their
    // union exactly once and is suitable for shared atlas overlap checks.
    // Existing rows retain their kind during edits; load/reload reclassifies
    // them. Edits invalidate references/pointers into all catalog views.
    const std::vector<WeaponRecord>& weapons() const;
    const std::vector<WeaponRecord>& ammunition() const;
    const std::vector<WeaponRecord>& items() const;
    const WeaponRecord* find(const std::string& section) const;
    // Includes non-catalog sections such as templates and [bullet_manager].
    // Missing/duplicate sections and unresolved ancestry return an explicit error.
    bool sectionValues(const std::string& section,WeaponConfigValues& values,
                       std::string& error) const;
    bool sectionParameters(const std::string& section,std::vector<WeaponValue>& values,std::string& error) const;
    // Returns ammo_class in configuration order, including unavailable IDs so
    // the UI can show the problem instead of silently choosing a different round.
    std::vector<WeaponAmmoRecord> ammunitionForWeapon(const std::string& section) const;
    const std::vector<std::string>& warnings() const;
    const std::filesystem::path& weaponsDirectory() const;
    const std::filesystem::path& stringTablePath() const;
    std::string text(const std::string& id) const;
    bool hasText(const std::string& id) const;
    bool setValue(const std::string& section,const std::string& key,
                  const std::string& value,std::string& error);
    // Creates a missing string id, or replaces only that string's text body.
    bool setText(const std::string& id,const std::string& value,std::string& error);
    bool dirty() const;
    std::vector<std::filesystem::path> changedFiles() const;
    // Refuses externally modified files. Stages all bytes, creates a unique
    // backup per original, replaces atomically per file, rolls back on error.
    bool save(std::string& error);
    bool reload(std::string& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
