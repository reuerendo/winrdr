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

HFONT PageRenderer::selectFontForStyle(epub::TextStyle style, float size_multiplier) {
    const bool is_bold = epub::hasStyle(style, epub::TextStyle::Bold);
    const bool is_italic = epub::hasStyle(style, epub::TextStyle::Italic);
    const bool is_mono = epub::hasStyle(style, epub::TextStyle::Monospace);
    
    int actual_size = static_cast<int>(font_size_ * size_multiplier);
    
    if (is_mono) {
        return CreateFontW(
            actual_size, 0, 0, 0,
            is_bold ? FW_BOLD : FW_NORMAL,
            is_italic ? TRUE : FALSE,
            FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
        );
    }
    
    if (is_bold && is_italic) {
        return createFont(actual_size, true, true, false, false);
    }
    if (is_bold) {
        return createFont(actual_size, true, false, false, false);
    }
    if (is_italic) {
        return createFont(actual_size, false, true, false, false);
    }
    
    return createFont(actual_size, false, false, false, false);
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
    const int base_font_size = static_cast<int>(font_size_ * elem.css_font_size);
    
    const int default_line_spacing = static_cast<int>(base_font_size * 0.3f);
    const int calculated_line_spacing = elem.css_line_height > 0.0f ? 
        static_cast<int>(base_font_size * (elem.css_line_height - 1.0f)) : 
        default_line_spacing;
    
    const int list_indent = 30;
    const int hr_height = 10;
    
    int total_height = elem.css_margin_top + elem.css_padding_top;
    
    switch (elem.type) {
        case epub::ElementType::LineBreak:
            total_height += base_font_size;
            break;
            
        case epub::ElementType::HorizontalRule:
            total_height += hr_height + calculated_line_spacing;
            break;
            
        case epub::ElementType::Image: {
            if (image_cache_) {
                const epub::ImageData* img = image_cache_->getImage(elem.image_id);
                if (img) {
                    int available_width = width - elem.css_margin_left - elem.css_margin_right - 
                                         elem.css_padding_left - elem.css_padding_right;
                    float scale = static_cast<float>(available_width) / img->width;
                    if (scale > 1.0f) scale = 1.0f;
                    total_height += static_cast<int>(img->height * scale) + calculated_line_spacing;
                }
            }
            break;
        }
        
        case epub::ElementType::Heading1:
        case epub::ElementType::Heading2:
        case epub::ElementType::Heading3:
        case epub::ElementType::Heading4:
        case epub::ElementType::Heading5:
        case epub::ElementType::Heading6: {
            int heading_size = static_cast<int>((base_font_size + 8) * elem.css_font_size);
            HFONT heading_font = createFont(heading_size, true, false, false, false);
            HFONT old_font = (HFONT)SelectObject(hdc, heading_font);
            
            int available_width = width - elem.css_margin_left - elem.css_margin_right - 
                                 elem.css_padding_left - elem.css_padding_right;
            RECT rect = {0, 0, available_width, 0};
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            
            SelectObject(hdc, old_font);
            DeleteObject(heading_font);
            
            int heading_spacing = calculated_line_spacing * 2;
            total_height += rect.bottom + heading_spacing;
            break;
        }
        
        case epub::ElementType::CodeBlock: {
            int available_width = width - elem.css_margin_left - elem.css_margin_right - 
                                 elem.css_padding_left - elem.css_padding_right;
            HFONT old_font = (HFONT)SelectObject(hdc, mono_font_);
            RECT rect = {0, 0, available_width, 0};
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_NOPREFIX);
            SelectObject(hdc, old_font);
            total_height += rect.bottom + calculated_line_spacing;
            break;
        }
        
        case epub::ElementType::ListItem: {
            int list_width = width - (list_indent * elem.list_level) - 
                            elem.css_margin_left - elem.css_margin_right - 
                            elem.css_padding_left - elem.css_padding_right;
            RECT rect = {0, 0, list_width, 0};
            
            HFONT font = selectFontForStyle(elem.style, elem.css_font_size);
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(hdc, old_font);
            DeleteObject(font);
            
            total_height += rect.bottom + calculated_line_spacing;
            break;
        }
        
        case epub::ElementType::Quote:
        case epub::ElementType::Paragraph:
        case epub::ElementType::Text:
        case epub::ElementType::Link: {
            int text_width = width - elem.css_margin_left - elem.css_margin_right - 
                            elem.css_padding_left - elem.css_padding_right;
            RECT rect = {0, 0, text_width, 0};
            
            HFONT font = selectFontForStyle(elem.style, elem.css_font_size);
            
            if (epub::hasStyle(elem.style, epub::TextStyle::Small)) {
                int small_size = static_cast<int>(base_font_size * 0.85);
                const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
                const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
                const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
                
                DeleteObject(font);
                
                if (is_mono) {
                    font = CreateFontW(
                        small_size, 0, 0, 0, is_bold ? FW_BOLD : FW_NORMAL, 
                        is_italic ? TRUE : FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
                    );
                } else {
                    font = createFont(small_size, is_bold, is_italic, false, false);
                }
            }
            
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            DrawTextW(hdc, elem.content.c_str(), -1, &rect, 
                     DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(hdc, old_font);
            DeleteObject(font);
            
            total_height += rect.bottom + calculated_line_spacing;
            break;
        }
    }
    
    total_height += elem.css_margin_bottom + elem.css_padding_bottom;
    
    return total_height;
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
    const int base_font_size = static_cast<int>(font_size_ * elem.css_font_size);
    
    const int default_line_spacing = static_cast<int>(base_font_size * 0.3f);
    const int calculated_line_spacing = elem.css_line_height > 0.0f ? 
        static_cast<int>(base_font_size * (elem.css_line_height - 1.0f)) : 
        default_line_spacing;
    
    const int list_indent = 30;
    const int hr_height = 2;
    
    y_pos += elem.css_margin_top;
    
    RECT work_rect = rect;
    work_rect.left += elem.css_margin_left + elem.css_padding_left;
    work_rect.right -= elem.css_margin_right + elem.css_padding_right;
    work_rect.top = y_pos + elem.css_padding_top;
    
    switch (elem.type) {
        case epub::ElementType::LineBreak:
            y_pos += base_font_size;
            break;
            
        case epub::ElementType::HorizontalRule: {
            HPEN pen = CreatePen(PS_SOLID, hr_height, RGB(128, 128, 128));
            HPEN old_pen = (HPEN)SelectObject(hdc, pen);
            
            MoveToEx(hdc, work_rect.left + 20, work_rect.top + 5, NULL);
            LineTo(hdc, work_rect.right - 20, work_rect.top + 5);
            
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
            
            y_pos += 10 + calculated_line_spacing;
            break;
        }
            
        case epub::ElementType::Image:
            drawImage(hdc, elem.image_id, work_rect, y_pos);
            y_pos += calculated_line_spacing;
            break;
            
        case epub::ElementType::Heading1:
        case epub::ElementType::Heading2:
        case epub::ElementType::Heading3:
        case epub::ElementType::Heading4:
        case epub::ElementType::Heading5:
        case epub::ElementType::Heading6: {
            int heading_size = static_cast<int>((base_font_size + 8) * elem.css_font_size);
            HFONT heading_font = createFont(heading_size, true, false, false, false);
            HFONT old_font = (HFONT)SelectObject(hdc, heading_font);
            
            SetTextColor(hdc, RGB(0, 0, 0));
            
            RECT heading_rect = work_rect;
            heading_rect.top = work_rect.top;
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &heading_rect, 
                                  DT_WORDBREAK | DT_NOPREFIX | DT_CENTER);
            
            y_pos = heading_rect.top + height + (calculated_line_spacing * 2);
            
            SelectObject(hdc, old_font);
            DeleteObject(heading_font);
            break;
        }
        
        case epub::ElementType::CodeBlock: {
            RECT code_rect = work_rect;
            code_rect.top = work_rect.top;
            code_rect.bottom = work_rect.top + 1000;
            
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
            
            y_pos = code_rect.bottom + calculated_line_spacing;
            
            SelectObject(hdc, old_font);
            break;
        }
        
        case epub::ElementType::ListItem: {
            RECT list_rect = work_rect;
            list_rect.left += list_indent * elem.list_level;
            list_rect.top = work_rect.top;
            
            const wchar_t bullet_char = 0x2022;
            wchar_t bullet_str[3] = {bullet_char, L' ', L'\0'};
            
            HFONT font = selectFontForStyle(elem.style, elem.css_font_size);
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            SetTextColor(hdc, RGB(0, 0, 0));
            
            DrawTextW(hdc, bullet_str, -1, &list_rect, DT_NOPREFIX);
            
            list_rect.left += 20;
            
            const bool has_underline = epub::hasStyle(elem.style, epub::TextStyle::Underline);
            const bool has_strikethrough = epub::hasStyle(elem.style, epub::TextStyle::Strikethrough);
            
            if (has_underline || has_strikethrough) {
                const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
                const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
                const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
                
                SelectObject(hdc, old_font);
                DeleteObject(font);
                
                int actual_size = static_cast<int>(font_size_ * elem.css_font_size);
                
                if (is_mono) {
                    font = CreateFontW(
                        actual_size, 0, 0, 0,
                        is_bold ? FW_BOLD : FW_NORMAL,
                        is_italic ? TRUE : FALSE,
                        has_underline ? TRUE : FALSE,
                        has_strikethrough ? TRUE : FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New"
                    );
                } else {
                    font = createFont(actual_size, is_bold, is_italic, has_underline, has_strikethrough);
                }
                old_font = (HFONT)SelectObject(hdc, font);
            }
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &list_rect, 
                                  DT_WORDBREAK | DT_NOPREFIX);
            
            y_pos = list_rect.top + height + calculated_line_spacing;
            
            SelectObject(hdc, old_font);
            DeleteObject(font);
            break;
        }
        
        case epub::ElementType::Quote: {
            const int quote_indent = 40;
            RECT quote_rect = work_rect;
            quote_rect.left += quote_indent;
            quote_rect.right -= quote_indent;
            quote_rect.top = work_rect.top;
            
            const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
            int actual_size = static_cast<int>(font_size_ * elem.css_font_size);
            
            HFONT font = is_bold ? 
                createFont(actual_size, true, true, false, false) : 
                createFont(actual_size, false, true, false, false);
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            
            SetTextColor(hdc, RGB(80, 80, 80));
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &quote_rect, 
                                  DT_WORDBREAK | DT_NOPREFIX);
            
            y_pos = quote_rect.top + height + calculated_line_spacing;
            
            SelectObject(hdc, old_font);
            DeleteObject(font);
            break;
        }
        
        case epub::ElementType::Link:
        case epub::ElementType::Paragraph:
        case epub::ElementType::Text: {
            RECT text_rect = work_rect;
            text_rect.top = work_rect.top;
            
            if (elem.css_text_indent != 0 && elem.type == epub::ElementType::Paragraph) {
                text_rect.left += elem.css_text_indent;
            }
            
            const bool is_bold = epub::hasStyle(elem.style, epub::TextStyle::Bold);
            const bool is_italic = epub::hasStyle(elem.style, epub::TextStyle::Italic);
            const bool has_underline = epub::hasStyle(elem.style, epub::TextStyle::Underline);
            const bool has_strikethrough = epub::hasStyle(elem.style, epub::TextStyle::Strikethrough);
            const bool is_mono = epub::hasStyle(elem.style, epub::TextStyle::Monospace);
            const bool is_small = epub::hasStyle(elem.style, epub::TextStyle::Small);
            
            int text_size = is_small ? 
                static_cast<int>(font_size_ * elem.css_font_size * 0.85f) : 
                static_cast<int>(font_size_ * elem.css_font_size);
            
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
            
            int height = DrawTextW(hdc, elem.content.c_str(), -1, &text_rect, format);
            
            y_pos = text_rect.top + height + calculated_line_spacing;
            
            SelectObject(hdc, old_font);
            DeleteObject(font);
            break;
        }
    }
    
    y_pos += elem.css_margin_bottom + elem.css_padding_bottom;
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