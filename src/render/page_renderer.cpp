#include "page_renderer.h"
#include "../utils/logger.h"
#include <algorithm>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#undef min
#undef max

PageRenderer::PageRenderer() 
    : current_page_(0)
    , viewport_width_(0)
    , viewport_height_(0)
    , margin_(20)
    , font_name_(L"Arial")
    , font_size_(20)
    , normal_font_(nullptr)
    , bold_font_(nullptr)
    , italic_font_(nullptr)
    , bold_italic_font_(nullptr)
    , mono_font_(nullptr)
    , mono_bold_font_(nullptr)
    , image_cache_(nullptr)
{
}

PageRenderer::~PageRenderer() {
    if (normal_font_) DeleteObject(normal_font_);
    if (bold_font_) DeleteObject(bold_font_);
    if (italic_font_) DeleteObject(italic_font_);
    if (bold_italic_font_) DeleteObject(bold_italic_font_);
    if (mono_font_) DeleteObject(mono_font_);
    if (mono_bold_font_) DeleteObject(mono_bold_font_);
}

void PageRenderer::setContent(const epub::FormattedContent& content) {
    content_ = content;
    current_page_ = 0;
    pages_.clear();
    
    LOG_DEBUG("Content set, elements:", content_.size());
}

void PageRenderer::setImageCache(epub::ImageCache* cache) {
    image_cache_ = cache;
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
    
    if (normal_font_) DeleteObject(normal_font_);
    if (bold_font_) DeleteObject(bold_font_);
    if (italic_font_) DeleteObject(italic_font_);
    if (bold_italic_font_) DeleteObject(bold_italic_font_);
    if (mono_font_) DeleteObject(mono_font_);
    if (mono_bold_font_) DeleteObject(mono_bold_font_);
    
    normal_font_ = createFont(font_size_, false, false, false, false);
    bold_font_ = createFont(font_size_, true, false, false, false);
    italic_font_ = createFont(font_size_, false, true, false, false);
    bold_italic_font_ = createFont(font_size_, true, true, false, false);
    
    mono_font_ = CreateFontW(
        font_size_, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
    );
    
    mono_bold_font_ = CreateFontW(
        font_size_, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
    );
    
    LOG_DEBUG("Fonts set:", font_size, "pt");
}

HFONT PageRenderer::createFont(int size, bool bold, bool italic, 
                               bool underline, bool strikethrough) {
    return CreateFontW(
        size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic ? TRUE : FALSE,
        underline ? TRUE : FALSE,
        strikethrough ? TRUE : FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        font_name_.c_str()
    );
}

HFONT PageRenderer::selectFontForStyle(epub::TextStyle style) {
    const bool is_bold = epub::hasStyle(style, epub::TextStyle::Bold);
    const bool is_italic = epub::hasStyle(style, epub::TextStyle::Italic);
    const bool is_mono = epub::hasStyle(style, epub::TextStyle::Monospace);
    
    if (is_mono) {
        return is_bold ? mono_bold_font_ : mono_font_;
    }
    
    if (is_bold && is_italic) return bold_italic_font_;
    if (is_bold) return bold_font_;
    if (is_italic) return italic_font_;
    return normal_font_;
}

void PageRenderer::calculatePages(HDC hdc) {
    if (content_.empty() || viewport_width_ <= 0 || viewport_height_ <= 0) {
        LOG_WARNING("Cannot calculate pages: empty content or invalid viewport");
        return;
    }
    
    pages_.clear();
    
    int content_height = viewport_height_ - 2 * margin_;
    int content_width = viewport_width_ - 2 * margin_;
    
    size_t elem_start = 0;
    int current_height = 0;
    
    for (size_t i = 0; i < content_.size(); i++) {
        int elem_height = measureElementHeight(hdc, content_[i], content_width);
        
        if (current_height + elem_height > content_height && i > elem_start) {
            PageBreak page;
            page.element_start = elem_start;
            page.element_count = i - elem_start;
            pages_.push_back(page);
            
            elem_start = i;
            current_height = 0;
        }
        
        current_height += elem_height;
    }
    
    if (elem_start < content_.size()) {
        PageBreak page;
        page.element_start = elem_start;
        page.element_count = content_.size() - elem_start;
        pages_.push_back(page);
    }
    
    LOG_INFO("Pages calculated:", pages_.size());
}

