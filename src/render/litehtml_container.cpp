#include "litehtml_container.h"
#include "../utils/logger.h"
#include <algorithm>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {
    const int DEFAULT_FONT_SIZE = 16;
    const wchar_t* DEFAULT_FONT_NAME = L"Arial";
    const int DPI = 96;
}

LitehtmlContainer::LitehtmlContainer(HDC hdc, epub::ImageCache* image_cache)
    : hdc_(hdc)
    , image_cache_(image_cache)
    , next_font_id_(1)
    , viewport_width_(600)
    , viewport_height_(800)
{
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

litehtml::uint_ptr LitehtmlContainer::create_font(const litehtml::tchar_t* face_name,
                                                   int size,
                                                   int weight,
                                                   litehtml::font_style italic,
                                                   unsigned int decoration,
                                                   litehtml::font_metrics* fm) {
    std::wstring font_face = utf8_to_wstring(face_name);
    
    if (font_face.empty()) {
        font_face = DEFAULT_FONT_NAME;
    }
    
    int font_weight = FW_NORMAL;
    if (weight >= 700) {
        font_weight = FW_BOLD;
    } else if (weight >= 600) {
        font_weight = FW_SEMIBOLD;
    } else if (weight >= 300) {
        font_weight = FW_NORMAL;
    } else {
        font_weight = FW_LIGHT;
    }
    
    BOOL is_italic = (italic == litehtml::fontStyleItalic) ? TRUE : FALSE;
    BOOL is_underline = (decoration & litehtml::font_decoration_underline) ? TRUE : FALSE;
    BOOL is_strikeout = (decoration & litehtml::font_decoration_linethrough) ? TRUE : FALSE;
    
    HFONT hfont = CreateFontW(
        -MulDiv(size, DPI, 72),
        0,
        0,
        0,
        font_weight,
        is_italic,
        is_underline,
        is_strikeout,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        font_face.c_str()
    );
    
    if (fm) {
        HFONT old_font = (HFONT)SelectObject(hdc_, hfont);
        
        TEXTMETRICW tm;
        GetTextMetricsW(hdc_, &tm);
        
        fm->height = tm.tmHeight;
        fm->ascent = tm.tmAscent;
        fm->descent = tm.tmDescent;
        fm->x_height = tm.tmHeight / 2;
        
        SelectObject(hdc_, old_font);
    }
    
    FontInfo info;
    info.hfont = hfont;
    info.size = size;
    info.weight = weight;
    info.italic = (italic == litehtml::fontStyleItalic);
    info.decoration = decoration;
    info.face_name = font_face;
    
    litehtml::uint_ptr font_id = next_font_id_++;
    fonts_[font_id] = info;
    
    return font_id;
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

int LitehtmlContainer::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) {
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) {
        return 0;
    }
    
    std::wstring wtext = utf8_to_wstring(text);
    
    HFONT old_font = (HFONT)SelectObject(hdc_, it->second.hfont);
    
    SIZE sz;
    GetTextExtentPoint32W(hdc_, wtext.c_str(), (int)wtext.length(), &sz);
    
    SelectObject(hdc_, old_font);
    
    return sz.cx;
}

void LitehtmlContainer::draw_text(litehtml::uint_ptr hdc,
                                 const litehtml::tchar_t* text,
                                 litehtml::uint_ptr hFont,
                                 litehtml::web_color color,
                                 const litehtml::position& pos) {
    auto it = fonts_.find(hFont);
    if (it == fonts_.end()) {
        return;
    }
    
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    
    std::wstring wtext = utf8_to_wstring(text);
    
    HFONT old_font = (HFONT)SelectObject(target_hdc, it->second.hfont);
    
    SetBkMode(target_hdc, TRANSPARENT);
    SetTextColor(target_hdc, web_color_to_colorref(color));
    
    TextOutW(target_hdc, pos.x, pos.y, wtext.c_str(), (int)wtext.length());
    
    apply_font_decoration(target_hdc, it->second, pos, wtext);
    
    SelectObject(target_hdc, old_font);
}

int LitehtmlContainer::pt_to_px(int pt) {
    return MulDiv(pt, DPI, 72);
}

int LitehtmlContainer::get_default_font_size() const {
    return DEFAULT_FONT_SIZE;
}

