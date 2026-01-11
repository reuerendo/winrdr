// CSS property application: handles text properties (font, text-decoration, etc.)
#pragma once

#include "css_types.h"
#include <string>

namespace epub {

class CSSPropertyApplier {
public:
    CSSPropertyApplier() = default;
    ~CSSPropertyApplier() = default;
    
    void applyProperty(const std::string& name, const std::string& value, CSSComputedStyle& style);
    
private:
    void applyFontWeight(const std::string& value, CSSComputedStyle& style);
    void applyFontStyle(const std::string& value, CSSComputedStyle& style);
    void applyTextDecoration(const std::string& value, CSSComputedStyle& style);
    void applyFontVariant(const std::string& value, CSSComputedStyle& style);
    void applyFontFamily(const std::string& value, CSSComputedStyle& style);
    void applyFontSize(const std::string& value, CSSComputedStyle& style);
    void applyLineHeight(const std::string& value, CSSComputedStyle& style);
    void applyLetterSpacing(const std::string& value, CSSComputedStyle& style);
    void applyTextAlign(const std::string& value, CSSComputedStyle& style);
    void applyDisplay(const std::string& value, CSSComputedStyle& style);
    void applyVerticalAlign(const std::string& value, CSSComputedStyle& style);
    void applyTextTransform(const std::string& value, CSSComputedStyle& style);
    void applyWhiteSpace(const std::string& value, CSSComputedStyle& style);
    
    int parseLength(const std::string& value);
    float parseFloat(const std::string& value);
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
};

} // namespace epub