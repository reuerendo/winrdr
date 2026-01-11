#pragma once

#include "formatted_text.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

typedef struct lxb_css_memory lxb_css_memory_t;
typedef struct lxb_css_parser lxb_css_parser_t;
typedef struct lxb_css_stylesheet lxb_css_stylesheet_t;
typedef struct lxb_css_selector lxb_css_selector_t;
typedef struct lxb_selectors lxb_selectors_t;
typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;
typedef struct lxb_html_document lxb_html_document_t;

namespace epub {

enum class CSSDisplay {
    None,
    Block,
    Inline,
    InlineBlock,
    ListItem
};

enum class CSSVerticalAlign {
    Baseline,
    Sub,
    Super,
    Top,
    Middle,
    Bottom
};

enum class CSSTextTransform {
    None,
    Uppercase,
    Lowercase,
    Capitalize
};

enum class CSSWhiteSpace {
    Normal,
    Pre,
    PreWrap,
    PreLine,
    Nowrap
};

struct CSSComputedStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool monospace = false;
    bool small_caps = false;
    
    float font_size = 1.0f;
    float line_height = 1.2f;
    float letter_spacing = 0.0f;
    
    int margin_top = 0;
    int margin_bottom = 0;
    int margin_left = 0;
    int margin_right = 0;
    
    int padding_top = 0;
    int padding_bottom = 0;
    int padding_left = 0;
    int padding_right = 0;
    
    CSSDisplay display = CSSDisplay::Inline;
    CSSVerticalAlign vertical_align = CSSVerticalAlign::Baseline;
    TextAlign text_align = TextAlign::Left;
    CSSTextTransform text_transform = CSSTextTransform::None;
    CSSWhiteSpace white_space = CSSWhiteSpace::Normal;
    
    bool page_break_before = false;
    bool page_break_after = false;
    bool page_break_inside_avoid = false;
};

class CSSProcessor {
public:
    CSSProcessor();
    ~CSSProcessor();
    
    void clear();
    
    bool loadDefaultStyles(const std::string& css_file_path);
    
    bool parseStylesheet(const std::string& css);
    
    void setDocument(lxb_html_document_t* document);
    
    CSSComputedStyle computeStyle(lxb_dom_node_t* node);
    
    TextStyle convertToTextStyle(const CSSComputedStyle& css_style);
    TextAlign convertToTextAlign(const CSSComputedStyle& css_style);
    
    size_t getRulesCount() const;

private:
    struct PropertyValue {
        std::string value;
        int specificity;
    };
    
    void applyProperty(const std::string& name, const std::string& value, 
                      CSSComputedStyle& style);
    
    void applyFontWeight(const std::string& value, CSSComputedStyle& style);
    void applyFontStyle(const std::string& value, CSSComputedStyle& style);
    void applyTextDecoration(const std::string& value, CSSComputedStyle& style);
    void applyFontVariant(const std::string& value, CSSComputedStyle& style);
    void applyFontFamily(const std::string& value, CSSComputedStyle& style);
    void applyFontSize(const std::string& value, CSSComputedStyle& style);
    void applyLineHeight(const std::string& value, CSSComputedStyle& style);
    void applyLetterSpacing(const std::string& value, CSSComputedStyle& style);
    void applyTextAlign(const std::string& value, CSSComputedStyle& style);
    void applyDisplay(const std::string& value, CSSComputedStyle& style);
    void applyVerticalAlign(const std::string& value, CSSComputedStyle& style);
    void applyTextTransform(const std::string& value, CSSComputedStyle& style);
    void applyWhiteSpace(const std::string& value, CSSComputedStyle& style);
    void applyMargin(const std::string& property, const std::string& value, 
                     CSSComputedStyle& style);
    void applyPadding(const std::string& property, const std::string& value, 
                      CSSComputedStyle& style);
    void applyPageBreak(const std::string& property, const std::string& value, 
                       CSSComputedStyle& style);
    
    int parseLength(const std::string& value);
    float parseFloat(const std::string& value);
    
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
    
    int calculateSpecificity(lxb_css_selector_t* selector);
    
    lxb_css_memory_t* css_memory_;
    lxb_css_parser_t* css_parser_;
    lxb_selectors_t* selectors_;
    lxb_html_document_t* document_;
    
    std::vector<lxb_css_stylesheet_t*> stylesheets_;
    std::unordered_map<lxb_dom_element_t*, std::unordered_map<std::string, PropertyValue>> inline_styles_;
};

} // namespace epub