// CSS property application with CREngine cascade logic
#pragma once

#include "css_types.h"
#include <string>
#include <vector>

namespace epub {

class CSSPropertyApplier {
public:
    CSSPropertyApplier() = default;
    ~CSSPropertyApplier() = default;
    
    void applyProperty(const std::string& name, const std::string& value, 
                      CSSComputedStyle& style, uint8_t importance = 0);
    
private:
    std::vector<CSSLength> parseShorthand(const std::string& value);
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
};

} // namespace epub