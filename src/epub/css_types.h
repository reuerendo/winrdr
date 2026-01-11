#pragma once

#include "formatted_text.h"
#include <string>
#include <unordered_map>

typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

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

struct PropertyValue {
    std::string value;
    int specificity;
};

struct RuleData {
    std::string selector;
    std::unordered_map<std::string, std::string> properties;
    int specificity;
};

} // namespace epub