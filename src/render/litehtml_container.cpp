#include "litehtml_container.h"
#include "../utils/logger.h"
#include <algorithm>
#include <gdiplus.h>

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
}

litehtml::uint_ptr LitehtmlContainer::create_font(const litehtml::font_description& font_description,
                                                   const litehtml::document* doc,
                                                   litehtml::font_metrics* fm) {
    try {
        std::wstring font_face = utf8_to_wstring(font_description.family);
        if (font_face.empty()) font_face = L"Arial";
        
        int font_weight = FW_NORMAL;
        if (font_description.weight >= 700) font_weight = FW_BOLD;
        else if (font_description.weight >= 600) font_weight = FW_SEMIBOLD;
        else if (font_description.weight >= 300) font_weight = FW_NORMAL;
        else font_weight = FW_LIGHT;
        
        BOOL is_italic = (font_description.style == litehtml::font_style_italic) ? TRUE : FALSE;
        int font_size = (int)font_description.size;
        if (font_size <= 0) font_size = DEFAULT_FONT_SIZE;
        
        HFONT hfont = CreateFontW(
            -MulDiv(font_size, DPI, 72), 0, 0, 0, font_weight, is_italic, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, font_face.c_str()
        );
        
        if (fm) {
            HDC use_hdc = hdc_ ? hdc_ : GetDC(NULL);
            HFONT old_font = (HFONT)SelectObject(use_hdc, hfont);
            TEXTMETRICW tm;
            GetTextMetricsW(use_hdc, &tm);
            fm->height = tm.tmHeight;
            fm->ascent = tm.tmAscent;
            fm->descent = tm.tmDescent;
            fm->x_height = tm.tmHeight / 2;
            SelectObject(use_hdc, old_font);
            if (!hdc_) ReleaseDC(NULL, use_hdc);
        }
        
        FontInfo info;
        info.hfont = hfont;
        litehtml::uint_ptr font_id = next_font_id_++;
        fonts_[font_id] = info;
        return font_id;
    } catch (...) { return 0; }
}

void LitehtmlContainer::delete_font(litehtml::uint_ptr hFont) {
    auto it = fonts_.find(hFont);
    if (it != fonts_.end()) {
        if (it->second.hfont) DeleteObject(it->second.hfont);
        fonts_.erase(it);
    }
}

litehtml::pixel_t LitehtmlContainer::text_width(const char* text, litehtml::uint_ptr hFont) {
    if (!text) return 0;
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) return 0;
    
    try {
        std::wstring wtext = utf8_to_wstring(text);
        if (wtext.empty()) return 0;
        
        HDC use_hdc = hdc_ ? hdc_ : GetDC(NULL);
        HFONT old_font = (HFONT)SelectObject(use_hdc, it->second.hfont);
        SIZE sz = {0, 0};
        GetTextExtentPoint32W(use_hdc, wtext.c_str(), (int)wtext.length(), &sz);
        SelectObject(use_hdc, old_font);
        if (!hdc_) ReleaseDC(NULL, use_hdc);
        return sz.cx;
    } catch (...) { return 0; }
}

void LitehtmlContainer::draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
    if (!text) return;
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) return;
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) return;
    
    try {
        std::wstring wtext = utf8_to_wstring(text);
        HFONT old_font = (HFONT)SelectObject(target_hdc, it->second.hfont);
        SetBkMode(target_hdc, TRANSPARENT);
        SetTextColor(target_hdc, web_color_to_colorref(color));
        TextOutW(target_hdc, pos.x, pos.y, wtext.c_str(), (int)wtext.length());
        SelectObject(target_hdc, old_font);
    } catch (...) {}
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
    // Упрощенная реализация маркеров
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) return;
    
    HBRUSH brush = CreateSolidBrush(web_color_to_colorref(marker.color));
    HBRUSH old_brush = (HBRUSH)SelectObject(target_hdc, brush);
    
    if (marker.marker_type == litehtml::list_style_type_disc || marker.marker_type == litehtml::list_style_type_circle) {
        Ellipse(target_hdc, marker.pos.x, marker.pos.y, marker.pos.right(), marker.pos.bottom());
    } else if (marker.marker_type == litehtml::list_style_type_square) {
        Rectangle(target_hdc, marker.pos.x, marker.pos.y, marker.pos.right(), marker.pos.bottom());
    }
    
    SelectObject(target_hdc, old_brush);
    DeleteObject(brush);
}