int PageRenderer::measureElementHeight(HDC hdc, const epub::TextElement& elem, int width) {
    const int LINE_SPACING = 5;
    const int PARAGRAPH_SPACING = 15;
    const int HEADING_SPACING = 20;
    const int LIST_INDENT = 30;
    const int HR_HEIGHT = 10;
    
    switch (elem.type) {
        case epub::ElementType::LineBreak:
            return font_size_;
            
        case epub::ElementType::HorizontalRule:
            return HR_HEIGHT + PARAGRAPH_SPACING;
            
        case epub::ElementType::Image: {
            if (image_cache_) {
                const epub::ImageData* img = image_cache_->getImage(elem.image_id);
                if (img) {
                    float scale = static_cast<float>(width) / img->width;
                    if (scale > 1.0f) scale = 1.0f;
                    return static_cast<int>(img->height * scale) + PARAGRAPH_SPACING;
                }
            }
            return PARAGRAPH_SPACING;
        }
        
        case epub::ElementType::Heading1:
        case epub::ElementType::Heading2:
        case epub::ElementType::Heading3:
        case epub::ElementType::Heading4:
        case epub::ElementType::Heading5:
        case epub::ElementType::Heading6: {
            int heading_size = font_size_ + 8;
            HFONT heading_font = createFont(heading_size, true, false, false, false);
            HFONT old_font = (HFONT)SelectObject(hdc, heading_font);
            
            RECT rect = {0, 0, width, 0};
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            
            SelectObject(hdc, old_font);
            DeleteObject(heading_font);
            
            return rect.bottom + HEADING_SPACING;
        }
        
        case epub::ElementType::CodeBlock: {
            HFONT old_font = (HFONT)SelectObject(hdc, mono_font_);
            RECT rect = {0, 0, width, 0};
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_NOPREFIX);
            SelectObject(hdc, old_font);
            return rect.bottom + PARAGRAPH_SPACING;
        }
        
        case epub::ElementType::ListItem: {
            int list_width = width - (LIST_INDENT * elem.list_level);
            RECT rect = {0, 0, list_width, 0};
            
            HFONT font = selectFontForStyle(elem.style);
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(hdc, old_font);
            
            return rect.bottom + LINE_SPACING;
        }
        
        case epub::ElementType::Quote:
        case epub::ElementType::Paragraph:
        case epub::ElementType::Text:
        case epub::ElementType::Link: {
            RECT rect = {0, 0, width, 0};
            
            HFONT font = selectFontForStyle(elem.style);
            
            if (epub::hasStyle(elem.style, epub::TextStyle::Small)) {
                int small_size = static_cast<int>(font_size_ * 0.85);
                const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
                const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
                const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
                
                if (is_mono) {
                    HFONT small_font = CreateFontW(
                        small_size, 0, 0, 0, is_bold ? FW_BOLD : FW_NORMAL, 
                        is_italic ? TRUE : FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
                    );
                    HFONT old_font = (HFONT)SelectObject(hdc, small_font);
                    DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                             DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                    SelectObject(hdc, old_font);
                    DeleteObject(small_font);
                } else {
                    HFONT small_font = createFont(small_size, is_bold, is_italic, false, false);
                    HFONT old_font = (HFONT)SelectObject(hdc, small_font);
                    DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                             DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                    SelectObject(hdc, old_font);
                    DeleteObject(small_font);
                }
            } else {
                HFONT old_font = (HFONT)SelectObject(hdc, font);
                DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                         DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                SelectObject(hdc, old_font);
            }
            
            int spacing = (elem.type == epub::ElementType::Paragraph) ? 
                         PARAGRAPH_SPACING : LINE_SPACING;
            return rect.bottom + spacing;
        }
    }
    
    return LINE_SPACING;
}

bool PageRenderer::nextPage() {
    if (pages_.empty()) {
        HDC hdc = GetDC(NULL);
        calculatePages(hdc);
        ReleaseDC(NULL, hdc);
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
        HDC hdc = GetDC(NULL);
        calculatePages(hdc);
        ReleaseDC(NULL, hdc);
    }
    
    if (page < pages_.size()) {
        current_page_ = page;
        LOG_DEBUG("Go to page:", current_page_ + 1, "/", pages_.size());
    }
}

void PageRenderer::render(HDC hdc) {
    if (pages_.empty()) {
        calculatePages(hdc);
    }
    
    if (pages_.empty() || current_page_ >= pages_.size()) {
        LOG_WARNING("Nothing to render");
        return;
    }
    
    const PageBreak& page = pages_[current_page_];
    
    SetBkMode(hdc, TRANSPARENT);
    
    int y_pos = margin_;
    
    for (size_t i = 0; i < page.element_count; i++) {
        size_t elem_idx = page.element_start + i;
        if (elem_idx >= content_.size()) break;
        
        RECT rect;
        rect.left = margin_;
        rect.top = y_pos;
        rect.right = viewport_width_ - margin_;
        rect.bottom = viewport_height_ - margin_;
        
        renderElement(hdc, content_[elem_idx], rect, y_pos);
    }
}

