#pragma once

#include <litehtml.h>
#include <windows.h>
#include <string>
#include <map>
#include <vector>
#include "../epub/image_cache.h"

class LitehtmlContainer : public litehtml::document_container {
public:
    LitehtmlContainer(HDC hdc, epub::ImageCache* image_cache);
    virtual ~LitehtmlContainer();
    
    void setHDC(HDC hdc) { hdc_ = hdc; }
    HDC getHDC() const { return hdc_; }
    
    // Required litehtml interface methods
    virtual litehtml::uint_ptr create_font(const litehtml::tchar_t* face_name, 
                                          int size, 
                                          int weight, 
                                          litehtml::font_style italic, 
                                          unsigned int decoration,
                                          litehtml::font_metrics* fm) override;
    
    virtual void delete_font(litehtml::uint_ptr hFont) override;
    
    virtual int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) override;
    
    virtual void draw_text(litehtml::uint_ptr hdc, 
                          const litehtml::tchar_t* text, 
                          litehtml::uint_ptr hFont, 
                          litehtml::web_color color, 
                          const litehtml::position& pos) override;
    
    virtual int pt_to_px(int pt) override;
    
    virtual int get_default_font_size() const override;
    
    virtual const litehtml::tchar_t* get_default_font_name() const override;
    
    virtual void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
    
    virtual void load_image(const litehtml::tchar_t* src, 
                           const litehtml::tchar_t* baseurl, 
                           bool redraw_on_ready) override;
    
    virtual void get_image_size(const litehtml::tchar_t* src, 
                               const litehtml::tchar_t* baseurl, 
                               litehtml::size& sz) override;
    
    virtual void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) override;
    
    virtual void draw_borders(litehtml::uint_ptr hdc, 
                             const litehtml::borders& borders, 
                             const litehtml::position& draw_pos, 
                             bool root) override;
    
    virtual void set_caption(const litehtml::tchar_t* caption) override;
    
    virtual void set_base_url(const litehtml::tchar_t* base_url) override;
    
    virtual void link(const std::shared_ptr<litehtml::document>& doc, 
                     const litehtml::element::ptr& el) override;
    
    virtual void on_anchor_click(const litehtml::tchar_t* url, 
                                const litehtml::element::ptr& el) override;
    
    virtual void set_cursor(const litehtml::tchar_t* cursor) override;
    
    virtual void transform_text(litehtml::tstring& text, litehtml::text_transform tt) override;
    
    virtual void import_css(litehtml::tstring& text, 
                           const litehtml::tstring& url, 
                           litehtml::tstring& baseurl) override;
    
    virtual void set_clip(const litehtml::position& pos, 
                         const litehtml::border_radiuses& bdr_radius, 
                         bool valid_x, 
                         bool valid_y) override;
    
    virtual void del_clip() override;
    
    virtual void get_client_rect(litehtml::position& client) const override;
    
    virtual std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name,
                                                              const litehtml::string_map& attributes,
                                                              const std::shared_ptr<litehtml::document>& doc) override;
    
    virtual void get_media_features(litehtml::media_features& media) const override;
    
    virtual void get_language(litehtml::tstring& language, litehtml::tstring& culture) const override;
    
    virtual litehtml::tstring resolve_color(const litehtml::tstring& color) const override;

private:
    struct FontInfo {
        HFONT hfont;
        int size;
        int weight;
        bool italic;
        unsigned int decoration;
        std::wstring face_name;
    };
    
    HDC hdc_;
    epub::ImageCache* image_cache_;
    std::map<litehtml::uint_ptr, FontInfo> fonts_;
    litehtml::uint_ptr next_font_id_;
    
    std::vector<HRGN> clip_regions_;
    
    int viewport_width_;
    int viewport_height_;
    
    std::wstring utf8_to_wstring(const std::string& str);
    std::string wstring_to_utf8(const std::wstring& wstr);
    COLORREF web_color_to_colorref(litehtml::web_color color);
    void apply_font_decoration(HDC hdc, const FontInfo& font, const litehtml::position& pos, const std::wstring& text);
};