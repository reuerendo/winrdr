#include "litehtml_container.h"
#include "../utils/logger.h"
#include <algorithm>
#include <gdiplus.h>

// Гарантируем, что заголовки знают, что мы используем UTF-8
#ifndef LITEHTML_UTF8
#define LITEHTML_UTF8
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {
    const int DEFAULT_FONT_SIZE = 16;
    const char* DEFAULT_FONT_NAME = "Arial";
    const int DPI = 96;
}

LitehtmlContainer::LitehtmlContainer(HDC hdc, epub::ImageCache* image_cache)
    : hdc_(hdc)
    , image_cache_(image_cache)
    , next_font_id_(1)
    , viewport_width_(600)
    , viewport_height_(800)
{
    LOG_DEBUG("LitehtmlContainer created");
}

LitehtmlContainer::~LitehtmlContainer() {
    for (auto& pair : fonts_) {
        if (pair.second.hfont) {
            DeleteObject(pair.second.hfont);
        }
    }
    fonts_.clear();
    
    for (HRGN region : clip_regions_) {
        DeleteObject(region);
    }
    clip_regions_.clear();
    
    LOG_DEBUG("LitehtmlContainer destroyed");
}

litehtml::uint_ptr LitehtmlContainer::create_font(const litehtml::font_description& font_description,
                                                   const litehtml::document* doc,
                                                   litehtml::font_metrics* fm) {
    try {
        std::wstring font_face = utf8_to_wstring(font_description.family);
        
        if (font_face.empty()) {
            font_face = L"Arial";
        }
        
        int font_weight = FW_NORMAL;
        if (font_description.weight >= 700) {
            font_weight = FW_BOLD;
        } else if (font_description.weight >= 600) {
            font_weight = FW_SEMIBOLD;
        } else if (font_description.weight >= 300) {
            font_weight = FW_NORMAL;
        } else {
            font_weight = FW_LIGHT;
        }
        
        BOOL is_italic = (font_description.style == litehtml::font_style_italic) ? TRUE : FALSE;
        
        int font_size = (int)font_description.size;
        if (font_size <= 0) {
            font_size = DEFAULT_FONT_SIZE;
        }
        
        HFONT hfont = CreateFontW(
            -MulDiv(font_size, DPI, 72),
            0,
            0,
            0,
            font_weight,
            is_italic,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            font_face.c_str()
        );
        
        if (!hfont) {
            LOG_ERROR("Failed to create font:", font_description.family);
            return 0;
        }
        
        if (fm) {
            HDC use_hdc = hdc_;
            bool temp_hdc = false;
            
            if (!use_hdc) {
                use_hdc = GetDC(NULL);
                temp_hdc = true;
            }
            
            if (use_hdc) {
                HFONT old_font = (HFONT)SelectObject(use_hdc, hfont);
                
                TEXTMETRICW tm;
                if (GetTextMetricsW(use_hdc, &tm)) {
                    fm->height = tm.tmHeight;
                    fm->ascent = tm.tmAscent;
                    fm->descent = tm.tmDescent;
                    fm->x_height = tm.tmHeight / 2;
                } else {
                    fm->height = font_size;
                    fm->ascent = font_size * 3 / 4;
                    fm->descent = font_size / 4;
                    fm->x_height = font_size / 2;
                }
                
                SelectObject(use_hdc, old_font);
                
                if (temp_hdc) {
                    ReleaseDC(NULL, use_hdc);
                }
            }
        }
        
        FontInfo info;
        info.hfont = hfont;
        
        litehtml::uint_ptr font_id = next_font_id_++;
        fonts_[font_id] = info;
        
        return font_id;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in create_font:", e.what());
        return 0;
    }
}

void LitehtmlContainer::delete_font(litehtml::uint_ptr hFont) {
    auto it = fonts_.find(hFont);
    if (it != fonts_.end()) {
        if (it->second.hfont) {
            DeleteObject(it->second.hfont);
        }
        fonts_.erase(it);
    }
}

