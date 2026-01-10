#pragma once

#include "dom_node.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace epub {

// CSS selector and rules
struct CSSRule {
    std::string selector;
    std::unordered_map<std::string, std::string> properties;
    int specificity;
    
    CSSRule() : specificity(0) {}
};

class StyleResolver {
public:
    StyleResolver();
    
    void addStylesheet(const std::string& css);
    void clear();
    
    // Resolve all styles in document tree (cascade + compute)
    void resolveStyles(DocumentNode* document);

private:
    void parseStylesheet(const std::string& css, std::vector<CSSRule>& rules);
    void parseRule(const std::string& selector_str, const std::string& properties_str,
                  std::vector<CSSRule>& rules);
    
<<<<<<< Updated upstream
    void parseDeclarations(const std::string& declarations_str,
                          std::unordered_map<std::string, std::string>& out);
    
    bool matchesSelector(ElementNode* element, const std::string& selector);
=======
    void cascadeStyles(DOMNode* node, ComputedStyle parent_style);
    void applyDefaultStyles(ElementNode* element, ComputedStyle& style);
    void applyMatchingRules(ElementNode* element, ComputedStyle& style);
    void applyInlineStyle(ElementNode* element, ComputedStyle& style);
    void computeFinalStyle(ComputedStyle& style, const ComputedStyle& parent);
    
    bool selectorMatches(const std::string& selector, ElementNode* element);
>>>>>>> Stashed changes
    int calculateSpecificity(const std::string& selector);
    
    void parseProperty(const std::string& name, const std::string& value,
                      ComputedStyle& style);
    ComputedStyle::Color parseColor(const std::string& color_str);
    float parseLength(const std::string& length_str);
    
<<<<<<< Updated upstream
    DisplayType parseDisplay(const std::string& value);
    ComputedStyle::TextAlign parseTextAlign(const std::string& value);
    ComputedStyle::VerticalAlign parseVerticalAlign(const std::string& value);
    ComputedStyle::WhiteSpace parseWhiteSpace(const std::string& value);
    ComputedStyle::Color parseColor(const std::string& value);
    float parseLength(const std::string& value, float base_size);
    
    std::string toLowerCase(const std::string& str);
=======
>>>>>>> Stashed changes
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
    
    std::vector<CSSRule> rules_;
};

} // namespace epub