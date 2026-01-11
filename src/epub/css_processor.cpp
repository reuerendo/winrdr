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
            rule.specificity = calculateSpecificity(selector);
            
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
        if (matchesSelector(node, rule.selector)) {
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
        applyProperty(prop_pair.first, prop_pair.second.value, style);
    }
    
    return style;
}

int CSSProcessor::calculateSpecificity(const std::string& selector) {
    int specificity = 0;
    
    size_t id_count = 0;
    size_t class_count = 0;
    size_t element_count = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '#') {
            id_count++;
        } else if (selector[i] == '.' || selector[i] == '[') {
            class_count++;
        } else if (std::isalpha(selector[i]) && 
                   (i == 0 || !std::isalnum(selector[i-1]))) {
            element_count++;
        }
    }
    
    specificity = (id_count * 100) + (class_count * 10) + element_count;
    
    return specificity;
}

std::string CSSProcessor::getTagName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    const lxb_char_t* tag_name_raw = lxb_dom_element_qualified_name(element, nullptr);
    
    if (tag_name_raw) {
        return std::string(reinterpret_cast<const char*>(tag_name_raw));
    }
    
    return "";
}

std::string CSSProcessor::getClassName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>("class"),
        5,
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

std::string CSSProcessor::getIdName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>("id"),
        2,
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

std::string CSSProcessor::getAttributeValue(lxb_dom_node_t* node, const std::string& attr_name) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>(attr_name.c_str()),
        attr_name.length(),
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

bool CSSProcessor::matchesSelector(lxb_dom_node_t* node, const std::string& selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return false;
    }
    
    std::string selector_lower = toLowerCase(trim(selector));
    
    std::string tag_name = toLowerCase(getTagName(node));
    std::string class_name = getClassName(node);
    std::string id_name = getIdName(node);
    
    if (selector_lower == "*") {
        return true;
    }
    
    if (selector_lower[0] == '#') {
        return id_name == selector_lower.substr(1);
    }
    
    if (selector_lower[0] == '.') {
        std::string class_to_match = selector_lower.substr(1);
        std::istringstream class_stream(class_name);
        std::string cls;
        while (class_stream >> cls) {
            if (toLowerCase(cls) == class_to_match) {
                return true;
            }
        }
        return false;
    }
    
    if (selector_lower.find('[') != std::string::npos) {
        size_t bracket_pos = selector_lower.find('[');
        std::string base_tag = selector_lower.substr(0, bracket_pos);
        
        if (!base_tag.empty() && base_tag != tag_name) {
            return false;
        }
        
        size_t bracket_end = selector_lower.find(']', bracket_pos);
        if (bracket_end != std::string::npos) {
            std::string attr_expr = selector_lower.substr(bracket_pos + 1, bracket_end - bracket_pos - 1);
            
            size_t eq_pos = attr_expr.find('=');
            if (eq_pos != std::string::npos) {
                std::string attr_name = trim(attr_expr.substr(0, eq_pos));
                std::string attr_value = trim(attr_expr.substr(eq_pos + 1));
                
                if (attr_value.front() == '"' || attr_value.front() == '\'') {
                    attr_value = attr_value.substr(1, attr_value.length() - 2);
                }
                
                std::string actual_value = getAttributeValue(node, attr_name);
                return toLowerCase(actual_value) == toLowerCase(attr_value);
            } else {
                std::string attr_name = trim(attr_expr);
                return !getAttributeValue(node, attr_name).empty();
            }
        }
        
        return true;
    }
    
    if (selector_lower.find(' ') != std::string::npos) {
        size_t space_pos = selector_lower.find_last_of(' ');
        std::string last_part = trim(selector_lower.substr(space_pos + 1));
        
        if (last_part[0] == '.') {
            std::string class_to_match = last_part.substr(1);
            std::istringstream class_stream(class_name);
            std::string cls;
            while (class_stream >> cls) {
                if (toLowerCase(cls) == class_to_match) {
                    return true;
                }
            }
            return false;
        }
        
        if (last_part[0] == '#') {
            return id_name == last_part.substr(1);
        }
        
        return tag_name == last_part;
    }
    
    return tag_name == selector_lower;
}