litehtml::pixel_t LitehtmlContainer::text_width(const char* text, litehtml::uint_ptr hFont) {
    if (!text) {
        return 0;
    }
    
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) {
        return 0;
    }
    
    try {
        std::wstring wtext = utf8_to_wstring(text);
        if (wtext.empty()) return 0;
        
        HDC use_hdc = hdc_;
        bool temp_hdc = false;
        
        if (!use_hdc) {
            use_hdc = GetDC(NULL);
            temp_hdc = true;
        }
        
        if (!use_hdc) {
            return 0;
        }
        
        HFONT old_font = (HFONT)SelectObject(use_hdc, it->second.hfont);
        
        SIZE sz = {0, 0};
        GetTextExtentPoint32W(use_hdc, wtext.c_str(), (int)wtext.length(), &sz);
        
        SelectObject(use_hdc, old_font);
        
        if (temp_hdc) {
            ReleaseDC(NULL, use_hdc);
        }
        
        return sz.cx;
        
    } catch (...) {
        return 0;
    }
}

void LitehtmlContainer::draw_text(litehtml::uint_ptr hdc,
                                 const char* text,
                                 litehtml::uint_ptr hFont,
                                 litehtml::web_color color,
                                 const litehtml::position& pos) {
    if (!text) {
        return;
    }
    
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) {
        return;
    }
    
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) {
        return;
    }
    
    try {
        std::wstring wtext = utf8_to_wstring(text);
        
        HFONT old_font = (HFONT)SelectObject(target_hdc, it->second.hfont);
        
        SetBkMode(target_hdc, TRANSPARENT);
        SetTextColor(target_hdc, web_color_to_colorref(color));
        
        TextOutW(target_hdc, pos.x, pos.y, wtext.c_str(), (int)wtext.length());
        
        SelectObject(target_hdc, old_font);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in draw_text:", e.what());
    }
}

litehtml::pixel_t LitehtmlContainer::pt_to_px(float pt) const {
    return (litehtml::pixel_t)MulDiv((int)pt, DPI, 72);
}

litehtml::pixel_t LitehtmlContainer::get_default_font_size() const {
    return DEFAULT_FONT_SIZE;
}

const char* LitehtmlContainer::get_default_font_name() const {
    return DEFAULT_FONT_NAME;
}

void LitehtmlContainer::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) {
        return;
    }
    
    try {
        HBRUSH brush = CreateSolidBrush(web_color_to_colorref(marker.color));
        HBRUSH old_brush = (HBRUSH)SelectObject(target_hdc, brush);
        HPEN pen = CreatePen(PS_SOLID, 1, web_color_to_colorref(marker.color));
        HPEN old_pen = (HPEN)SelectObject(target_hdc, pen);
        
        switch (marker.marker_type) {
            case litehtml::list_style_type_circle:
            case litehtml::list_style_type_disc:
                Ellipse(target_hdc, 
                       marker.pos.x, 
                       marker.pos.y, 
                       marker.pos.x + marker.pos.width, 
                       marker.pos.y + marker.pos.height);
                break;
                
            case litehtml::list_style_type_square:
                Rectangle(target_hdc, 
                         marker.pos.x, 
                         marker.pos.y, 
                         marker.pos.x + marker.pos.width, 
                         marker.pos.y + marker.pos.height);
                break;
                
            default:
                if (marker.marker_type >= litehtml::list_style_type_decimal) {
                    SetBkMode(target_hdc, TRANSPARENT);
                    SetTextColor(target_hdc, web_color_to_colorref(marker.color));
                    
                    std::wstring wtext = utf8_to_wstring(marker.image.c_str());
                    
                    if (marker.font != 0) {
                        auto it = fonts_.find(marker.font);
                        if (it != fonts_.end()) {
                            HFONT old_font = (HFONT)SelectObject(target_hdc, it->second.hfont);
                            TextOutW(target_hdc, marker.pos.x, marker.pos.y, wtext.c_str(), (int)wtext.length());
                            SelectObject(target_hdc, old_font);
                        }
                    }
                }
                break;
        }
        
        SelectObject(target_hdc, old_pen);
        SelectObject(target_hdc, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
        
    } catch (...) {
        LOG_ERROR("Exception in draw_list_marker");
    }
}

void LitehtmlContainer::load_image(const char* src,
                                   const char* baseurl,
                                   bool redraw_on_ready) {
}

void LitehtmlContainer::get_image_size(const char* src,
                                      const char* baseurl,
                                      litehtml::size& sz) {
    sz.width = 0;
    sz.height = 0;
    
    if (!src || !image_cache_) {
        return;
    }
    
    const epub::ImageData* img = image_cache_->getImage(src);
    if (img) {
        sz.width = img->width;
        sz.height = img->height;
    }
}

