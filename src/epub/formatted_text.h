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
    float text_indent;  // In em units
    
    // NEW: Style properties passed explicitly to avoid LayoutEngine hacks
    float margin_top = 0.0f;    // In pixels
    float margin_bottom = 0.0f; // In pixels
    std::string font_family;
    
    // NEW: Flags for inline rendering flow
    bool is_inline_continuation = false; // Draws on the same line as previous element
    
    TextElement() 
        : type(ElementType::Text)
        , style(TextStyle::Normal)
        , align(TextAlign::Left)
        , list_level(0)
        , text_indent(0.0f)
    {}
};

using FormattedContent = std::vector<TextElement>;

} // namespace epub