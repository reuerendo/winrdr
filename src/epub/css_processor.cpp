#include "css_processor.h"
#include "../utils/logger.h"
#include <lexbor/selectors/selectors.h>
#include <lexbor/html/interfaces/element.h>
#include <lexbor/dom/interfaces/element.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cctype>
#include <fstream>

namespace epub {

// Weight constants for specificity (based on crengine)
static constexpr int WEIGHT_SPECIFICITY_ID = 1 << 29;
static constexpr int WEIGHT_SPECIFICITY_ATTRCLS = 1 << 24;
static constexpr int WEIGHT_SPECIFICITY_ELEMENT = 1 << 19;
static constexpr int WEIGHT_SELECTOR_ORDER = 1;

CSSProcessor::CSSProcessor() 
    : selectors_(nullptr)
    , document_(nullptr)
    , debug_enabled_(false)
    , selector_order_(0)
{
    selectors_ = lxb_selectors_create();
    lxb_selectors_init(selectors_);
}

CSSProcessor::~CSSProcessor() {
    clear();
    
    if (selectors_) {
        lxb_selectors_destroy(selectors_, true);
    }
}

void CSSProcessor::clear() {
    rules_.clear();
    inline_styles_.clear();
    loaded_stylesheets_.clear();
    debug_logger_.clear();
    selector_order_ = 0;
}

void CSSProcessor::clearDocument() {
    inline_styles_.clear();
    debug_logger_.clear();
}

void CSSProcessor::addInlineStyle(lxb_dom_element_t* element, const std::string& style_text) {
    if (!element || style_text.empty()) {
        return;
    }
    
    std::unordered_map<std::string, PropertyValue>& properties = inline_styles_[element];
    parseInlineStyle(style_text, properties);
}

void CSSProcessor::parseInlineStyle(const std::string& style_text,
                                    std::unordered_map<std::string, PropertyValue>& properties) {
    const int inline_specificity = 1000;
    
    std::istringstream stream(style_text);
    std::string declaration;
    
    while (std::getline(stream, declaration, ';')) {
        size_t colon_pos = declaration.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        
        std::string prop_name = trim(declaration.substr(0, colon_pos));
        std::string prop_value = trim(declaration.substr(colon_pos + 1));
        
        bool is_important = false;
        size_t important_pos = prop_value.find("!important");
        if (important_pos != std::string::npos) {
            prop_value = trim(prop_value.substr(0, important_pos));
            is_important = true;
        }
        
        if (!prop_name.empty() && !prop_value.empty()) {
            PropertyValue pv;
            pv.value = prop_value;
            pv.specificity = inline_specificity;
            pv.is_important = is_important;
            properties[prop_name] = pv;
        }
    }
}

bool CSSProcessor::loadDefaultStyles(const std::string& css_file_path) {
    LOG_INFO("Loading default styles from:", css_file_path);
    
    std::ifstream file(css_file_path);
    if (!file.is_open()) {
        LOG_WARNING("Could not open default CSS file:", css_file_path);
        return false;
    }
    
    std::string css_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    if (css_content.empty()) {
        LOG_WARNING("Default CSS file is empty");
        return false;
    }
    
    LOG_INFO("Loaded default CSS, length:", css_content.length());
    return parseStylesheet(css_content, "default:" + css_file_path);
}

void CSSProcessor::setDocument(lxb_html_document_t* document) {
    document_ = document;
}

bool CSSProcessor::parseStylesheet(const std::string& css, const std::string& source_path) {
    if (css.empty()) {
        return false;
    }
    
    if (!source_path.empty()) {
        if (loaded_stylesheets_.find(source_path) != loaded_stylesheets_.end()) {
            LOG_DEBUG("Stylesheet already loaded, skipping:", source_path);
            return true;
        }
        loaded_stylesheets_.insert(source_path);
    }
    
    LOG_DEBUG("Parsing CSS stylesheet, source:", source_path, "length:", css.length());
    
    const size_t rules_before = rules_.size();
    parseSimpleCSS(css);
    const size_t rules_added = rules_.size() - rules_before;
    
    LOG_INFO("CSS rules added:", rules_added, "Total rules:", rules_.size());
    return true;
}

void CSSProcessor::parseSimpleCSS(const std::string& css) {
    size_t pos = 0;
    
    while (pos < css.length()) {
        size_t brace_open = css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector_text = css.substr(pos, brace_open - pos);
        
        size_t brace_close = css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string declarations_text = css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        std::istringstream selector_stream(selector_text);
        std::string selector;
        
        while (std::getline(selector_stream, selector, ',')) {
            selector = trim(selector);
            if (selector.empty()) continue;
            
            RuleData rule;
            rule.selector = selector;
            rule.specificity = selector_matcher_.calculateSpecificity(selector).calculate();
            rule.specificity += selector_order_;
            selector_order_ += WEIGHT_SELECTOR_ORDER;
            rule.is_important = false;
            
            std::istringstream decl_stream(declarations_text);
            std::string declaration;
            
            while (std::getline(decl_stream, declaration, ';')) {
                size_t colon_pos = declaration.find(':');
                if (colon_pos == std::string::npos) continue;
                
                std::string prop_name = trim(declaration.substr(0, colon_pos));
                std::string prop_value = trim(declaration.substr(colon_pos + 1));
                
                if (prop_value.find("!important") != std::string::npos) {
                    rule.is_important = true;
                    size_t important_pos = prop_value.find("!important");
                    prop_value = trim(prop_value.substr(0, important_pos));
                }
                
                if (!prop_name.empty() && !prop_value.empty()) {
                    rule.properties[prop_name] = prop_value;
                }
            }
            
            if (!rule.properties.empty()) {
                rules_.push_back(rule);
            }
        }
        
        pos = brace_close + 1;
    }
}

CSSComputedStyle CSSProcessor::computeStyle(lxb_dom_node_t* node) {
    CSSComputedStyle style;
    
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return style;
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    lxb_dom_node_t* parent_node = lxb_dom_node_parent(node);
    if (parent_node && parent_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        CSSComputedStyle parent_style = computeStyle(parent_node);
        style.inheritFrom(parent_style);
    }
    
    std::vector<const RuleData*> matched_rules;
    std::vector<std::string> matched_selectors;
    
    for (const RuleData& rule : rules_) {
        if (selector_matcher_.matchesSelector(node, rule.selector)) {
            matched_rules.push_back(&rule);
            if (debug_enabled_) {
                matched_selectors.push_back(rule.selector);
            }
        }
    }
    
    std::stable_sort(matched_rules.begin(), matched_rules.end(),
        [](const RuleData* a, const RuleData* b) {
            if (a->is_important != b->is_important) {
                return !a->is_important;
            }
            return a->specificity < b->specificity;
        });
    
    for (const RuleData* rule : matched_rules) {
        uint8_t importance = 0;
        if (rule->is_important) {
            importance = 1;
        }
        
        for (const auto& prop_pair : rule->properties) {
            property_applier_.applyProperty(prop_pair.first, prop_pair.second, style, importance);
        }
    }
    
    auto inline_it = inline_styles_.find(element);
    if (inline_it != inline_styles_.end()) {
        for (const auto& prop_pair : inline_it->second) {
            uint8_t importance = prop_pair.second.is_important ? 2 : 1;
            property_applier_.applyProperty(prop_pair.first, prop_pair.second.value, 
                                          style, importance);
        }
    }
    
    if (debug_enabled_) {
        int depth = 0;
        lxb_dom_node_t* parent = lxb_dom_node_parent(node);
        while (parent && parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            depth++;
            parent = lxb_dom_node_parent(parent);
        }
        
        debug_logger_.logElement(node, matched_selectors, style, depth);
    }
    
    return style;
}

void CSSProcessor::saveDebugReport(const std::string& output_path) {
    debug_logger_.printReport(output_path);
}

void CSSProcessor::printDebugReportToConsole() {
    debug_logger_.printReportToConsole();
}

std::string CSSProcessor::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string CSSProcessor::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

TextStyle CSSProcessor::convertToTextStyle(const CSSComputedStyle& css_style) {
    TextStyle style = TextStyle::Normal;
    
    if (css_style.font_weight >= CSSFontWeight::Bold) {
        style = style | TextStyle::Bold;
    }
    
    if (css_style.font_style == CSSFontStyle::Italic || 
        css_style.font_style == CSSFontStyle::Oblique) {
        style = style | TextStyle::Italic;
    }
    
    if (css_style.text_decoration == CSSTextDecoration::Underline) {
        style = style | TextStyle::Underline;
    }
    
    if (css_style.text_decoration == CSSTextDecoration::LineThrough) {
        style = style | TextStyle::Strikethrough;
    }
    
    if (css_style.font_family == CSSFontFamily::Monospace ||
        css_style.font_name.find("mono") != std::string::npos ||
        css_style.font_name.find("courier") != std::string::npos) {
        style = style | TextStyle::Monospace;
    }
    
    if (css_style.vertical_align == CSSVerticalAlign::Sub) {
        style = style | TextStyle::Subscript;
    } else if (css_style.vertical_align == CSSVerticalAlign::Super) {
        style = style | TextStyle::Superscript;
    }
    
    return style;
}

TextAlign CSSProcessor::convertToTextAlign(const CSSComputedStyle& css_style) {
    switch (css_style.text_align) {
        case CSSTextAlign::Left:
            return TextAlign::Left;
        case CSSTextAlign::Right:
            return TextAlign::Right;
        case CSSTextAlign::Center:
            return TextAlign::Center;
        case CSSTextAlign::Justify:
            return TextAlign::Justify;
        default:
            return TextAlign::Left;
    }
}

} // namespace epub