void CSSProcessor::applyProperty(const std::string& name, const std::string& value, 
                                CSSComputedStyle& style) {
    std::string prop_name = toLowerCase(trim(name));
    std::string prop_value = toLowerCase(trim(value));
    
    if (prop_name == "font-weight") {
        applyFontWeight(prop_value, style);
    } else if (prop_name == "font-style") {
        applyFontStyle(prop_value, style);
    } else if (prop_name == "text-decoration" || prop_name == "text-decoration-line") {
        applyTextDecoration(prop_value, style);
    } else if (prop_name == "font-variant" || prop_name == "font-variant-caps") {
        applyFontVariant(prop_value, style);
    } else if (prop_name == "font-family") {
        applyFontFamily(prop_value, style);
    } else if (prop_name == "font-size") {
        applyFontSize(prop_value, style);
    } else if (prop_name == "line-height") {
        applyLineHeight(prop_value, style);
    } else if (prop_name == "letter-spacing") {
        applyLetterSpacing(prop_value, style);
    } else if (prop_name == "text-align" || prop_name == "text-align-last") {
        applyTextAlign(prop_value, style);
    } else if (prop_name == "display") {
        applyDisplay(prop_value, style);
    } else if (prop_name == "vertical-align" || prop_name == "font-variant-position") {
        applyVerticalAlign(prop_value, style);
    } else if (prop_name == "text-transform") {
        applyTextTransform(prop_value, style);
    } else if (prop_name == "white-space") {
        applyWhiteSpace(prop_value, style);
    } else if (prop_name.find("margin") == 0) {
        applyMargin(prop_name, prop_value, style);
    } else if (prop_name.find("padding") == 0) {
        applyPadding(prop_name, prop_value, style);
    } else if (prop_name.find("page-break") == 0) {
        applyPageBreak(prop_name, prop_value, style);
    }
}

void CSSProcessor::applyFontWeight(const std::string& value, CSSComputedStyle& style) {
    if (value.find("bold") != std::string::npos || 
        value.find("700") != std::string::npos ||
        value.find("800") != std::string::npos ||
        value.find("900") != std::string::npos) {
        style.bold = true;
    }
}

void CSSProcessor::applyFontStyle(const std::string& value, CSSComputedStyle& style) {
    if (value.find("italic") != std::string::npos || 
        value.find("oblique") != std::string::npos) {
        style.italic = true;
    }
}

void CSSProcessor::applyTextDecoration(const std::string& value, CSSComputedStyle& style) {
    if (value.find("underline") != std::string::npos) {
        style.underline = true;
    }
    if (value.find("line-through") != std::string::npos) {
        style.strikethrough = true;
    }
}

void CSSProcessor::applyFontVariant(const std::string& value, CSSComputedStyle& style) {
    if (value.find("small-caps") != std::string::npos || 
        value.find("all-small-caps") != std::string::npos) {
        style.small_caps = true;
    }
}

void CSSProcessor::applyFontFamily(const std::string& value, CSSComputedStyle& style) {
    if (value.find("mono") != std::string::npos || 
        value.find("courier") != std::string::npos ||
        value.find("consolas") != std::string::npos) {
        style.monospace = true;
    }
}

void CSSProcessor::applyFontSize(const std::string& value, CSSComputedStyle& style) {
    if (value.find("xxx-large") != std::string::npos) {
        style.font_size = 2.5f;
    } else if (value == "xx-large") {
        style.font_size = 2.0f;
    } else if (value == "x-large") {
        style.font_size = 1.5f;
    } else if (value == "large" || value == "larger") {
        style.font_size = 1.2f;
    } else if (value == "medium") {
        style.font_size = 1.0f;
    } else if (value == "small" || value == "smaller" || value == "x-small") {
        style.font_size = 0.85f;
    } else if (value == "xx-small") {
        style.font_size = 0.6f;
    } else {
        float parsed = parseFloat(value);
        if (parsed > 0.0f) {
            if (value.find("em") != std::string::npos) {
                style.font_size = parsed;
            } else if (value.find("%") != std::string::npos) {
                style.font_size = parsed / 100.0f;
            } else if (value.find("pt") != std::string::npos) {
                style.font_size = parsed / 12.0f;
            } else if (value.find("px") != std::string::npos) {
                style.font_size = parsed / 16.0f;
            } else if (value.find("rem") != std::string::npos) {
                style.font_size = parsed;
            }
        }
    }
}

void CSSProcessor::applyLineHeight(const std::string& value, CSSComputedStyle& style) {
    float parsed = parseFloat(value);
    if (parsed > 0.0f) {
        style.line_height = parsed;
    }
}

void CSSProcessor::applyLetterSpacing(const std::string& value, CSSComputedStyle& style) {
    float parsed = parseFloat(value);
    style.letter_spacing = parsed;
}

void CSSProcessor::applyTextAlign(const std::string& value, CSSComputedStyle& style) {
    if (value.find("center") != std::string::npos) {
        style.text_align = TextAlign::Center;
    } else if (value.find("right") != std::string::npos) {
        style.text_align = TextAlign::Right;
    } else if (value.find("justify") != std::string::npos) {
        style.text_align = TextAlign::Justify;
    } else if (value.find("left") != std::string::npos) {
        style.text_align = TextAlign::Left;
    }
}

