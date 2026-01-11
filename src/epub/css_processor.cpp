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

CSSProcessor::CSSProcessor() 
    : selectors_(nullptr)
    , document_(nullptr)
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
        
        if (!prop_name.empty() && !prop_value.empty()) {
            PropertyValue pv;
            pv.value = prop_value;
            pv.specificity = inline_specificity;
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
    return parseStylesheet(css_content);
}

void CSSProcessor::setDocument(lxb_html_document_t* document) {
    document_ = document;
}

bool CSSProcessor::parseStylesheet(const std::string& css) {
    if (css.empty()) {
        return false;
    }
    
    LOG_DEBUG("Parsing CSS stylesheet, length:", css.length());
    
    parseSimpleCSS(css);
    
    LOG_INFO("CSS rules loaded:", rules_.size());
    return true;
}

void CSSProcessor::parseSimpleCSS(const std::string& css) {
    size_t pos = 0;
    int rules_parsed = 0;
    
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
            rule.specificity = selector_matcher_.calculateSpecificity(selector);
            
            std::istringstream decl_stream(declarations_text);
            std::string declaration;
            
            while (std::getline(decl_stream, declaration, ';')) {
                size_t colon_pos = declaration.find(':');
                if (colon_pos == std::string::npos) continue;
                
                std::string prop_name = trim(declaration.substr(0, colon_pos));
                std::string prop_value = trim(declaration.substr(colon_pos + 1));
                
                if (!prop_name.empty() && !prop_value.empty()) {
                    rule.properties[prop_name] = prop_value;
                }
            }
            
            if (!rule.properties.empty()) {
                rules_.push_back(rule);
                rules_parsed++;
            }
        }
        
        pos = brace_close + 1;
    }
    
    LOG_DEBUG("Parsed CSS rules:", rules_parsed);
}

CSSComputedStyle CSSProcessor::computeStyle(lxb_dom_node_t* node) {
    CSSComputedStyle style;
    
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return style;
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    std::unordered_map<std::string, PropertyValue> matched_properties;
    
    for (const RuleData& rule : rules_) {
        if (selector_matcher_.matchesSelector(node, rule.selector)) {
            for (const auto& prop_pair : rule.properties) {
                const std::string& prop_name = prop_pair.first;
                const std::string& prop_value = prop_pair.second;
                
                auto it = matched_properties.find(prop_name);
                if (it == matched_properties.end() || it->second.specificity < rule.specificity) {
                    PropertyValue pv;
                    pv.value = prop_value;
                    pv.specificity = rule.specificity;
                    matched_properties[prop_name] = pv;
                }
            }
        }
    }
    
    auto inline_it = inline_styles_.find(element);
    if (inline_it != inline_styles_.end()) {
        for (const auto& prop_pair : inline_it->second) {
            matched_properties[prop_pair.first] = prop_pair.second;
        }
    }
    
    for (const auto& prop_pair : matched_properties) {
        property_applier_.applyProperty(prop_pair.first, prop_pair.second.value, style);
        box_model_applier_.applyProperty(prop_pair.first, prop_pair.second.value, style);
    }
    
    // DEBUG LOGGING - Add this section
    if (style.margin_top != 0 || style.margin_bottom != 0 || 
        style.padding_top != 0 || style.padding_bottom != 0 || 
        style.text_indent != 0) {
        
        std::string tag_name;
        const lxb_char_t* tag_name_raw = lxb_dom_element_qualified_name(element, nullptr);
        if (tag_name_raw) {
            tag_name = std::string(reinterpret_cast<const char*>(tag_name_raw));
        }
        
        LOG_DEBUG("CSS computed for", tag_name, 
                  "margin-top:", style.margin_top,
                  "margin-bottom:", style.margin_bottom,
                  "padding-top:", style.padding_top,
                  "padding-bottom:", style.padding_bottom,
                  "text-indent:", style.text_indent);
    }
    
    return style;
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
    
    if (css_style.bold) style = style | TextStyle::Bold;
    if (css_style.italic) style = style | TextStyle::Italic;
    if (css_style.underline) style = style | TextStyle::Underline;
    if (css_style.strikethrough) style = style | TextStyle::Strikethrough;
    if (css_style.monospace) style = style | TextStyle::Monospace;
    
    if (css_style.vertical_align == CSSVerticalAlign::Sub) {
        style = style | TextStyle::Subscript;
    } else if (css_style.vertical_align == CSSVerticalAlign::Super) {
        style = style | TextStyle::Superscript;
    }
    
    return style;
}

TextAlign CSSProcessor::convertToTextAlign(const CSSComputedStyle& css_style) {
    return css_style.text_align;
}

} // namespace epub