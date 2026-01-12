#pragma once

#include <windows.h>
#include <litehtml.h>
#include <string>
#include <vector>
#include <memory>
#include "litehtml_container.h"
#include "../epub/image_cache.h"

class PageRenderer {
public:
    PageRenderer();
    ~PageRenderer();
    
    void setContent(const std::string& html, const std::string& css);
    void setImageCache(epub::ImageCache* cache);
    
    void setViewport(int width, int height, int margin);
    
    size_t getPageCount() const { return pages_.size(); }
    size_t getCurrentPage() const { return current_page_; }
    
    bool nextPage();
    bool prevPage();
    void goToPage(size_t page);
    
    void render(HDC hdc);

private:
    struct PageInfo {
        int scroll_offset;
    };
    
    void calculatePages(HDC hdc);
    void createMemoryDC();
    
    std::shared_ptr<litehtml::document> document_;
    LitehtmlContainer* container_;
    
    epub::ImageCache* image_cache_;
    
    std::vector<PageInfo> pages_;
    size_t current_page_;
    
    int viewport_width_;
    int viewport_height_;
    int margin_;
    
    int total_height_;
    
    std::string master_css_;
    
    HDC memory_hdc_;
    HBITMAP memory_bitmap_;
};