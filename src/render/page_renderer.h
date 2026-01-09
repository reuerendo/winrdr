#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include "../epub/formatted_text.h"
#include "../epub/image_cache.h"

struct PageBreak {
    size_t element_start;
    size_t element_count;
};

class PageRenderer {
public:
    PageRenderer();
    ~PageRenderer();
    
    void setContent(const epub::FormattedContent& content);
    void setImageCache(epub::ImageCache* cache);
    
    void setViewport(int width, int height, int margin);
    void setFont(const std::wstring& font_name, int font_size);
    
    size_t getPageCount() const { return pages_.size(); }
    size_t getCurrentPage() const { return current_page_; }
    
    bool nextPage();
    bool prevPage();
    void goToPage(size_t page);
    
    void render(HDC hdc);

private:
    void calculatePages(HDC hdc);
    void renderElement(HDC hdc, const epub::TextElement& elem, RECT& rect, int& y_pos);
    
    HFONT createFont(int size, bool bold, bool italic, bool underline, bool strikethrough);
    int measureElementHeight(HDC hdc, const epub::TextElement& elem, int width);
    
    void drawText(HDC hdc, const std::wstring& text, RECT& rect, 
                  epub::TextAlign align, bool bold, bool italic);
    void drawImage(HDC hdc, const std::string& image_id, RECT& rect, int& y_pos);
    
    epub::FormattedContent content_;
    std::vector<PageBreak> pages_;
    size_t current_page_;
    
    int viewport_width_;
    int viewport_height_;
    int margin_;
    
    std::wstring font_name_;
    int font_size_;
    
    HFONT normal_font_;
    HFONT bold_font_;
    HFONT italic_font_;
    HFONT bold_italic_font_;
    
    epub::ImageCache* image_cache_;
};