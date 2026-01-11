#include "css_box_model_applier.h"
#include <algorithm>
#include <cctype>

namespace epub {

void CSSBoxModelApplier::applyProperty(const std::string& property, const std::string& value, 
                                       CSSComputedStyle& style) {
    std::string prop_name = toLowerCase(trim(property));
    std::string prop_value = toLowerCase(trim(value));
    
    if (prop_name.find("margin") == 0) {
        applyMargin(prop_name, prop_value, style);
    } else if (prop_name.find("padding") == 0) {
        applyPadding(prop_name, prop_value, style);
    } else if (prop_name.find("page-break") == 0) {
        applyPageBreak(prop_name, prop_value, style);
    }
}

void CSSBoxModelApplier::applyMargin(const std::string& property, const std::string& value, 
                                     CSSComputedStyle& style) {
    int length = parseLength(value);
    
    if (property == "margin-top") {
        style.margin_top = length;
    } else if (property == "margin-bottom") {
        style.margin_bottom = length;
    } else if (property == "margin-left") {
        style.margin_left = length;
    } else if (property == "margin-right") {
        style.margin_right = length;
    }
}

void CSSBoxModelApplier::applyPadding(const std::string& property, const std::string& value, 
                                      CSSComputedStyle& style) {
    int length = parseLength(value);
    
    if (property == "padding-top") {
        style.padding_top = length;
    } else if (property == "padding-bottom") {
        style.padding_bottom = length;
    } else if (property == "padding-left") {
        style.padding_left = length;
    } else if (property == "padding-right") {
        style.padding_right = length;
    }
}

void CSSBoxModelApplier::applyPageBreak(const std::string& property, const std::string& value, 
                                        CSSComputedStyle& style) {
    bool is_always = value.find("always") != std::string::npos;
    bool is_avoid = value.find("avoid") != std::string::npos;
    
    if (property == "page-break-before" && is_always) {
        style.page_break_before = true;
    } else if (property == "page-break-after" && is_always) {
        style.page_break_after = true;
    } else if (property == "page-break-inside" && is_avoid) {
        style.page_break_inside_avoid = true;
    }
}

int CSSBoxModelApplier::parseLength(const std::string& value) {
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

float CSSBoxModelApplier::parseFloat(const std::string& value) {
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

std::string CSSBoxModelApplier::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string CSSBoxModelApplier::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace epub