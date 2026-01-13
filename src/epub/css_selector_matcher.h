#pragma once

#include <string>
#include <vector>

typedef struct lxb_dom_node lxb_dom_node_t;

namespace epub {

// Specificity calculation based on CSS specs
// Format: (a,b,c,d) where:
// a = inline styles (handled separately)
// b = ID selectors
// c = class selectors, attribute selectors, pseudo-classes
// d = element selectors, pseudo-elements
struct SelectorSpecificity {
    int id_count = 0;        // b
    int class_count = 0;     // c  
    int element_count = 0;   // d
    int order = 0;           // parsing order for tie-breaking
    
    int calculate() const {
        return (id_count * 10000) + (class_count * 100) + element_count + order;
    }
    
    bool operator<(const SelectorSpecificity& other) const {
        return calculate() < other.calculate();
    }
};

class CSSSelectorMatcher {
public:
    CSSSelectorMatcher() = default;
    ~CSSSelectorMatcher() = default;
    
    SelectorSpecificity calculateSpecificity(const std::string& selector);
    bool matchesSelector(lxb_dom_node_t* node, const std::string& selector);
    
private:
    bool matchesSimpleSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesBasicSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesDescendantSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesChildSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesAdjacentSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesSiblingSelector(lxb_dom_node_t* node, const std::string& selector);
    bool matchesPseudoClass(lxb_dom_node_t* node, const std::string& pseudo);
    
    std::string getTagName(lxb_dom_node_t* node);
    std::string getClassName(lxb_dom_node_t* node);
    std::string getIdName(lxb_dom_node_t* node);
    std::string getAttributeValue(lxb_dom_node_t* node, const std::string& attr_name);
    
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
    
    void countSelectorParts(const std::string& selector, SelectorSpecificity& spec);
};

} // namespace epub