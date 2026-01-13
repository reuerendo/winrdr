#include "css_property_applier.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace epub {

// Property name mapping (based on crengine's css_decl_code)
enum CSSPropertyCode {
    cssd_unknown,
    cssd_display,
    cssd_white_space,
    cssd_text_align,
    cssd_text_align_last,
    cssd_text_decoration,
    cssd_text_transform,
    cssd_color,
    cssd_background_color,
    cssd_vertical_align,
    cssd_font_family,
    cssd_font_names,
    cssd_font_size,
    cssd_font_style,
    cssd_font_weight,
    cssd_text_indent,
    cssd_line_height,
    cssd_letter_spacing,
    cssd_width,
    cssd_height,
    cssd_margin_left,
    cssd_margin_right,
    cssd_margin_top,
    cssd_margin_bottom,
    cssd_margin,
    cssd_padding_left,
    cssd_padding_right,
    cssd_padding_top,
    cssd_padding_bottom,
    cssd_padding,
    cssd_page_break_before,
    cssd_page_break_after,
    cssd_page_break_inside
};

static const char* css_d_names[] = {
    "",
    "ruby",
    "run-in",
    "inline",
    "block",
    "list-item",
    "inline-block",
    "inline-table",
    "table",
    "table-row-group",
    "table-header-group",
    "table-footer-group",
    "table-row",
    "table-column-group",
    "table-column",
    "table-cell",
    "table-caption",
    "none",
    nullptr
};

static const char* css_ws_names[] = {
    "",
    "normal",
    "nowrap",
    "pre-line",
    "pre",
    "pre-wrap",
    nullptr
};

static const char* css_ta_names[] = {
    "",
    "left",
    "right",
    "center",
    "justify",
    "start",
    "end",
    nullptr
};

static const char* css_td_names[] = {
    "",
    "none",
    "underline",
    "overline",
    "line-through",
    "blink",
    nullptr
};

static const char* css_tt_names[] = {
    "",
    "none",
    "uppercase",
    "lowercase",
    "capitalize",
    "full-width",
    nullptr
};

static const char* css_va_names[] = {
    "",
    "baseline",
    "sub",
    "super",
    "top",
    "text-top",
    "middle",
    "bottom",
    "text-bottom",
    nullptr
};

static const char* css_fs_names[] = {
    "",
    "normal",
    "italic",
    "oblique",
    nullptr
};

static const char* css_fw_names[] = {
    "",
    "normal",
    "bold",
    "bolder",
    "lighter",
    "100",
    "200",
    "300",
    "400",
    "500",
    "600",
    "700",
    "800",
    "900",
    nullptr
};

static const char* css_ff_names[] = {
    "",
    "serif",
    "sans-serif",
    "cursive",
    "fantasy",
    "monospace",
    nullptr
};

static const char* css_pb_names[] = {
    "",
    "auto",
    "avoid",
    "always",
    "left",
    "right",
    "page",
    nullptr
};

CSSPropertyCode parsePropertyName(const std::string& name) {
    std::string prop = toLowerCase(trim(name));
    
    if (prop == "display") return cssd_display;
    if (prop == "white-space") return cssd_white_space;
    if (prop == "text-align") return cssd_text_align;
    if (prop == "text-align-last") return cssd_text_align_last;
    if (prop == "text-decoration") return cssd_text_decoration;
    if (prop == "text-transform") return cssd_text_transform;
    if (prop == "color") return cssd_color;
    if (prop == "background-color") return cssd_background_color;
    if (prop == "vertical-align") return cssd_vertical_align;
    if (prop == "font-family") return cssd_font_family;
    if (prop == "font-size") return cssd_font_size;
    if (prop == "font-style") return cssd_font_style;
    if (prop == "font-weight") return cssd_font_weight;
    if (prop == "text-indent") return cssd_text_indent;
    if (prop == "line-height") return cssd_line_height;
    if (prop == "letter-spacing") return cssd_letter_spacing;
    if (prop == "width") return cssd_width;
    if (prop == "height") return cssd_height;
    if (prop == "margin-left") return cssd_margin_left;
    if (prop == "margin-right") return cssd_margin_right;
    if (prop == "margin-top") return cssd_margin_top;
    if (prop == "margin-bottom") return cssd_margin_bottom;
    if (prop == "margin") return cssd_margin;
    if (prop == "padding-left") return cssd_padding_left;
    if (prop == "padding-right") return cssd_padding_right;
    if (prop == "padding-top") return cssd_padding_top;
    if (prop == "padding-bottom") return cssd_padding_bottom;
    if (prop == "padding") return cssd_padding;
    if (prop == "page-break-before" || prop == "break-before") return cssd_page_break_before;
    if (prop == "page-break-after" || prop == "break-after") return cssd_page_break_after;
    if (prop == "page-break-inside" || prop == "break-inside") return cssd_page_break_inside;
    
    return cssd_unknown;
}

