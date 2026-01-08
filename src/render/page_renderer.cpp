#include "page_renderer.h"
#include "../utils/logger.h"
#include <algorithm>

#undef min
#undef max

PageRenderer::PageRenderer() 
    : current_page_(0)
    , viewport_width_(0)
    , viewport_height_(0)
    , margin_(20)
    , font_name_(L"Arial")
    , font_size_(20)
    , font_handle_(nullptr)
{
}

PageRenderer::~PageRenderer() {
    if (font_handle_) {
        DeleteObject(font_handle_);
    }
}

void PageRenderer::setText(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    text_.resize(size);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &text_[0], size);
    
    current_page_ = 0;
    pages_.clear();
    
    LOG_DEBUG("Text set, length:", text_.length(), "chars");
}

void PageRenderer::setViewport(int width, int height, int margin) {
    viewport_width_ = width;
    viewport_height_ = height;
    margin_ = margin;
    
    LOG_DEBUG("Viewport set:", width, "x", height, "margin:", margin);
}

void PageRenderer::setFont(const std::wstring& font_name, int font_size) {
    font_name_ = font_name;
    font_size_ = font_size;
    
    if (font_handle_) {
        DeleteObject(font_handle_);
    }
    
    font_handle_ = CreateFontW(
        font_size_, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font_name_.c_str()
    );
    
    LOG_DEBUG("Font set:", font_size, "pt");
}

void PageRenderer::calculatePages() {
    if (text_.empty() || viewport_width_ <= 0 || viewport_height_ <= 0) {
        LOG_WARNING("Cannot calculate pages: empty text or invalid viewport");
        return;
    }
    
    pages_.clear();
    
    HDC hdc = GetDC(NULL);
    HFONT old_font = (HFONT)SelectObject(hdc, font_handle_);
    
    int content_width = viewport_width_ - 2 * margin_;
    int content_height = viewport_height_ - 2 * margin_;
    
    size_t text_pos = 0;
    
    while (text_pos < text_.length()) {
        RECT rect = {0, 0, content_width, content_height};
        
        // Find how much text fits on one page
        size_t chunk_size = std::min((size_t)1000, text_.length() - text_pos);
        std::wstring chunk = text_.substr(text_pos, chunk_size);
        
        // Measure text height
        int height = DrawTextW(hdc, chunk.c_str(), -1, &rect, 
                              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
        
        // Adjust chunk size to fit page
        while (height > content_height && chunk_size > 10) {
            chunk_size = chunk_size * content_height / height;
            chunk = text_.substr(text_pos, chunk_size);
            rect = {0, 0, content_width, content_height};
            height = DrawTextW(hdc, chunk.c_str(), -1, &rect, 
                             DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
        }
        
        // Try to break at word boundary
        if (text_pos + chunk_size < text_.length()) {
            size_t last_space = chunk.find_last_of(L" \n\r\t");
            if (last_space != std::wstring::npos && last_space > chunk_size / 2) {
                chunk_size = last_space + 1;
            }
        }
        
        PageBreak page;
        page.text_offset = text_pos;
        page.text_length = chunk_size;
        pages_.push_back(page);
        
        text_pos += chunk_size;
    }
    
    SelectObject(hdc, old_font);
    ReleaseDC(NULL, hdc);
    
    LOG_INFO("Pages calculated:", pages_.size());
}

bool PageRenderer::nextPage() {
    if (pages_.empty()) {
        calculatePages();
    }
    
    if (current_page_ + 1 < pages_.size()) {
        current_page_++;
        LOG_DEBUG("Next page:", current_page_ + 1, "/", pages_.size());
        return true;
    }
    return false;
}

bool PageRenderer::prevPage() {
    if (current_page_ > 0) {
        current_page_--;
        LOG_DEBUG("Prev page:", current_page_ + 1, "/", pages_.size());
        return true;
    }
    return false;
}

void PageRenderer::goToPage(size_t page) {
    if (pages_.empty()) {
        calculatePages();
    }
    
    if (page < pages_.size()) {
        current_page_ = page;
        LOG_DEBUG("Go to page:", current_page_ + 1, "/", pages_.size());
    }
}

void PageRenderer::render(HDC hdc) {
    if (pages_.empty()) {
        calculatePages();
    }
    
    if (pages_.empty() || current_page_ >= pages_.size()) {
        LOG_WARNING("Nothing to render");
        return;
    }
    
    const PageBreak& page = pages_[current_page_];
    std::wstring page_text = text_.substr(page.text_offset, page.text_length);
    
    HFONT old_font = (HFONT)SelectObject(hdc, font_handle_);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    
    RECT rect;
    rect.left = margin_;
    rect.top = margin_;
    rect.right = viewport_width_ - margin_;
    rect.bottom = viewport_height_ - margin_;
    
    DrawTextW(hdc, page_text.c_str(), -1, &rect, 
             DT_LEFT | DT_TOP | DT_WORDBREAK);
    
    SelectObject(hdc, old_font);
}