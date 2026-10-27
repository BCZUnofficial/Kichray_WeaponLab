#pragma once
#include "Core.h"
#include <filesystem>
#include <string>
#include <vector>

namespace lab {
// X-Ray SoC inventory coordinates are 50px cells, not 1024/20 pixels.
struct IconRect { int x=0,y=0,w=1,h=1; };
struct IconGrid {
    int cellPixels=50;
    static constexpr int MaxCells=20;
    int columns(const Image& atlas) const;
    int rows(const Image& atlas) const;
    bool validate(const Image& atlas,const IconRect& rect,std::string& error) const;
};
Image CropIcon(const Image& atlas,const IconRect& rect,const IconGrid& grid,std::string& error);
// Fits visible alpha bounds into the requested canvas. Uses premultiplied-alpha
// filtering, then returns straight RGBA for PNG/DDS. Transparent padding is in pixels.
Image FitIconSnapshot(const Image& source,uint32_t width,uint32_t height,int padding,std::string& error);
// Exact-size replacement: pixels in the destination rectangle are overwritten,
// including transparent pixels; every pixel outside it stays unchanged.
bool ReplaceIcon(Image& atlas,const IconRect& rect,const IconGrid& grid,const Image& icon,std::string& error);
// Legacy DDS A8R8G8B8, single top level (no atlas cross-cell mip bleeding), lossless.
std::vector<uint8_t> EncodeIconAtlasDDS(const Image& image);
class IconAtlasDocument {
public:
    bool load(const std::filesystem::path& path,std::string& error);
    bool replace(const IconRect& rect,const Image& exactSizeIcon,std::string& error);
    // Saves only after verifying disk bytes still match those loaded. Keeps a
    // numbered .weaponlab.bak copy, and uses atomic file replacement.
    bool save(std::string& error);
    void discard();
    const Image& image() const {return image_;}
    const std::filesystem::path& path() const {return path_;}
    const std::filesystem::path& lastBackupPath() const {return backup_;}
    bool dirty() const {return dirty_;}
    bool loaded() const {return image_.valid();}
    IconGrid grid;
private:
    std::filesystem::path path_,backup_;
    std::vector<uint8_t> originalBytes_;
    Image originalImage_,image_;
    bool dirty_=false;
};
}