void LitehtmlContainer::draw_image(litehtml::uint_ptr hdc,
                                  const litehtml::background_layer& layer,
                                  const std::string& url,
                                  const std::string& base_url) {
    if (!image_cache_ || url.empty()) {
        return;
    }
    
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) {
        return;
    }
    
    const epub::ImageData* img = image_cache_->getImage(url.c_str());
    if (!img || img->pixels.empty()) {
        return;
    }
    
    try {
        Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(
            img->width,
            img->height,
            img->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB
        );
        
        if (!bitmap) return;
        
        Gdiplus::BitmapData bitmapData;
        Gdiplus::Rect bitmap_rect(0, 0, img->width, img->height);
        
        Gdiplus::Status status = bitmap->LockBits(
            &bitmap_rect, 
            Gdiplus::ImageLockModeWrite,
            img->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB,
            &bitmapData
        );
        
        if (status == Gdiplus::Ok) {
            for (int y = 0; y < img->height; y++) {
                unsigned char* dest = (unsigned char*)bitmapData.Scan0 + y * bitmapData.Stride;
                const unsigned char* src = img->pixels.data() + y * img->width * img->channels;
                
                for (int x = 0; x < img->width; x++) {
                    if (img->channels == 4) {
                        dest[x * 4 + 0] = src[x * 4 + 2];
                        dest[x * 4 + 1] = src[x * 4 + 1];
                        dest[x * 4 + 2] = src[x * 4 + 0];
                        dest[x * 4 + 3] = src[x * 4 + 3];
                    } else if (img->channels == 3) {
                        dest[x * 3 + 0] = src[x * 3 + 2];
                        dest[x * 3 + 1] = src[x * 3 + 1];
                        dest[x * 3 + 2] = src[x * 3 + 0];
                    }
                }
            }
            
            bitmap->UnlockBits(&bitmapData);
            
            Gdiplus::Graphics graphics(target_hdc);
            graphics.DrawImage(
                bitmap,
                layer.clip_box.x,
                layer.clip_box.y,
                layer.clip_box.width,
                layer.clip_box.height
            );
        }
        
        delete bitmap;
        
    } catch (...) {
        LOG_ERROR("Exception in draw_image");
    }
}

void LitehtmlContainer::draw_solid_fill(litehtml::uint_ptr hdc,
                                       const litehtml::background_layer& layer,
                                       const litehtml::web_color& color) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) {
        return;
    }
    
    if (color.alpha == 0) {
        return;
    }
    
    try {
        RECT rect;
        rect.left = layer.clip_box.x;
        rect.top = layer.clip_box.y;
        rect.right = layer.clip_box.x + layer.clip_box.width;
        rect.bottom = layer.clip_box.y + layer.clip_box.height;
        
        HBRUSH brush = CreateSolidBrush(web_color_to_colorref(color));
        FillRect(target_hdc, &rect, brush);
        DeleteObject(brush);
    } catch (...) {
    }
}

void LitehtmlContainer::draw_linear_gradient(litehtml::uint_ptr hdc,
                                            const litehtml::background_layer& layer,
                                            const litehtml::background_layer::linear_gradient& gradient) {
}

void LitehtmlContainer::draw_radial_gradient(litehtml::uint_ptr hdc,
                                            const litehtml::background_layer& layer,
                                            const litehtml::background_layer::radial_gradient& gradient) {
}

void LitehtmlContainer::draw_conic_gradient(litehtml::uint_ptr hdc,
                                           const litehtml::background_layer& layer,
                                           const litehtml::background_layer::conic_gradient& gradient) {
}

void LitehtmlContainer::draw_borders(litehtml::uint_ptr hdc,
                                     const litehtml::borders& borders,
                                     const litehtml::position& draw_pos,
                                     bool root) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) {
        return;
    }
    
    try {
        auto draw_border = [&](int x1, int y1, int x2, int y2, const litehtml::border& border) {
            if (border.width <= 0 || border.color.alpha == 0) {
                return;
            }
            
            HPEN pen = CreatePen(PS_SOLID, border.width, web_color_to_colorref(border.color));
            HPEN old_pen = (HPEN)SelectObject(target_hdc, pen);
            
            MoveToEx(target_hdc, x1, y1, NULL);
            LineTo(target_hdc, x2, y2);
            
            SelectObject(target_hdc, old_pen);
            DeleteObject(pen);
        };
        
        draw_border(draw_pos.left(), draw_pos.top(),
                   draw_pos.right(), draw_pos.top(),
                   borders.top);
        
        draw_border(draw_pos.right(), draw_pos.top(),
                   draw_pos.right(), draw_pos.bottom(),
                   borders.right);
        
        draw_border(draw_pos.right(), draw_pos.bottom(),
                   draw_pos.left(), draw_pos.bottom(),
                   borders.bottom);
        
        draw_border(draw_pos.left(), draw_pos.bottom(),
                   draw_pos.left(), draw_pos.top(),
                   borders.left);
    } catch (...) {
    }
}