int parseName(const std::string& value, const char** names) {
    std::string val = toLowerCase(trim(value));
    
    for (int i = 1; names[i] != nullptr; i++) {
        if (val == names[i]) {
            return i;
        }
    }
    
    return -1;
}

CSSLength parseNumberValue(const std::string& value) {
    std::string val = toLowerCase(trim(value));
    
    if (val.empty()) {
        return CSSLength::unspecified(CSSGenericValue::Auto);
    }
    
    if (val == "auto") {
        return CSSLength::unspecified(CSSGenericValue::Auto);
    }
    if (val == "none") {
        return CSSLength::unspecified(CSSGenericValue::None);
    }
    if (val == "normal") {
        return CSSLength::unspecified(CSSGenericValue::Normal);
    }
    if (val == "inherit") {
        return CSSLength::inherited();
    }
    
    // Named font sizes
    if (val == "xx-small") return CSSLength::rem(0.6f);
    if (val == "x-small") return CSSLength::rem(0.75f);
    if (val == "small") return CSSLength::rem(0.89f);
    if (val == "medium") return CSSLength::rem(1.0f);
    if (val == "large") return CSSLength::rem(1.2f);
    if (val == "x-large") return CSSLength::rem(1.5f);
    if (val == "xx-large") return CSSLength::rem(2.0f);
    if (val == "xxx-large") return CSSLength::rem(3.0f);
    if (val == "smaller") return CSSLength::percent(80);
    if (val == "larger") return CSSLength::percent(125);
    
    float parsed_value = 0.0f;
    size_t pos = 0;
    
    try {
        while (pos < val.length() && 
               (std::isdigit(val[pos]) || val[pos] == '.' || val[pos] == '-')) {
            pos++;
        }
        if (pos > 0) {
            parsed_value = std::stof(val.substr(0, pos));
        }
    } catch (...) {
        return CSSLength::px(0);
    }
    
    std::string unit = val.substr(pos);
    
    if (unit == "em") {
        return CSSLength::em(parsed_value);
    }
    if (unit == "rem") {
        return CSSLength::rem(parsed_value);
    }
    if (unit == "ex") {
        return CSSLength(CSSValueType::EX, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "px") {
        return CSSLength::px(static_cast<int>(parsed_value));
    }
    if (unit == "pt") {
        return CSSLength(CSSValueType::PT, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "pc") {
        return CSSLength(CSSValueType::PC, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "in") {
        return CSSLength(CSSValueType::IN, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "cm") {
        return CSSLength(CSSValueType::CM, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "mm") {
        return CSSLength(CSSValueType::MM, static_cast<int>(parsed_value * 256.0f));
    }
    if (unit == "%") {
        return CSSLength::percent(static_cast<int>(parsed_value));
    }
    
    if (unit.empty() && parsed_value == 0.0f) {
        return CSSLength::px(0);
    }
    
    return CSSLength(CSSValueType::Unspecified, static_cast<int>(parsed_value * 256.0f));
}

CSSLength parseColorValue(const std::string& value) {
    std::string val = toLowerCase(trim(value));
    
    if (val.empty()) {
        return CSSLength::color(0x000000);
    }
    
    if (val == "transparent") {
        return CSSLength::color(0xFFFFFF00);
    }
    if (val == "currentcolor") {
        return CSSLength::unspecified(CSSGenericValue::CurrentColor);
    }
    if (val == "inherit") {
        return CSSLength::inherited();
    }
    
    if (val[0] == '#') {
        std::string hex = val.substr(1);
        uint32_t color = 0;
        
        if (hex.length() == 3) {
            int r = std::stoi(hex.substr(0, 1), nullptr, 16);
            int g = std::stoi(hex.substr(1, 1), nullptr, 16);
            int b = std::stoi(hex.substr(2, 1), nullptr, 16);
            color = ((r * 17) << 16) | ((g * 17) << 8) | (b * 17);
        } else if (hex.length() == 6) {
            color = std::stoi(hex, nullptr, 16);
        }
        
        return CSSLength::color(color);
    }
    
    if (val.find("rgb(") == 0 || val.find("rgba(") == 0) {
        size_t start = val.find('(') + 1;
        size_t end = val.find(')');
        if (end != std::string::npos) {
            std::string rgb_vals = val.substr(start, end - start);
            
            int r = 0, g = 0, b = 0;
            size_t pos = 0;
            int count = 0;
            std::string current;
            
            for (size_t i = 0; i <= rgb_vals.length(); i++) {
                if (i == rgb_vals.length() || rgb_vals[i] == ',') {
                    if (!current.empty()) {
                        int value = std::stoi(trim(current));
                        if (count == 0) r = value;
                        else if (count == 1) g = value;
                        else if (count == 2) b = value;
                        count++;
                        current.clear();
                    }
                } else if (rgb_vals[i] != ' ') {
                    current += rgb_vals[i];
                }
            }
            
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            
            uint32_t color = (r << 16) | (g << 8) | b;
            return CSSLength::color(color);
        }
    }
    
    struct NamedColor { const char* name; uint32_t color; };
    static const NamedColor named_colors[] = {
        {"black", 0x000000}, {"white", 0xFFFFFF}, {"red", 0xFF0000},
        {"green", 0x008000}, {"blue", 0x0000FF}, {"yellow", 0xFFFF00},
        {"cyan", 0x00FFFF}, {"magenta", 0xFF00FF}, {"gray", 0x808080},
        {"silver", 0xC0C0C0}, {"maroon", 0x800000}, {"olive", 0x808000},
        {"lime", 0x00FF00}, {"aqua", 0x00FFFF}, {"teal", 0x008080},
        {"navy", 0x000080}, {"fuchsia", 0xFF00FF}, {"purple", 0x800080},
        {nullptr, 0}
    };
    
    for (int i = 0; named_colors[i].name != nullptr; i++) {
        if (val == named_colors[i].name) {
            return CSSLength::color(named_colors[i].color);
        }
    }
    
    return CSSLength::color(0x000000);
}

void CSSPropertyApplier::applyProperty(const std::string& name, const std::string& value,
                                       CSSComputedStyle& style, uint8_t importance) {
    CSSPropertyCode prop_code = parsePropertyName(name);
    
    if (prop_code == cssd_unknown) {
        return;
    }
    
    std::string val = toLowerCase(trim(value));
    
    int n = -1;
    CSSLength len;
    
    switch (prop_code) {
        case cssd_display:
            n = parseName(val, css_d_names);
            if (n > 0) {
                style.apply(static_cast<CSSDisplay>(n), &style.display, 
                           imp_bit_display, importance);
            }
            break;
            
        case cssd_white_space:
            n = parseName(val, css_ws_names);
            if (n > 0) {
                style.apply(static_cast<CSSWhiteSpace>(n), &style.white_space,
                           imp_bit_white_space, importance);
            }
            break;
            
        case cssd_text_align:
            n = parseName(val, css_ta_names);
            if (n > 0) {
                style.apply(static_cast<CSSTextAlign>(n), &style.text_align,
                           imp_bit_text_align, importance);
            }
            break;
            
        case cssd_text_align_last:
            n = parseName(val, css_ta_names);
            if (n > 0) {
                style.apply(static_cast<CSSTextAlign>(n), &style.text_align_last,
                           imp_bit_text_align_last, importance);
            }
            break;
            
        case cssd_text_decoration:
            n = parseName(val, css_td_names);
            if (n > 0) {
                style.apply(static_cast<CSSTextDecoration>(n), &style.text_decoration,
                           imp_bit_text_decoration, importance);
            }
            break;
            
        case cssd_text_transform:
            n = parseName(val, css_tt_names);
            if (n > 0) {
                style.apply(static_cast<CSSTextTransform>(n), &style.text_transform,
                           imp_bit_text_transform, importance);
            }
            break;
            
        case cssd_vertical_align:
            n = parseName(val, css_va_names);
            if (n > 0) {
                style.apply(static_cast<CSSVerticalAlign>(n), &style.vertical_align,
                           imp_bit_vertical_align, importance);
            } else {
                len = parseNumberValue(val);
                style.apply(len, &style.vertical_align, imp_bit_vertical_align, importance);
            }
            break;
            
        case cssd_font_style:
            n = parseName(val, css_fs_names);
            if (n > 0) {
                style.apply(static_cast<CSSFontStyle>(n), &style.font_style,
                           imp_bit_font_style, importance);
            }
            break;
            
        case cssd_font_weight:
            n = parseName(val, css_fw_names);
            if (n > 0) {
                style.apply(static_cast<CSSFontWeight>(n), &style.font_weight,
                           imp_bit_font_weight, importance);
            }
            break;
            
        case cssd_font_family:
            n = parseName(val, css_ff_names);
            if (n > 0) {
                style.apply(static_cast<CSSFontFamily>(n), &style.font_family,
                           imp_bit_font_family, importance);
            } else {
                style.font_name = val;
            }
            break;
            
        case cssd_font_size:
            len = parseNumberValue(val);
            style.apply(len, &style.font_size, imp_bit_font_size, importance);
            break;
            
        case cssd_line_height:
            len = parseNumberValue(val);
            style.apply(len, &style.line_height, imp_bit_line_height, importance);
            break;
            
        case cssd_letter_spacing:
            len = parseNumberValue(val);
            style.apply(len, &style.letter_spacing, imp_bit_letter_spacing, importance);
            break;
            
        case cssd_color:
            len = parseColorValue(val);
            style.apply(len, &style.color, imp_bit_color, importance);
            break;
            
        case cssd_background_color:
            len = parseColorValue(val);
            style.apply(len, &style.background_color, imp_bit_background_color, importance);
            break;
            
        case cssd_text_indent:
            len = parseNumberValue(val);
            style.apply(len, &style.text_indent, imp_bit_text_indent, importance);
            break;
            
        case cssd_width:
            len = parseNumberValue(val);
            style.apply(len, &style.width, imp_bit_width, importance);
            break;
            
        case cssd_height:
            len = parseNumberValue(val);
            style.apply(len, &style.height, imp_bit_height, importance);
            break;
            
        case cssd_margin_left:
            len = parseNumberValue(val);
            style.apply(len, &style.margin[3], imp_bit_margin_left, importance);
            break;
            
        case cssd_margin_right:
            len = parseNumberValue(val);
            style.apply(len, &style.margin[1], imp_bit_margin_right, importance);
            break;
            
        case cssd_margin_top:
            len = parseNumberValue(val);
            style.apply(len, &style.margin[0], imp_bit_margin_top, importance);
            break;
            
        case cssd_margin_bottom:
            len = parseNumberValue(val);
            style.apply(len, &style.margin[2], imp_bit_margin_bottom, importance);
            break;
            
        case cssd_margin: {
            std::vector<CSSLength> values = parseShorthand(val);
            if (values.size() == 1) {
                style.apply(values[0], &style.margin[0], imp_bit_margin_top, importance);
                style.apply(values[0], &style.margin[1], imp_bit_margin_right, importance);
                style.apply(values[0], &style.margin[2], imp_bit_margin_bottom, importance);
                style.apply(values[0], &style.margin[3], imp_bit_margin_left, importance);
            } else if (values.size() == 2) {
                style.apply(values[0], &style.margin[0], imp_bit_margin_top, importance);
                style.apply(values[1], &style.margin[1], imp_bit_margin_right, importance);
                style.apply(values[0], &style.margin[2], imp_bit_margin_bottom, importance);
                style.apply(values[1], &style.margin[3], imp_bit_margin_left, importance);
            } else if (values.size() == 3) {
                style.apply(values[0], &style.margin[0], imp_bit_margin_top, importance);
                style.apply(values[1], &style.margin[1], imp_bit_margin_right, importance);
                style.apply(values[2], &style.margin[2], imp_bit_margin_bottom, importance);
                style.apply(values[1], &style.margin[3], imp_bit_margin_left, importance);
            } else if (values.size() == 4) {
                style.apply(values[0], &style.margin[0], imp_bit_margin_top, importance);
                style.apply(values[1], &style.margin[1], imp_bit_margin_right, importance);
                style.apply(values[2], &style.margin[2], imp_bit_margin_bottom, importance);
                style.apply(values[3], &style.margin[3], imp_bit_margin_left, importance);
            }
            break;
        }
            
        case cssd_padding_left:
            len = parseNumberValue(val);
            style.apply(len, &style.padding[3], imp_bit_padding_left, importance);
            break;
            
        case cssd_padding_right:
            len = parseNumberValue(val);
            style.apply(len, &style.padding[1], imp_bit_padding_right, importance);
            break;
            
        case cssd_padding_top:
            len = parseNumberValue(val);
            style.apply(len, &style.padding[0], imp_bit_padding_top, importance);
            break;
            
        case cssd_padding_bottom:
            len = parseNumberValue(val);
            style.apply(len, &style.padding[2], imp_bit_padding_bottom, importance);
            break;
            
        case cssd_padding: {
            std::vector<CSSLength> values = parseShorthand(val);
            if (values.size() == 1) {
                style.apply(values[0], &style.padding[0], imp_bit_padding_top, importance);
                style.apply(values[0], &style.padding[1], imp_bit_padding_right, importance);
                style.apply(values[0], &style.padding[2], imp_bit_padding_bottom, importance);
                style.apply(values[0], &style.padding[3], imp_bit_padding_left, importance);
            } else if (values.size() == 2) {
                style.apply(values[0], &style.padding[0], imp_bit_padding_top, importance);
                style.apply(values[1], &style.padding[1], imp_bit_padding_right, importance);
                style.apply(values[0], &style.padding[2], imp_bit_padding_bottom, importance);
                style.apply(values[1], &style.padding[3], imp_bit_padding_left, importance);
            } else if (values.size() == 3) {
                style.apply(values[0], &style.padding[0], imp_bit_padding_top, importance);
                style.apply(values[1], &style.padding[1], imp_bit_padding_right, importance);
                style.apply(values[2], &style.padding[2], imp_bit_padding_bottom, importance);
                style.apply(values[1], &style.padding[3], imp_bit_padding_left, importance);
            } else if (values.size() == 4) {
                style.apply(values[0], &style.padding[0], imp_bit_padding_top, importance);
                style.apply(values[1], &style.padding[1], imp_bit_padding_right, importance);
                style.apply(values[2], &style.padding[2], imp_bit_padding_bottom, importance);
                style.apply(values[3], &style.padding[3], imp_bit_padding_left, importance);
            }
            break;
        }
            
        case cssd_page_break_before:
            n = parseName(val, css_pb_names);
            if (n > 0) {
                style.apply(static_cast<CSSPageBreak>(n), &style.page_break_before,
                           imp_bit_page_break_before, importance);
            }
            break;
            
        case cssd_page_break_after:
            n = parseName(val, css_pb_names);
            if (n > 0) {
                style.apply(static_cast<CSSPageBreak>(n), &style.page_break_after,
                           imp_bit_page_break_after, importance);
            }
            break;
            
        case cssd_page_break_inside:
            n = parseName(val, css_pb_names);
            if (n > 0 && n <= 2) {
                style.apply(static_cast<CSSPageBreak>(n), &style.page_break_inside,
                           imp_bit_page_break_inside, importance);
            }
            break;
            
        default:
            break;
    }
}

std::vector<CSSLength> CSSPropertyApplier::parseShorthand(const std::string& value) {
    std::vector<CSSLength> result;
    std::string current;
    
    for (size_t i = 0; i < value.length(); i++) {
        if (value[i] == ' ' || value[i] == '\t') {
            if (!current.empty()) {
                result.push_back(parseNumberValue(current));
                current.clear();
            }
        } else {
            current += value[i];
        }
    }
    
    if (!current.empty()) {
        result.push_back(parseNumberValue(current));
    }
    
    return result;
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