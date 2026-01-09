#pragma once

#include "dom_node.h"
#include "css_parser.h"
#include <string>
#include <vector>

namespace epub {

struct CSSRule {
    std::string selector;
    int specificity;
    std::unordered_map<std::string, std::string> declarations;
};

class StyleResolver {
public:
    StyleResolver();
    
    void addStylesheet(const std::string& css);
    void clear();
    
    void resolveStyles(DocumentNode* document);

private:
    struct SelectorPart {
        std::string selector;
        char combinator;  // ' ' (descendant), '>' (child), '+' (adjacent), '~' (sibling)
    };
    
    struct AttributeMatcher {
        std::string name;
        std::string op;     // "exists", "=", "^=", "$=", "*=", "~=", "|="
        std::string value;
    };
    
    void applyDefaultStyles(DOMNode* node);
    int applyCSSRules(DOMNode* node);
    void applyInlineStyle(ElementNode* element);
    void inheritStyles(DOMNode* node);
    
    void parseDeclarations(const std::string& declarations_str,
                          std::unordered_map<std::string, std::string>& out);
    
    std::string removeComments(const std::string& css);
    std::vector<std::string> splitSelectors(const std::string& selector);
    
    // Complex selector matching
    bool matchesSelector(ElementNode* element, const std::string& selector);
    std::vector<SelectorPart> parseComplexSelector(const std::string& selector);
    bool matchesSimpleSelector(ElementNode* element, const std::string& selector);
    
    // Attribute and class matching
    AttributeMatcher parseAttributeSelector(const std::string& attr_str);
    bool hasClass(ElementNode* element, const std::string& class_name);
    bool matchesAttribute(ElementNode* element, const AttributeMatcher& matcher);
    bool matchesPseudoClass(ElementNode* element, const std::string& pseudo);
    
    int calculateSpecificity(const std::string& selector);
    
    void applyDeclaration(const std::string& property, const std::string& value, 
                         ComputedStyle& style);
    
    DisplayType parseDisplay(const std::string& value);
    ComputedStyle::TextAlign parseTextAlign(const std::string& value);
    ComputedStyle::VerticalAlign parseVerticalAlign(const std::string& value);
    ComputedStyle::WhiteSpace parseWhiteSpace(const std::string& value);
    ComputedStyle::Color parseColor(const std::string& value);
    float parseLength(const std::string& value, float base_size);
    
    std::string toLowerCase(const std::string& str);
    std::string trim(const std::string& str);
    
    std::vector<CSSRule> rules_;
    CSSParser css_parser_;
};

} // namespace epub