#pragma once

#include <string>
#include <vector>

namespace epub {

enum class TextStyle {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Underline = 1 << 2,
    Strikethrough = 1 << 3,
    Monospace = 1 << 4,
    Small = 1 << 5,
    Subscript = 1 << 6,
    Superscript = 1 << 7
};

inline TextStyle operator|(TextStyle a, TextStyle b) {
    return static_cast<TextStyle>(static_cast<int>(a) | static_cast<int>(b));
}

inline TextStyle operator&(TextStyle a, TextStyle b) {
    return static_cast<TextStyle>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool hasStyle(TextStyle style, TextStyle flag) {
    return (static_cast<int>(style) & static_cast<int>(flag)) != 0;
}

enum class TextAlign {
    Left,
    Center,
    Right,
    Justify
};

enum class ElementType {
    Text,
    Paragraph,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Heading5,
    Heading6,
    ListItem,
    Quote,
    Image,
    LineBreak,
    HorizontalRule,
    CodeBlock,
    Link
};

struct TextElement {
    ElementType type;
    std::wstring content;
    TextStyle style;
    TextAlign align;
    std::string image_id;
    std::string link_href;
    int list_level;
    
    // CSS spacing/indentation (in em units)
    float margin_top;
    float margin_bottom;
    float padding_left;
    float text_indent;      // First line indent
    bool has_css_spacing;   // Was CSS spacing applied?
    
    // CSS text properties
    float font_size_multiplier;  // Relative to base font size
    float line_height;           // Line spacing multiplier
    
    // CSS colors (RGB 0-255)
    unsigned char text_color_r;
    unsigned char text_color_g;
    unsigned char text_color_b;
    bool has_text_color;
    
    TextElement() 
        : type(ElementType::Text)
        , style(TextStyle::Normal)
        , align(TextAlign::Left)
        , list_level(0)
        , margin_top(0.0f)
        , margin_bottom(0.0f)
        , padding_left(0.0f)
        , text_indent(0.0f)
        , has_css_spacing(false)
        , font_size_multiplier(1.0f)
        , line_height(1.2f)
        , text_color_r(0)
        , text_color_g(0)
        , text_color_b(0)
        , has_text_color(false)
    {}
};

using FormattedContent = std::vector<TextElement>;

} // namespace epub