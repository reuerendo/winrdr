#include "css_property_applier.h"
#include <algorithm>
#include <cctype>

namespace epub {

void CSSPropertyApplier::applyProperty(const std::string& name, const std::string& value, 
                                       CSSComputedStyle& style) {
    std::string prop_name = toLowerCase(trim(name));
    std::string prop_value = toLowerCase(trim(value));
    
    if (prop_name == "font-weight") {
        applyFontWeight(prop_value, style);
    } else if (prop_name == "font-style") {
        applyFontStyle(prop_value, style);
    } else if (prop_name == "text-decoration" || prop_name == "text-decoration-line") {
        applyTextDecoration(prop_value, style);
    } else if (prop_name == "font-variant" || prop_name == "font-variant-caps") {
        applyFontVariant(prop_value, style);
    } else if (prop_name == "font-family") {
        applyFontFamily(prop_value, style);
    } else if (prop_name == "font-size") {
        applyFontSize(prop_value, style);
    } else if (prop_name == "line-height") {
        applyLineHeight(prop_value, style);
    } else if (prop_name == "letter-spacing") {
        applyLetterSpacing(prop_value, style);
    } else if (prop_name == "text-align" || prop_name == "text-align-last") {
        applyTextAlign(prop_value, style);
    } else if (prop_name == "display") {
        applyDisplay(prop_value, style);
    } else if (prop_name == "vertical-align" || prop_name == "font-variant-position") {
        applyVerticalAlign(prop_value, style);
    } else if (prop_name == "text-transform") {
        applyTextTransform(prop_value, style);
    } else if (prop_name == "white-space") {
        applyWhiteSpace(prop_value, style);
    }
}

void CSSPropertyApplier::applyFontWeight(const std::string& value, CSSComputedStyle& style) {
    if (value.find("bold") != std::string::npos || 
        value.find("700") != std::string::npos ||
        value.find("800") != std::string::npos ||
        value.find("900") != std::string::npos) {
        style.bold = true;
    }
}

void CSSPropertyApplier::applyFontStyle(const std::string& value, CSSComputedStyle& style) {
    if (value.find("italic") != std::string::npos || 
        value.find("oblique") != std::string::npos) {
        style.italic = true;
    }
}

void CSSPropertyApplier::applyTextDecoration(const std::string& value, CSSComputedStyle& style) {
    if (value.find("underline") != std::string::npos) {
        style.underline = true;
    }
    if (value.find("line-through") != std::string::npos) {
        style.strikethrough = true;
    }
}

void CSSPropertyApplier::applyFontVariant(const std::string& value, CSSComputedStyle& style) {
    if (value.find("small-caps") != std::string::npos || 
        value.find("all-small-caps") != std::string::npos) {
        style.small_caps = true;
    }
}

void CSSPropertyApplier::applyFontFamily(const std::string& value, CSSComputedStyle& style) {
    if (value.find("mono") != std::string::npos || 
        value.find("courier") != std::string::npos ||
        value.find("consolas") != std::string::npos) {
        style.monospace = true;
    }
}

void CSSPropertyApplier::applyFontSize(const std::string& value, CSSComputedStyle& style) {
    if (value.find("xxx-large") != std::string::npos) {
        style.font_size = 2.5f;
    } else if (value == "xx-large") {
        style.font_size = 2.0f;
    } else if (value == "x-large") {
        style.font_size = 1.5f;
    } else if (value == "large" || value == "larger") {
        style.font_size = 1.2f;
    } else if (value == "medium") {
        style.font_size = 1.0f;
    } else if (value == "small" || value == "smaller" || value == "x-small") {
        style.font_size = 0.85f;
    } else if (value == "xx-small") {
        style.font_size = 0.6f;
    } else {
        float parsed = parseFloat(value);
        if (parsed > 0.0f) {
            if (value.find("em") != std::string::npos) {
                style.font_size = parsed;
            } else if (value.find("%") != std::string::npos) {
                style.font_size = parsed / 100.0f;
            } else if (value.find("pt") != std::string::npos) {
                style.font_size = parsed / 12.0f;
            } else if (value.find("px") != std::string::npos) {
                style.font_size = parsed / 16.0f;
            } else if (value.find("rem") != std::string::npos) {
                style.font_size = parsed;
            }
        }
    }
}

void CSSPropertyApplier::applyLineHeight(const std::string& value, CSSComputedStyle& style) {
    float parsed = parseFloat(value);
    if (parsed > 0.0f) {
        style.line_height = parsed;
    }
}

void CSSPropertyApplier::applyLetterSpacing(const std::string& value, CSSComputedStyle& style) {
    float parsed = parseFloat(value);
    style.letter_spacing = parsed;
}

void CSSPropertyApplier::applyTextAlign(const std::string& value, CSSComputedStyle& style) {
    if (value.find("center") != std::string::npos) {
        style.text_align = TextAlign::Center;
    } else if (value.find("right") != std::string::npos) {
        style.text_align = TextAlign::Right;
    } else if (value.find("justify") != std::string::npos) {
        style.text_align = TextAlign::Justify;
    } else if (value.find("left") != std::string::npos) {
        style.text_align = TextAlign::Left;
    }
}

void CSSPropertyApplier::applyDisplay(const std::string& value, CSSComputedStyle& style) {
    if (value.find("none") != std::string::npos) {
        style.display = CSSDisplay::None;
    } else if (value.find("block") != std::string::npos) {
        style.display = CSSDisplay::Block;
    } else if (value.find("inline-block") != std::string::npos) {
        style.display = CSSDisplay::InlineBlock;
    } else if (value.find("list-item") != std::string::npos) {
        style.display = CSSDisplay::ListItem;
    } else if (value.find("inline") != std::string::npos) {
        style.display = CSSDisplay::Inline;
    }
}

void CSSPropertyApplier::applyVerticalAlign(const std::string& value, CSSComputedStyle& style) {
    if (value.find("super") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Super;
    } else if (value.find("sub") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Sub;
    } else if (value.find("top") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Top;
    } else if (value.find("middle") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Middle;
    } else if (value.find("bottom") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Bottom;
    } else {
        style.vertical_align = CSSVerticalAlign::Baseline;
    }
}

void CSSPropertyApplier::applyTextTransform(const std::string& value, CSSComputedStyle& style) {
    if (value.find("uppercase") != std::string::npos) {
        style.text_transform = CSSTextTransform::Uppercase;
    } else if (value.find("lowercase") != std::string::npos) {
        style.text_transform = CSSTextTransform::Lowercase;
    } else if (value.find("capitalize") != std::string::npos) {
        style.text_transform = CSSTextTransform::Capitalize;
    }
}

void CSSPropertyApplier::applyWhiteSpace(const std::string& value, CSSComputedStyle& style) {
    if (value.find("pre-wrap") != std::string::npos) {
        style.white_space = CSSWhiteSpace::PreWrap;
    } else if (value.find("pre-line") != std::string::npos) {
        style.white_space = CSSWhiteSpace::PreLine;
    } else if (value.find("pre") != std::string::npos) {
        style.white_space = CSSWhiteSpace::Pre;
    } else if (value.find("nowrap") != std::string::npos) {
        style.white_space = CSSWhiteSpace::Nowrap;
    }
}

int CSSPropertyApplier::parseLength(const std::string& value) {
    float parsed = parseFloat(value);
    
    if (value.find("rem") != std::string::npos) {
        return static_cast<int>(parsed * 16.0f);
    } else if (value.find("em") != std::string::npos) {
        return static_cast<int>(parsed * 16.0f);
    } else if (value.find("px") != std::string::npos) {
        return static_cast<int>(parsed);
    } else if (value.find("pt") != std::string::npos) {
        return static_cast<int>(parsed * 1.333f);
    }
    
    return static_cast<int>(parsed);
}

float CSSPropertyApplier::parseFloat(const std::string& value) {
    try {
        size_t pos = 0;
        while (pos < value.length() && 
               (std::isdigit(value[pos]) || value[pos] == '.' || value[pos] == '-')) {
            pos++;
        }
        if (pos > 0) {
            return std::stof(value.substr(0, pos));
        }
    } catch (...) {
    }
    return 0.0f;
}

std::string CSSPropertyApplier::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string CSSPropertyApplier::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace epub