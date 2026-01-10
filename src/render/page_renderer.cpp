#include "page_renderer.h"
#include "../utils/logger.h"
#include <algorithm>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#undef min
#undef max

// Static variables to track rendering position across multiple elements
// In a production environment, these should be part of a RenderContext struct passed around
static int current_x_position_ = 0;
static int current_row_max_height_ = 0;

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
    
    // Create default base fonts
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

// Helper overload to create font with specific face name
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

// Private helper to create fonts with custom family support
HFONT CreateFontWithFamily(int size, bool bold, bool italic, 
                          bool underline, bool strikethrough, const std::wstring& family) {
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
        family.c_str()
    );
}

// Helper to get font size based on element type
int getFontSizeForType(epub::ElementType type, int base_size) {
    switch (type) {
        case epub::ElementType::Heading1: return static_cast<int>(base_size * 2.0);
        case epub::ElementType::Heading2: return static_cast<int>(base_size * 1.5);
        case epub::ElementType::Heading3: return static_cast<int>(base_size * 1.17);
        case epub::ElementType::Heading4: return static_cast<int>(base_size * 1.0);
        case epub::ElementType::Heading5: return static_cast<int>(base_size * 0.83);
        case epub::ElementType::Heading6: return static_cast<int>(base_size * 0.67);
        default: return base_size;
    }
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
    
    size_t element_start_index = 0;
    int current_height = 0;
    
    // Reset global tracking state for calculation
    current_x_position_ = margin_;
    current_row_max_height_ = 0;
    
    for (size_t i = 0; i < content_.size(); i++) {
        int element_height = measureElementHeight(hdc, content_[i], content_width);
        
        // If an element exceeds the remaining page height, break the page
        if (current_height + element_height > content_height && i > element_start_index) {
            PageBreak page;
            page.element_start = element_start_index;
            page.element_count = i - element_start_index;
            pages_.push_back(page);
            
            element_start_index = i;
            current_height = 0;
            current_x_position_ = margin_;
            current_row_max_height_ = 0;
            
            // Re-measure for the new page context
            element_height = measureElementHeight(hdc, content_[i], content_width);
        }
        
        current_height += element_height;
    }
    
    if (element_start_index < content_.size()) {
        PageBreak page;
        page.element_start = element_start_index;
        page.element_count = content_.size() - element_start_index;
        pages_.push_back(page);
    }
    
    LOG_INFO("Pages calculated:", pages_.size());
}

int PageRenderer::measureElementHeight(HDC hdc, const epub::TextElement& element, int width) {
    if (element.type == epub::ElementType::LineBreak) {
        return font_size_;
    }

    if (element.type == epub::ElementType::Image) {
        // Estimate image height (simplified)
        return 300 + static_cast<int>(element.margin_top + element.margin_bottom);
    }

    // Determine font properties matching renderElement
    bool is_bold = epub::hasStyle(element.style, epub::TextStyle::Bold);
    bool is_italic = epub::hasStyle(element.style, epub::TextStyle::Italic);
    bool has_underline = epub::hasStyle(element.style, epub::TextStyle::Underline);
    bool has_strikethrough = epub::hasStyle(element.style, epub::TextStyle::Strikethrough);
    bool is_small = epub::hasStyle(element.style, epub::TextStyle::Small);

    int base_type_size = getFontSizeForType(element.type, font_size_);
    int text_size = is_small ? static_cast<int>(base_type_size * 0.85) : base_type_size;

    std::wstring family_to_use = font_name_;
    if (!element.font_family.empty()) {
        family_to_use = std::wstring(element.font_family.begin(), element.font_family.end());
    }

    HFONT font = CreateFontWithFamily(text_size, is_bold, is_italic, 
                                    has_underline, has_strikethrough, family_to_use);
    HFONT old_font = (HFONT)SelectObject(hdc, font);

    RECT calc_rect = { 0, 0, width, 0 };
    
    // Apply indent for the measurement if it's a new block
    if (!element.is_inline_continuation) {
        int indent_pixels = static_cast<int>(element.text_indent * font_size_);
        calc_rect.left += indent_pixels;
    }

    DrawTextW(hdc, element.content.c_str(), -1, &calc_rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);

    SelectObject(hdc, old_font);
    DeleteObject(font);

    int height = calc_rect.bottom - calc_rect.top;
    
    // Add margins if starting a new block
    if (!element.is_inline_continuation) {
        height += static_cast<int>(element.margin_top + element.margin_bottom);
    }

    return height;
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
    current_x_position_ = margin_;
    current_row_max_height_ = 0;
    
    for (size_t i = 0; i < page.element_count; i++) {
        size_t element_index = page.element_start + i;
        if (element_index >= content_.size()) break;
        
        RECT rect;
        rect.left = margin_;
        rect.top = y_pos;
        rect.right = viewport_width_ - margin_;
        rect.bottom = viewport_height_ - margin_;
        
        renderElement(hdc, content_[element_index], rect, y_pos);
    }
}

