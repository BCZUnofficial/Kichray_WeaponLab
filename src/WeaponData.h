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
struct WeaponRecord {
    std::string section,label,visual,nameId,shortNameId,descriptionId;
    std::string name,description;
    std::filesystem::path sourceFile;
    int iconX=0,iconY=0,iconWidth=1,iconHeight=1;
    std::vector<WeaponValue> values;
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
    const std::vector<WeaponRecord>& weapons() const;
    const WeaponRecord* find(const std::string& section) const;
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