const litehtml::tchar_t* LitehtmlContainer::get_default_font_name() const {
    return "Arial";
}

void LitehtmlContainer::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    
    HBRUSH brush = CreateSolidBrush(web_color_to_colorref(marker.color));
    HBRUSH old_brush = (HBRUSH)SelectObject(target_hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, web_color_to_colorref(marker.color));
    HPEN old_pen = (HPEN)SelectObject(target_hdc, pen);
    
    switch (marker.marker_type) {
        case litehtml::list_style_type_circle:
            Ellipse(target_hdc, 
                   marker.pos.x, 
                   marker.pos.y, 
                   marker.pos.x + marker.pos.width, 
                   marker.pos.y + marker.pos.height);
            break;
            
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
            if (!marker.image.empty()) {
                // Image marker - not implemented
            } else if (marker.marker_type >= litehtml::list_style_type_decimal) {
                SetBkMode(target_hdc, TRANSPARENT);
                SetTextColor(target_hdc, web_color_to_colorref(marker.color));
                
                litehtml::tstring text = marker.image;
                std::wstring wtext = utf8_to_wstring(text.c_str());
                
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
}

void LitehtmlContainer::load_image(const litehtml::tchar_t* src,
                                   const litehtml::tchar_t* baseurl,
                                   bool redraw_on_ready) {
    // Images are already loaded by epub parser into image_cache_
}

void LitehtmlContainer::get_image_size(const litehtml::tchar_t* src,
                                      const litehtml::tchar_t* baseurl,
                                      litehtml::size& sz) {
    if (!image_cache_) {
        sz.width = 0;
        sz.height = 0;
        return;
    }
    
    const epub::ImageData* img = image_cache_->getImage(src);
    if (img) {
        sz.width = img->width;
        sz.height = img->height;
    } else {
        sz.width = 0;
        sz.height = 0;
    }
}

void LitehtmlContainer::draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    
    RECT rect;
    rect.left = bg.border_box.x;
    rect.top = bg.border_box.y;
    rect.right = bg.border_box.x + bg.border_box.width;
    rect.bottom = bg.border_box.y + bg.border_box.height;
    
    if (bg.color.alpha != 0) {
        HBRUSH brush = CreateSolidBrush(web_color_to_colorref(bg.color));
        FillRect(target_hdc, &rect, brush);
        DeleteObject(brush);
    }
    
    if (!bg.image.empty() && image_cache_) {
        const epub::ImageData* img = image_cache_->getImage(bg.image.c_str());
        if (img) {
            static bool gdiplus_initialized = false;
            static ULONG_PTR gdiplusToken;
            
            if (!gdiplus_initialized) {
                Gdiplus::GdiplusStartupInput gdiplusStartupInput;
                Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
                gdiplus_initialized = true;
            }
            
            Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(
                img->width,
                img->height,
                img->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB
            );
            
            if (bitmap) {
                Gdiplus::BitmapData bitmapData;
                Gdiplus::Rect bitmap_rect(0, 0, img->width, img->height);
                
                bitmap->LockBits(&bitmap_rect, Gdiplus::ImageLockModeWrite,
                                img->channels == 4 ? PixelFormat32bppARGB : PixelFormat24bppRGB,
                                &bitmapData);
                
                for (int y = 0; y < img->height; y++) {
                    unsigned char* dest = (unsigned char*)bitmapData.Scan0 + y * bitmapData.Stride;
                    const unsigned char* src = img->pixels.data() + y * img->width * img->channels;
                    
                    for (int x = 0; x < img->width; x++) {
                        if (img->channels == 4) {
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
                
                Gdiplus::Graphics graphics(target_hdc);
                graphics.DrawImage(bitmap,
                                  bg.image_box.x,
                                  bg.image_box.y,
                                  bg.image_box.width,
                                  bg.image_box.height);
                
                delete bitmap;
            }
        }
    }
}

void LitehtmlContainer::draw_borders(litehtml::uint_ptr hdc,
                                     const litehtml::borders& borders,
                                     const litehtml::position& draw_pos,
                                     bool root) {
    HDC target_hdc = hdc ? (HDC)hdc : hdc_;
    
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
}

void LitehtmlContainer::set_caption(const litehtml::tchar_t* caption) {
    // Not used in our implementation
}

void LitehtmlContainer::set_base_url(const litehtml::tchar_t* base_url) {
    // Base URL is handled by epub parser
}

void LitehtmlContainer::link(const std::shared_ptr<litehtml::document>& doc,
                             const litehtml::element::ptr& el) {
    // CSS linking - not used
}

void LitehtmlContainer::on_anchor_click(const litehtml::tchar_t* url,
                                       const litehtml::element::ptr& el) {
    // Link clicks - not implemented yet
}

void LitehtmlContainer::set_cursor(const litehtml::tchar_t* cursor) {
    // Cursor changes - not implemented
}

void LitehtmlContainer::transform_text(litehtml::tstring& text, litehtml::text_transform tt) {
    if (text.empty()) return;
    
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
}

void LitehtmlContainer::import_css(litehtml::tstring& text,
                                   const litehtml::tstring& url,
                                   litehtml::tstring& baseurl) {
    // CSS imports - not used
}

void LitehtmlContainer::set_clip(const litehtml::position& pos,
                                const litehtml::border_radiuses& bdr_radius,
                                bool valid_x,
                                bool valid_y) {
    HRGN region = CreateRectRgn(pos.x, pos.y, pos.x + pos.width, pos.y + pos.height);
    
    if (clip_regions_.empty()) {
        SelectClipRgn(hdc_, region);
    } else {
        ExtSelectClipRgn(hdc_, region, RGN_AND);
    }
    
    clip_regions_.push_back(region);
}

void LitehtmlContainer::del_clip() {
    if (!clip_regions_.empty()) {
        HRGN region = clip_regions_.back();
        DeleteObject(region);
        clip_regions_.pop_back();
        
        if (clip_regions_.empty()) {
            SelectClipRgn(hdc_, NULL);
        } else {
            SelectClipRgn(hdc_, clip_regions_.back());
        }
    }
}

void LitehtmlContainer::get_client_rect(litehtml::position& client) const {
    client.x = 0;
    client.y = 0;
    client.width = viewport_width_;
    client.height = viewport_height_;
}

std::shared_ptr<litehtml::element> LitehtmlContainer::create_element(const litehtml::tchar_t* tag_name,
                                                                      const litehtml::string_map& attributes,
                                                                      const std::shared_ptr<litehtml::document>& doc) {
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

void LitehtmlContainer::get_language(litehtml::tstring& language, litehtml::tstring& culture) const {
    language = "en";
    culture = "";
}

litehtml::tstring LitehtmlContainer::resolve_color(const litehtml::tstring& color) const {
    return color;
}

std::wstring LitehtmlContainer::utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string LitehtmlContainer::wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

COLORREF LitehtmlContainer::web_color_to_colorref(litehtml::web_color color) {
    return RGB(color.red, color.green, color.blue);
}

void LitehtmlContainer::apply_font_decoration(HDC hdc, const FontInfo& font, const litehtml::position& pos, const std::wstring& text) {
    SIZE sz;
    GetTextExtentPoint32W(hdc, text.c_str(), (int)text.length(), &sz);
    
    if (font.decoration & litehtml::font_decoration_underline) {
        HPEN pen = CreatePen(PS_SOLID, 1, GetTextColor(hdc));
        HPEN old_pen = (HPEN)SelectObject(hdc, pen);
        
        int underline_y = pos.y + sz.cy - 2;
        MoveToEx(hdc, pos.x, underline_y, NULL);
        LineTo(hdc, pos.x + sz.cx, underline_y);
        
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    
    if (font.decoration & litehtml::font_decoration_linethrough) {
        HPEN pen = CreatePen(PS_SOLID, 1, GetTextColor(hdc));
        HPEN old_pen = (HPEN)SelectObject(hdc, pen);
        
        int strikethrough_y = pos.y + sz.cy / 2;
        MoveToEx(hdc, pos.x, strikethrough_y, NULL);
        LineTo(hdc, pos.x + sz.cx, strikethrough_y);
        
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    
    if (font.decoration & litehtml::font_decoration_overline) {
        HPEN pen = CreatePen(PS_SOLID, 1, GetTextColor(hdc));
        HPEN old_pen = (HPEN)SelectObject(hdc, pen);
        
        int overline_y = pos.y + 2;
        MoveToEx(hdc, pos.x, overline_y, NULL);
        LineTo(hdc, pos.x + sz.cx, overline_y);
        
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
}