void PageRenderer::renderElement(HDC hdc, const epub::TextElement& element, 
                                RECT& rect, int& y_pos) {
    
    // Convert margins to pixels
    int margin_top_pixels = static_cast<int>(element.margin_top);
    int margin_bottom_pixels = static_cast<int>(element.margin_bottom);
    
    // If this is a new block (not a continuation of the previous line), reset X and apply top margin
    if (!element.is_inline_continuation) {
        // If there was a previous row pending, flush its height now
        if (current_row_max_height_ > 0) {
            y_pos += current_row_max_height_;
            current_row_max_height_ = 0;
        }
        
        current_x_position_ = rect.left;
        y_pos += margin_top_pixels;
    }
    
    switch (element.type) {
        case epub::ElementType::LineBreak:
            y_pos += font_size_; 
            current_x_position_ = rect.left;
            current_row_max_height_ = 0;
            break;
            
        case epub::ElementType::HorizontalRule: {
            if (current_row_max_height_ > 0) y_pos += current_row_max_height_;
            
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(128, 128, 128));
            HPEN old_pen = (HPEN)SelectObject(hdc, pen);
            
            MoveToEx(hdc, rect.left + 20, y_pos + 5, NULL);
            LineTo(hdc, rect.right - 20, y_pos + 5);
            
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
            
            y_pos += 10 + margin_bottom_pixels;
            current_x_position_ = rect.left;
            current_row_max_height_ = 0;
            break;
        }
            
        case epub::ElementType::Image:
            if (current_row_max_height_ > 0) {
                y_pos += current_row_max_height_;
                current_row_max_height_ = 0;
            }
            drawImage(hdc, element.image_id, rect, y_pos);
            y_pos += margin_bottom_pixels;
            current_x_position_ = rect.left;
            break;
            
        case epub::ElementType::Heading1:
        case epub::ElementType::Heading2:
        case epub::ElementType::Heading3:
        case epub::ElementType::Heading4:
        case epub::ElementType::Heading5:
        case epub::ElementType::Heading6:
        case epub::ElementType::Link:
        case epub::ElementType::Paragraph:
        case epub::ElementType::Text:
        case epub::ElementType::Quote:
        case epub::ElementType::ListItem:
        case epub::ElementType::CodeBlock: {
            
            // 1. Prepare Font with dynamic scaling for headings
            bool is_bold = epub::hasStyle(element.style, epub::TextStyle::Bold);
            bool is_italic = epub::hasStyle(element.style, epub::TextStyle::Italic);
            bool has_underline = epub::hasStyle(element.style, epub::TextStyle::Underline);
            bool has_strikethrough = epub::hasStyle(element.style, epub::TextStyle::Strikethrough);
            bool is_small = epub::hasStyle(element.style, epub::TextStyle::Small);
            
            int base_type_size = getFontSizeForType(element.type, font_size_);
            int text_size = is_small ? static_cast<int>(base_type_size * 0.85) : base_type_size;
            
            // Use custom font family if provided, otherwise fallback to default
            std::wstring family_to_use = font_name_;
            if (!element.font_family.empty()) {
                family_to_use = std::wstring(element.font_family.begin(), element.font_family.end());
            }
            
            HFONT font = CreateFontWithFamily(text_size, is_bold, is_italic, 
                                            has_underline, has_strikethrough, family_to_use);
            
            HFONT old_font = (HFONT)SelectObject(hdc, font);
            
            // 2. Set Colors
            if (element.type == epub::ElementType::Link) {
                SetTextColor(hdc, RGB(0, 0, 255));
            } else if (element.type == epub::ElementType::Quote) {
                SetTextColor(hdc, RGB(80, 80, 80));
            } else {
                SetTextColor(hdc, RGB(0, 0, 0));
            }
            
            // 3. Calculate Geometry
            RECT text_rect = rect;
            
            // If start of block, apply text-indent
            if (!element.is_inline_continuation) {
                int indent_pixels = static_cast<int>(element.text_indent * font_size_);
                current_x_position_ += indent_pixels;
            }
            
            text_rect.left = current_x_position_;
            text_rect.top = y_pos;
            
            // 4. Setup DrawText flags
            UINT format = DT_NOPREFIX | DT_WORDBREAK;
            
            switch (element.align) {
                case epub::TextAlign::Center:
                    format |= DT_CENTER;
                    if (!element.is_inline_continuation) text_rect.left = rect.left;
                    break;
                case epub::TextAlign::Right:
                    format |= DT_RIGHT;
                    if (!element.is_inline_continuation) text_rect.left = rect.left;
                    break;
                default:
                    format |= DT_LEFT;
                    break;
            }
            
            // 5. Measure and Draw
            RECT calculation_rect = text_rect;
            DrawTextW(hdc, element.content.c_str(), -1, &calculation_rect, format | DT_CALCRECT);
            int height_drawn = calculation_rect.bottom - calculation_rect.top;
            int width_drawn = calculation_rect.right - calculation_rect.left;
            
            DrawTextW(hdc, element.content.c_str(), -1, &text_rect, format);
            
            // 6. Update Cursor Positions
            // Heuristic for wrap: if width is close to viewport width or height > single line
            if (height_drawn > (text_size * 1.5)) {
                y_pos += height_drawn;
                current_x_position_ = rect.left;
                current_row_max_height_ = 0;
            } else {
                current_x_position_ += width_drawn;
                if (height_drawn > current_row_max_height_) {
                    current_row_max_height_ = height_drawn;
                }
            }
            
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