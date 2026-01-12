// Main CSS processor: coordinates parsing, selector matching, and property application
#pragma once

#include "css_types.h"
#include "css_selector_matcher.h"
#include "css_property_applier.h"
#include "css_box_model_applier.h"
#include "css_debug_logger.h"
#include "formatted_text.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

typedef struct lxb_selectors lxb_selectors_t;
typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;
typedef struct lxb_html_document lxb_html_document_t;

namespace epub {

class CSSProcessor {
public:
    CSSProcessor();
    ~CSSProcessor();
    
    void clear();
    void clearDocument();
    
    bool loadDefaultStyles(const std::string& css_file_path);
    bool parseStylesheet(const std::string& css, const std::string& source_path = "");
    void setDocument(lxb_html_document_t* document);
    
    void addInlineStyle(lxb_dom_element_t* element, const std::string& style_text);
    
    CSSComputedStyle computeStyle(lxb_dom_node_t* node);
    
    TextStyle convertToTextStyle(const CSSComputedStyle& css_style);
    TextAlign convertToTextAlign(const CSSComputedStyle& css_style);
    
    size_t getRulesCount() const { return rules_.size(); }
    
    void enableDebugMode(bool enable) { debug_enabled_ = enable; }
    void saveDebugReport(const std::string& output_path);
    void printDebugReportToConsole();
    
    CSSDebugLogger& getDebugLogger() { return debug_logger_; }

private:
    void parseSimpleCSS(const std::string& css);
    void parseInlineStyle(const std::string& style_text, 
                         std::unordered_map<std::string, PropertyValue>& properties);
    
    void traverseAndDebug(lxb_dom_node_t* node, int depth);
    
    std::string trim(const std::string& str);
    std::string toLowerCase(const std::string& str);
    
    lxb_selectors_t* selectors_;
    lxb_html_document_t* document_;
    
    std::vector<RuleData> rules_;
    std::unordered_map<lxb_dom_element_t*, std::unordered_map<std::string, PropertyValue>> inline_styles_;
    std::unordered_set<std::string> loaded_stylesheets_;
    
    CSSSelectorMatcher selector_matcher_;
    CSSPropertyApplier property_applier_;
    CSSBoxModelApplier box_model_applier_;
    CSSDebugLogger debug_logger_;
    
    bool debug_enabled_;
};

} // namespace epub