void LitehtmlContainer::load_image(const char* src, const char* baseurl, bool redraw_on_ready) {}

void LitehtmlContainer::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) {
    sz.width = 0; sz.height = 0;
    if (src && image_cache_) {
        const epub::ImageData* img = image_cache_->getImage(src);
        if (img) { sz.width = img->width; sz.height = img->height; }
    }
}

void LitehtmlContainer::draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) {
    if (!image_cache_ || url.empty()) return;
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) return;
    
    const epub::ImageData* img = image_cache_->getImage(url.c_str());
    if (!img) return;
    
    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(img->width, img->height, img->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB);
    if (!bitmap) return;
    
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, img->width, img->height);
    bitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, bitmap->GetPixelFormat(), &bitmapData);
    
    // Копирование пикселей (упрощено, предполагаем совпадение форматов)
    // В реальном коде тут нужна аккуратная обработка stride
    memcpy(bitmapData.Scan0, img->pixels.data(), img->pixels.size());
    
    bitmap->UnlockBits(&bitmapData);
    Gdiplus::Graphics graphics(target_hdc);
    graphics.DrawImage(bitmap, layer.clip_box.x, layer.clip_box.y, layer.clip_box.width, layer.clip_box.height);
    delete bitmap;
}

void LitehtmlContainer::draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc || color.alpha == 0) return;
    RECT rect = { layer.clip_box.x, layer.clip_box.y, layer.clip_box.right(), layer.clip_box.bottom() };
    HBRUSH brush = CreateSolidBrush(web_color_to_colorref(color));
    FillRect(target_hdc, &rect, brush);
    DeleteObject(brush);
}

void LitehtmlContainer::draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) {}
void LitehtmlContainer::draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) {}
void LitehtmlContainer::draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) {}

void LitehtmlContainer::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    if (!target_hdc) return;
    
    // Простая отрисовка границ (только сплошные)
    if (borders.left.width > 0) {
        HPEN pen = CreatePen(PS_SOLID, borders.left.width, web_color_to_colorref(borders.left.color));
        SelectObject(target_hdc, pen);
        MoveToEx(target_hdc, draw_pos.left(), draw_pos.top(), NULL);
        LineTo(target_hdc, draw_pos.left(), draw_pos.bottom());
        DeleteObject(pen);
    }
    // ... аналогично для других сторон
}

void LitehtmlContainer::set_caption(const char* caption) {}
void LitehtmlContainer::set_base_url(const char* base_url) {}
void LitehtmlContainer::link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) {}
void LitehtmlContainer::on_anchor_click(const char* url, const litehtml::element::ptr& el) {}
void LitehtmlContainer::on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) {}
void LitehtmlContainer::set_cursor(const char* cursor) {}

void LitehtmlContainer::transform_text(litehtml::string& text, litehtml::text_transform tt) {
    // Здесь должна быть реализация трансформации (upper, lower, capitalize)
}

void LitehtmlContainer::import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) {}

void LitehtmlContainer::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) {
    if (!hdc_) return;
    HRGN region = CreateRectRgn(pos.x, pos.y, pos.right(), pos.bottom());
    SelectClipRgn(hdc_, region);
    clip_regions_.push_back(region);
}

void LitehtmlContainer::del_clip() {
    if (!hdc_ || clip_regions_.empty()) return;
    DeleteObject(clip_regions_.back());
    clip_regions_.pop_back();
    if (clip_regions_.empty()) SelectClipRgn(hdc_, NULL);
    else SelectClipRgn(hdc_, clip_regions_.back());
}

void LitehtmlContainer::get_viewport(litehtml::position& viewport) const {
    viewport.x = 0; viewport.y = 0;
    viewport.width = viewport_width_;
    viewport.height = viewport_height_;
}

std::shared_ptr<litehtml::element> LitehtmlContainer::create_element(const char* tag_name,
                                                                      const litehtml::string_map& attributes,
                                                                      const std::shared_ptr<litehtml::document>& doc) {
    // Логируем вызов, чтобы убедиться, что ABI совпадает и мы дошли до этого момента
    // LOG_DEBUG("create_element called for tag:", tag_name ? tag_name : "null");
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
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string LitehtmlContainer::wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

COLORREF LitehtmlContainer::web_color_to_colorref(litehtml::web_color color) {
    return RGB(color.red, color.green, color.blue);
}