void CSSProcessor::applyDisplay(const std::string& value, CSSComputedStyle& style) {
    if (value.find("none") != std::string::npos) {
        style.display = CSSDisplay::None;
    } else if (value.find("block") != std::string::npos) {
        style.display = CSSDisplay::Block;
    } else if (value.find("inline-block") != std::string::npos) {
        style.display = CSSDisplay::InlineBlock;
    } else if (value.find("list-item") != std::string::npos) {
        style.display = CSSDisplay::ListItem;
    } else if (value.find("inline") != std::string::npos) {
        style.display = CSSDisplay::Inline;
    }
}

void CSSProcessor::applyVerticalAlign(const std::string& value, CSSComputedStyle& style) {
    if (value.find("super") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Super;
    } else if (value.find("sub") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Sub;
    } else if (value.find("top") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Top;
    } else if (value.find("middle") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Middle;
    } else if (value.find("bottom") != std::string::npos) {
        style.vertical_align = CSSVerticalAlign::Bottom;
    } else {
        style.vertical_align = CSSVerticalAlign::Baseline;
    }
}

void CSSProcessor::applyTextTransform(const std::string& value, CSSComputedStyle& style) {
    if (value.find("uppercase") != std::string::npos) {
        style.text_transform = CSSTextTransform::Uppercase;
    } else if (value.find("lowercase") != std::string::npos) {
        style.text_transform = CSSTextTransform::Lowercase;
    } else if (value.find("capitalize") != std::string::npos) {
        style.text_transform = CSSTextTransform::Capitalize;
    }
}

void CSSProcessor::applyWhiteSpace(const std::string& value, CSSComputedStyle& style) {
    if (value.find("pre-wrap") != std::string::npos) {
        style.white_space = CSSWhiteSpace::PreWrap;
    } else if (value.find("pre-line") != std::string::npos) {
        style.white_space = CSSWhiteSpace::PreLine;
    } else if (value.find("pre") != std::string::npos) {
        style.white_space = CSSWhiteSpace::Pre;
    } else if (value.find("nowrap") != std::string::npos) {
        style.white_space = CSSWhiteSpace::Nowrap;
    }
}

void CSSProcessor::applyMargin(const std::string& property, const std::string& value, 
                              CSSComputedStyle& style) {
    int length = parseLength(value);
    
    if (property == "margin-top") {
        style.margin_top = length;
    } else if (property == "margin-bottom") {
        style.margin_bottom = length;
    } else if (property == "margin-left") {
        style.margin_left = length;
    } else if (property == "margin-right") {
        style.margin_right = length;
    }
}

void CSSProcessor::applyPadding(const std::string& property, const std::string& value, 
                               CSSComputedStyle& style) {
    int length = parseLength(value);
    
    if (property == "padding-top") {
        style.padding_top = length;
    } else if (property == "padding-bottom") {
        style.padding_bottom = length;
    } else if (property == "padding-left") {
        style.padding_left = length;
    } else if (property == "padding-right") {
        style.padding_right = length;
    }
}

void CSSProcessor::applyPageBreak(const std::string& property, const std::string& value, 
                                 CSSComputedStyle& style) {
    bool is_always = value.find("always") != std::string::npos;
    bool is_avoid = value.find("avoid") != std::string::npos;
    
    if (property == "page-break-before" && is_always) {
        style.page_break_before = true;
    } else if (property == "page-break-after" && is_always) {
        style.page_break_after = true;
    } else if (property == "page-break-inside" && is_avoid) {
        style.page_break_inside_avoid = true;
    }
}

int CSSProcessor::parseLength(const std::string& value) {
    float parsed = parseFloat(value);
    
    if (value.find("rem") != std::string::npos) {
        return static_cast<int>(parsed * 16.0f);
    } else if (value.find("em") != std::string::npos) {
        return static_cast<int>(parsed * 16.0f);
    } else if (value.find("px") != std::string::npos) {
        return static_cast<int>(parsed);
    } else if (value.find("pt") != std::string::npos) {
        return static_cast<int>(parsed * 1.333f);
    }
    
    return static_cast<int>(parsed);
}

float CSSProcessor::parseFloat(const std::string& value) {
    try {
        size_t pos = 0;
        while (pos < value.length() && 
               (std::isdigit(value[pos]) || value[pos] == '.' || value[pos] == '-')) {
            pos++;
        }
        if (pos > 0) {
            return std::stof(value.substr(0, pos));
        }
    } catch (...) {
    }
    return 0.0f;
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