void LitehtmlContainer::set_caption(const char* caption) {
}

void LitehtmlContainer::set_base_url(const char* base_url) {
}

void LitehtmlContainer::link(const std::shared_ptr<litehtml::document>& doc,
                             const litehtml::element::ptr& el) {
}

void LitehtmlContainer::on_anchor_click(const char* url,
                                       const litehtml::element::ptr& el) {
}

void LitehtmlContainer::on_mouse_event(const litehtml::element::ptr& el,
                                      litehtml::mouse_event event) {
}

void LitehtmlContainer::set_cursor(const char* cursor) {
}

void LitehtmlContainer::transform_text(litehtml::string& text, litehtml::text_transform tt) {
    if (text.empty()) {
        return;
    }
    
    try {
        std::wstring wtext = utf8_to_wstring(text.c_str());
        
        switch (tt) {
            case litehtml::text_transform_capitalize:
                if (!wtext.empty()) {
                    wtext[0] = towupper(wtext[0]);
                }
                break;
                
            case litehtml::text_transform_uppercase:
                std::transform(wtext.begin(), wtext.end(), wtext.begin(), towupper);
                break;
                
            case litehtml::text_transform_lowercase:
                std::transform(wtext.begin(), wtext.end(), wtext.begin(), towlower);
                break;
                
            default:
                break;
        }
        
        text = wstring_to_utf8(wtext);
        
    } catch (...) {
    }
}

void LitehtmlContainer::import_css(litehtml::string& text,
                                   const litehtml::string& url,
                                   litehtml::string& baseurl) {
}

void LitehtmlContainer::set_clip(const litehtml::position& pos,
                                const litehtml::border_radiuses& bdr_radius) {
    if (!hdc_) {
        return;
    }
    
    try {
        HRGN region = CreateRectRgn(pos.x, pos.y, pos.x + pos.width, pos.y + pos.height);
        
        if (clip_regions_.empty()) {
            SelectClipRgn(hdc_, region);
        } else {
            ExtSelectClipRgn(hdc_, region, RGN_AND);
        }
        
        clip_regions_.push_back(region);
        
    } catch (...) {
    }
}

void LitehtmlContainer::del_clip() {
    if (!hdc_ || clip_regions_.empty()) {
        return;
    }
    
    try {
        HRGN region = clip_regions_.back();
        DeleteObject(region);
        clip_regions_.pop_back();
        
        if (clip_regions_.empty()) {
            SelectClipRgn(hdc_, NULL);
        } else {
            SelectClipRgn(hdc_, clip_regions_.back());
        }
    } catch (...) {
    }
}

void LitehtmlContainer::get_viewport(litehtml::position& viewport) const {
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = viewport_width_;
    viewport.height = viewport_height_;
}

std::shared_ptr<litehtml::element> LitehtmlContainer::create_element(const char* tag_name,
                                                                      const litehtml::string_map& attributes,
                                                                      const std::shared_ptr<litehtml::document>& doc) {
    // Returning nullptr allows litehtml to fallback to default generic element.
    // However, explicit handling or logging can be added here if specific custom elements are needed.
    return nullptr;
}

void LitehtmlContainer::get_media_features(litehtml::media_features& media) const {
    media.type = litehtml::media_type_screen;
    media.width = viewport_width_;
    media.height = viewport_height_;
    media.device_width = viewport_width_;
    media.device_height = viewport_height_;
    media.color = 8;
    media.monochrome = 0;
    media.color_index = 256;
    media.resolution = DPI;
}

void LitehtmlContainer::get_language(litehtml::string& language, litehtml::string& culture) const {
    language = "en";
    culture = "";
}

litehtml::string LitehtmlContainer::resolve_color(const litehtml::string& color) const {
    return color;
}

std::wstring LitehtmlContainer::utf8_to_wstring(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
    if (size <= 0) return std::wstring();
    
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], size);
    return result;
}

std::string LitehtmlContainer::wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &result[0], size, nullptr, nullptr);
    return result;
}

COLORREF LitehtmlContainer::web_color_to_colorref(litehtml::web_color color) {
    return RGB(color.red, color.green, color.blue);
}