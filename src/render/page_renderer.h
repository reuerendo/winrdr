#pragma once

#include "../epub/layout_engine.h"
#include "../epub/image_cache.h"
#include <windows.h>
#include <vector>
#include <string>

// Forward declaration
namespace Gdiplus {
    class Graphics;
}

class PageRenderer {
public:
    PageRenderer();
    ~PageRenderer();
    
    void setViewport(int width, int height, int margin);
    void setFont(const std::wstring& family, int size);
    void setContent(const std::vector<epub::RenderLine>& lines);
    void setImageCache(epub::ImageCache* cache);
    
    void render(HDC hdc);
    
    bool nextPage();
    bool prevPage();
    void goToPage(size_t page);
    
    size_t getCurrentPage() const { return current_page_; }
    size_t getPageCount() const { return pages_.size(); }

private:
    void recalculatePages();
    void renderLine(Gdiplus::Graphics& graphics, const epub::RenderLine& line, int y_offset);
    void renderTextLine(Gdiplus::Graphics& graphics, const epub::RenderLine& line, int y);
    void renderImage(Gdiplus::Graphics& graphics, const epub::RenderLine& line, int y);
    
    std::vector<epub::RenderLine> lines_;
    std::vector<epub::PageInfo> pages_;
    
    int viewport_width_;
    int viewport_height_;
    int margin_;
    size_t current_page_;
    
    std::wstring font_family_;
    int font_size_;
    
    epub::ImageCache* image_cache_;
    
    ULONG_PTR gdiplusToken_;
};