void PageRenderer::renderElement(HDC hdc, const epub::TextElement& elem, 
                                RECT& rect, int& y_pos) {
    const int LINE_SPACING = 5;
    const int PARAGRAPH_SPACING = 15;
    const int HEADING_SPACING = 20;
    const int LIST_INDENT = 30;
    const int QUOTE_INDENT = 40;
    const int HR_HEIGHT = 2;
    
    switch (elem.type) {
        case epub::ElementType::LineBreak:
            y_pos += font_size_;
            break;
            
        case epub::ElementType::HorizontalRule: {
            HPEN pen = CreatePen(PS_SOLID, HR_HEIGHT, RGB(128, 128, 128));
            HPEN old_pen = (HPEN)SelectObject(hdc, pen);
            
            MoveToEx(hdc, rect.left + 20, y_pos + 5, NULL);
            LineTo(hdc, rect.right - 20, y_pos + 5);
            
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
            
            y_pos += 10 + PARAGRAPH_SPACING;
            break;
        }
            
        case epub::ElementType::Image:
            drawImage(hdc, elem.image_id, rect, y_pos);
            y_pos += PARAGRAPH_SPACING;
            break;
            
        case epub::ElementType::Heading1:
        case epub::ElementType::Heading2:
        case epub::ElementType::Heading3:
        case epub::ElementType::Heading4:
        case epub::ElementType::Heading5:
        case epub::ElementType::Heading6: {
            int heading_size = font_size_ + 8;
            HFONT heading_font = createFont(heading_size, true, false, false, false);
            HFONT old_font = (HFONT)SelectObject(hdc, heading_font);
            
            SetTextColor(hdc, RGB(0, 0, 0));
            
            RECT heading_rect = rect;
            heading_rect.top = y_pos;
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &heading_rect, 
                                  DT_WORDBREAK | DT_NOPREFIX);
            
            y_pos += height + HEADING_SPACING;
            
            SelectObject(hdc, old_font);
            DeleteObject(heading_font);
            break;
        }
        
        case epub::ElementType::CodeBlock: {
            RECT code_rect = rect;
            code_rect.top = y_pos;
            code_rect.bottom = y_pos + 1000;
            
            HFONT old_font = (HFONT)SelectObject(hdc, mono_font_);
            
            DrawTextW(hdc, elem.content.c_str(), -1, &code_rect, 
                     DT_CALCRECT | DT_NOPREFIX);
            
            code_rect.left -= 5;
            code_rect.right += 5;
            code_rect.top -= 3;
            code_rect.bottom += 3;
            
            HBRUSH brush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &code_rect, brush);
            DeleteObject(brush);
            
            code_rect.left += 5;
            code_rect.right -= 5;
            code_rect.top += 3;
            
            SetTextColor(hdc, RGB(0, 0, 0));
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &code_rect, 
                                  DT_NOPREFIX);
            
            y_pos = code_rect.bottom + PARAGRAPH_SPACING;
            
            SelectObject(hdc, old_font);
            break;
        }
        
        case epub::ElementType::ListItem: {
            rect.left += LIST_INDENT * elem.list_level;
            rect.top = y_pos;
            
            const wchar_t bullet_char = 0x2022;
            wchar_t bullet_str[3] = {bullet_char, L' ', L'\0'};
            
            HFONT font = selectFontForStyle(elem.style);
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            SetTextColor(hdc, RGB(0, 0, 0));
            
            DrawTextW(hdc, bullet_str, -1, &rect, DT_NOPREFIX);
            
            rect.left += 20;
            
            const bool has_underline = epub::hasStyle(elem.style, epub::TextStyle::Underline);
            const bool has_strikethrough = epub::hasStyle(elem.style, epub::TextStyle::Strikethrough);
            
            if (has_underline || has_strikethrough) {
                const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
                const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
                const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
                
                SelectObject(hdc, old_font);
                
                if (is_mono) {
                    HFONT styled_font = CreateFontW(
                        font_size_, 0, 0, 0,
                        is_bold ? FW_BOLD : FW_NORMAL,
                        is_italic ? TRUE : FALSE,
                        has_underline ? TRUE : FALSE,
                        has_strikethrough ? TRUE : FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
                    );
                    old_font = (HFONT)SelectObject(hdc, styled_font);
                } else {
                    HFONT styled_font = createFont(font_size_, is_bold, is_italic, has_underline, has_strikethrough);
                    old_font = (HFONT)SelectObject(hdc, styled_font);
                }
            }
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                                  DT_WORDBREAK | DT_NOPREFIX);
            
            y_pos += height + LINE_SPACING;
            
            SelectObject(hdc, old_font);
            if (has_underline || has_strikethrough) {
                DeleteObject(font);
            }
            break;
        }
        
        case epub::ElementType::Quote: {
            rect.left += QUOTE_INDENT;
            rect.right -= QUOTE_INDENT;
            rect.top = y_pos;
            
            const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
            HFONT font = is_bold ? bold_italic_font_ : italic_font_;
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            
            SetTextColor(hdc, RGB(80, 80, 80));
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                                  DT_WORDBREAK | DT_NOPREFIX);
            
            y_pos += height + PARAGRAPH_SPACING;
            
            SelectObject(hdc, old_font);
            break;
        }
        
        case epub::ElementType::Link:
        case epub::ElementType::Paragraph:
        case epub::ElementType::Text: {
            rect.top = y_pos;
            
            const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
            const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
            const bool has_underline = epub::hasStyle(elem.style, epub::TextStyle::Underline);
            const bool has_strikethrough = epub::hasStyle(elem.style, epub::TextStyle::Strikethrough);
            const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
            const bool is_small = epub::hasStyle(elem.style, epub::TextStyle::Small);
            
            int text_size = is_small ? static_cast<int>(font_size_ * 0.85) : font_size_;
            
            HFONT font;
            if (is_mono) {
                font = CreateFontW(
                    text_size, 0, 0, 0,
                    is_bold ? FW_BOLD : FW_NORMAL,
                    is_italic ? TRUE : FALSE,
                    has_underline ? TRUE : FALSE,
                    has_strikethrough ? TRUE : FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
                );
            } else {
                font = createFont(text_size, is_bold, is_italic, has_underline, has_strikethrough);
            }
            
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            
            if (elem.type == epub::ElementType::Link) {
                SetTextColor(hdc, RGB(0, 0, 255));
            } else {
                SetTextColor(hdc, RGB(0, 0, 0));
            }
            
            UINT format = DT_WORDBREAK | DT_NOPREFIX;
            
            switch (elem.align) {
                case epub::TextAlign::Center:
                    format |= DT_CENTER;
                    break;
                case epub::TextAlign::Right:
                    format |= DT_RIGHT;
                    break;
                case epub::TextAlign::Justify:
                    format |= DT_LEFT;
                    break;
                default:
                    format |= DT_LEFT;
                    break;
            }
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &rect, format);
            
            int spacing = (elem.type == epub::ElementType::Paragraph) ? 
                         PARAGRAPH_SPACING : LINE_SPACING;
            y_pos += height + spacing;
            
            SelectObject(hdc, old_font);
            DeleteObject(font);
            break;
        }
    }
}

