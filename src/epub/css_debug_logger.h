#pragma once

#include "css_types.h"
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace epub {

struct CSSDebugInfo {
    std::string tag_name;
    std::string id;
    std::string class_name;
    std::vector<std::string> matched_selectors;
    CSSComputedStyle computed_style;
    int depth;
};

class CSSDebugLogger {
public:
    CSSDebugLogger() = default;
    ~CSSDebugLogger() = default;
    
    void logElement(lxb_dom_node_t* node, 
                   const std::vector<std::string>& matched_selectors,
                   const CSSComputedStyle& style,
                   int depth);
    
    void printReport(const std::string& output_path);
    void printReportToConsole();
    void clear();
    
private:
    void writeReport(std::ostream& out);
    std::string formatStyle(const CSSComputedStyle& style);
    std::string getTagName(lxb_dom_node_t* node);
    std::string getClassName(lxb_dom_node_t* node);
    std::string getIdName(lxb_dom_node_t* node);
    
    std::vector<CSSDebugInfo> elements_;
};

} // namespace epub