#pragma once

#include <string>
#include <vector>
#include <windows.h>

struct PageBreak {
    size_t text_offset;
    size_t text_length;
};

class PageRenderer {
public:
    PageRenderer();
    ~PageRenderer();
    
    void setText(const std::string& text);
    void setViewport(int width, int height, int margin);
    void setFont(const std::wstring& font_name, int font_size);
    
    size_t getPageCount() const { return pages_.size(); }
    size_t getCurrentPage() const { return current_page_; }
    
    bool nextPage();
    bool prevPage();
    void goToPage(size_t page);
    
    void render(HDC hdc);

private:
    void calculatePages();
    int measureTextHeight(HDC hdc, const std::wstring& text, int width);
    
    std::wstring text_;
    std::vector<PageBreak> pages_;
    size_t current_page_;
    
    int viewport_width_;
    int viewport_height_;
    int margin_;
    
    std::wstring font_name_;
    int font_size_;
    HFONT font_handle_;
};