void PageRenderer::drawImage(HDC hdc, const std::string& image_id, RECT& rect, int& y_pos) {
    if (!image_cache_) return;
    
    const epub::ImageData* img_data = image_cache_->getImage(image_id);
    if (!img_data) {
        LOG_WARNING("Image not found in cache:", image_id);
        return;
    }
    
    int content_width = rect.right - rect.left;
    
    float scale = static_cast<float>(content_width) / img_data->width;
    if (scale > 1.0f) scale = 1.0f;
    
    int draw_width = static_cast<int>(img_data->width * scale);
    int draw_height = static_cast<int>(img_data->height * scale);
    
    static bool gdiplus_initialized = false;
    static ULONG_PTR gdiplusToken;
    
    if (!gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        gdiplus_initialized = true;
    }
    
    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(
        img_data->width, 
        img_data->height,
        img_data->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB
    );
    
    if (bitmap) {
        Gdiplus::BitmapData bitmapData;
        Gdiplus::Rect bitmap_rect(0, 0, img_data->width, img_data->height);
        
        bitmap->LockBits(&bitmap_rect, Gdiplus::ImageLockModeWrite, 
                        img_data->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB,
                        &bitmapData);
        
        for (int y = 0; y < img_data->height; y++) {
            unsigned char* dest = (unsigned char*)bitmapData.Scan0 + y * bitmapData.Stride;
            const unsigned char* src = img_data->pixels.data() + y * img_data->width * img_data->channels;
            
            for (int x = 0; x < img_data->width; x++) {
                if (img_data->channels == 4) {
                    dest[x * 4 + 0] = src[x * 4 + 2];
                    dest[x * 4 + 1] = src[x * 4 + 1];
                    dest[x * 4 + 2] = src[x * 4 + 0];
                    dest[x * 4 + 3] = src[x * 4 + 3];
                } else {
                    dest[x * 3 + 0] = src[x * 3 + 2];
                    dest[x * 3 + 1] = src[x * 3 + 1];
                    dest[x * 3 + 2] = src[x * 3 + 0];
                }
            }
        }
        
        bitmap->UnlockBits(&bitmapData);
        
        Gdiplus::Graphics graphics(hdc);
        int x_pos = rect.left + (content_width - draw_width) / 2;
        
        graphics.DrawImage(bitmap, x_pos, y_pos, draw_width, draw_height);
        
        y_pos += draw_height;
        
        delete bitmap;
    }
}