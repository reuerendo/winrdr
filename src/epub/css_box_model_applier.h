// CSS box model properties: handles margin, padding, and page-break properties
#pragma once

#include "css_types.h"
#include <string>

namespace epub {

class CSSBoxModelApplier {
public:
    CSSBoxModelApplier() = default;
    ~CSSBoxModelApplier() = default;
    
    void applyProperty(const std::string& property, const std::string& value, CSSComputedStyle& style);
    
private:
    void applyMargin(const std::string& property, const std::string& value, CSSComputedStyle& style);
    void applyPadding(const std::string& property, const std::string& value, CSSComputedStyle& style);
    void applyPageBreak(const std::string& property, const std::string& value, CSSComputedStyle& style);
    
    int parseLength(const std::string& value);
    float parseFloat(const std::string& value);
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
};

} // namespace epub