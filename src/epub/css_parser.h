#pragma once

#include "formatted_text.h"
#include <string>
#include <unordered_map>

namespace epub {

struct CSSStyle {
    TextStyle text_style;
    TextAlign align;
    bool has_style;
    bool has_align;
    
    CSSStyle() 
        : text_style(TextStyle::Normal)
        , align(TextAlign::Left)
        , has_style(false)
        , has_align(false) 
    {}
};

class CSSParser {
public:
    CSSParser();
    
    void parseStylesheet(const std::string& css);
    void parseInlineStyle(const std::string& style, CSSStyle& out);
    
    CSSStyle getStyle(const std::string& selector) const;
    void clear();

private:
    void parseRule(const std::string& selector, const std::string& properties);
    void parseProperty(const std::string& name, const std::string& value, CSSStyle& style);
    
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
    
    std::unordered_map<std::string, CSSStyle> styles_;
};

} // namespace epub