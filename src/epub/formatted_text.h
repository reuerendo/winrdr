#pragma once

#include <string>
#include <vector>

namespace epub {

enum class TextStyle {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Underline = 1 << 2,
    Strikethrough = 1 << 3
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
    LineBreak
};

struct TextElement {
    ElementType type;
    std::wstring content;
    TextStyle style;
    TextAlign align;
    std::string image_id;
    int list_level;
    
    TextElement() 
        : type(ElementType::Text)
        , style(TextStyle::Normal)
        , align(TextAlign::Left)
        , list_level(0) 
    {}
};

using FormattedContent = std::vector<TextElement>;

} // namespace epub