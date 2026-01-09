#include "css_parser.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>

namespace epub {

CSSParser::CSSParser() {}

void CSSParser::parseStylesheet(const std::string& css) {
    size_t pos = 0;
    
    while (pos < css.length()) {
        // Find selector
        size_t brace_open = css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(css.substr(pos, brace_open - pos));
        
        // Find properties
        size_t brace_close = css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string properties = css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        parseRule(selector, properties);
        
        pos = brace_close + 1;
    }
    
    LOG_DEBUG("Parsed CSS rules:", styles_.size());
}

void CSSParser::parseInlineStyle(const std::string& style, CSSStyle& out) {
    size_t pos = 0;
    
    while (pos < style.length()) {
        size_t colon = style.find(':', pos);
        if (colon == std::string::npos) break;
        
        std::string name = trim(style.substr(pos, colon - pos));
        
        size_t semicolon = style.find(';', colon);
        if (semicolon == std::string::npos) {
            semicolon = style.length();
        }
        
        std::string value = trim(style.substr(colon + 1, semicolon - colon - 1));
        
        parseProperty(name, value, out);
        
        pos = semicolon + 1;
    }
}

CSSStyle CSSParser::getStyle(const std::string& selector) const {
    auto it = styles_.find(toLowerCase(selector));
    if (it != styles_.end()) {
        return it->second;
    }
    return CSSStyle();
}

void CSSParser::clear() {
    styles_.clear();
}

void CSSParser::parseRule(const std::string& selector, const std::string& properties) {
    CSSStyle style;
    
    size_t pos = 0;
    while (pos < properties.length()) {
        size_t colon = properties.find(':', pos);
        if (colon == std::string::npos) break;
        
        std::string name = trim(properties.substr(pos, colon - pos));
        
        size_t semicolon = properties.find(';', colon);
        if (semicolon == std::string::npos) {
            semicolon = properties.length();
        }
        
        std::string value = trim(properties.substr(colon + 1, semicolon - colon - 1));
        
        parseProperty(name, value, style);
        
        pos = semicolon + 1;
    }
    
    if (style.has_style || style.has_align) {
        styles_[toLowerCase(selector)] = style;
    }
}

void CSSParser::parseProperty(const std::string& name, const std::string& value, 
                              CSSStyle& style) {
    std::string name_lower = toLowerCase(name);
    std::string value_lower = toLowerCase(value);
    
    if (name_lower == "font-weight") {
        if (value_lower == "bold" || value_lower == "bolder" || value_lower == "700" || 
            value_lower == "800" || value_lower == "900") {
            style.text_style = style.text_style | TextStyle::Bold;
            style.has_style = true;
        }
    } 
    else if (name_lower == "font-style") {
        if (value_lower == "italic" || value_lower == "oblique") {
            style.text_style = style.text_style | TextStyle::Italic;
            style.has_style = true;
        }
    }
    else if (name_lower == "font-family") {
        if (value_lower.find("monospace") != std::string::npos ||
            value_lower.find("courier") != std::string::npos ||
            value_lower.find("mono") != std::string::npos) {
            style.text_style = style.text_style | TextStyle::Monospace;
            style.has_style = true;
        }
    }
    else if (name_lower == "font-size") {
        if (value_lower.find("small") != std::string::npos ||
            value_lower.find("0.8") != std::string::npos ||
            value_lower.find("80%") != std::string::npos) {
            style.text_style = style.text_style | TextStyle::Small;
            style.has_style = true;
        }
    }
    else if (name_lower == "text-decoration") {
        if (value_lower.find("underline") != std::string::npos) {
            style.text_style = style.text_style | TextStyle::Underline;
            style.has_style = true;
        }
        if (value_lower.find("line-through") != std::string::npos) {
            style.text_style = style.text_style | TextStyle::Strikethrough;
            style.has_style = true;
        }
    }
    else if (name_lower == "text-align") {
        style.has_align = true;
        if (value_lower == "center") {
            style.align = TextAlign::Center;
        } else if (value_lower == "right") {
            style.align = TextAlign::Right;
        } else if (value_lower == "justify") {
            style.align = TextAlign::Justify;
        } else {
            style.align = TextAlign::Left;
        }
    }
    else if (name_lower == "vertical-align") {
        if (value_lower == "sub" || value_lower == "subscript") {
            style.text_style = style.text_style | TextStyle::Subscript;
            style.has_style = true;
        } else if (value_lower == "sup" || value_lower == "super" || value_lower == "superscript") {
            style.text_style = style.text_style | TextStyle::Superscript;
            style.has_style = true;
        }
    }
}

std::string CSSParser::trim(const std::string& str) const {
    size_t start = 0;
    while (start < str.length() && std::isspace(str[start])) {
        start++;
    }
    
    size_t end = str.length();
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    
    return str.substr(start, end - start);
}

std::string CSSParser::toLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace epub