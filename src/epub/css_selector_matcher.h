// CSS selector matching: handles all selector types including descendant, child, adjacent, sibling, and pseudo-classes
#pragma once

#include <string>

typedef struct lxb_dom_node lxb_dom_node_t;

namespace epub {

class CSSSelectorMatcher {
public:
    CSSSelectorMatcher() = default;
    ~CSSSelectorMatcher() = default;
    
    int calculateSpecificity(const std::string& selector);
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
};

} // namespace epub