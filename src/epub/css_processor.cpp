#include "css_processor.h"
#include "../utils/logger.h"
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>
#include <lexbor/dom/interfaces/element.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cctype>
#include <fstream>

namespace epub {

CSSProcessor::CSSProcessor() 
    : css_memory_(nullptr)
    , css_parser_(nullptr)
    , selectors_(nullptr)
    , document_(nullptr)
{
    css_memory_ = lxb_css_memory_create();
    lxb_css_memory_init(css_memory_, 128);
    
    css_parser_ = lxb_css_parser_create();
    lxb_css_parser_init(css_parser_, nullptr);
    
    selectors_ = lxb_selectors_create();
    lxb_selectors_init(selectors_);
}

CSSProcessor::~CSSProcessor() {
    clear();
    
    if (selectors_) {
        lxb_selectors_destroy(selectors_, true);
    }
    
    if (css_parser_) {
        lxb_css_parser_destroy(css_parser_, true);
    }
    
    if (css_memory_) {
        lxb_css_memory_destroy(css_memory_, true);
    }
}

void CSSProcessor::clear() {
    for (auto* stylesheet : stylesheets_) {
        if (stylesheet) {
            lxb_css_stylesheet_destroy(stylesheet, true);
        }
    }
    stylesheets_.clear();
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
    
    lxb_css_stylesheet_t* stylesheet = lxb_css_stylesheet_create(css_memory_);
    if (!stylesheet) {
        LOG_ERROR("Failed to create CSS stylesheet");
        return false;
    }
    
    lxb_status_t status = lxb_css_stylesheet_parse(stylesheet, 
        reinterpret_cast<const lxb_char_t*>(css.c_str()), css.length());
    
    if (status != LXB_STATUS_OK) {
        LOG_WARNING("CSS parse had errors, but continuing");
    }
    
    stylesheets_.push_back(stylesheet);
    
    lxb_css_rule_list_t* rules = lxb_css_stylesheet_rules(stylesheet);
    if (rules) {
        size_t rules_count = lxb_css_rule_list_length(rules);
        LOG_INFO("Parsed", rules_count, "CSS rules");
    }
    
    return true;
}

CSSComputedStyle CSSProcessor::computeStyle(lxb_dom_node_t* node) {
    CSSComputedStyle style;
    
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return style;
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    std::unordered_map<std::string, PropertyValue> matched_properties;
    
    for (auto* stylesheet : stylesheets_) {
        lxb_css_rule_list_t* rules = lxb_css_stylesheet_rules(stylesheet);
        if (!rules) continue;
        
        size_t rules_count = lxb_css_rule_list_length(rules);
        
        for (size_t i = 0; i < rules_count; i++) {
            lxb_css_rule_t* rule = lxb_css_rule_list_at(rules, i);
            if (!rule || rule->type != LXB_CSS_RULE_STYLE) {
                continue;
            }
            
            lxb_css_rule_style_t* style_rule = lxb_css_rule_style(rule);
            lxb_css_selector_list_t* selector_list = lxb_css_rule_style_selector(style_rule);
            lxb_css_rule_declaration_list_t* declarations = lxb_css_rule_style_declarations(style_rule);
            
            if (!selector_list || !declarations) {
                continue;
            }
            
            for (size_t s = 0; s < lxb_css_selector_list_length(selector_list); s++) {
                lxb_css_selector_t* selector = lxb_css_selector_list_at(selector_list, s);
                if (!selector) continue;
                
                lxb_status_t match_status = lxb_selectors_match(selectors_, element, selector);
                
                if (match_status == LXB_STATUS_OK) {
                    int specificity = calculateSpecificity(selector);
                    
                    for (size_t d = 0; d < lxb_css_rule_declaration_list_length(declarations); d++) {
                        lxb_css_rule_declaration_t* decl = lxb_css_rule_declaration_list_at(declarations, d);
                        if (!decl) continue;
                        
                        size_t name_len = 0;
                        const lxb_char_t* name = lxb_css_rule_declaration_name(decl, &name_len);
                        
                        size_t value_len = 0;
                        const lxb_char_t* value = lxb_css_rule_declaration_value_serialize(decl, &value_len);
                        
                        if (name && value && name_len > 0 && value_len > 0) {
                            std::string prop_name(reinterpret_cast<const char*>(name), name_len);
                            std::string prop_value(reinterpret_cast<const char*>(value), value_len);
                            
                            auto it = matched_properties.find(prop_name);
                            if (it == matched_properties.end() || it->second.specificity < specificity) {
                                PropertyValue pv;
                                pv.value = prop_value;
                                pv.specificity = specificity;
                                matched_properties[prop_name] = pv;
                            }
                        }
                    }
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

int CSSProcessor::calculateSpecificity(lxb_css_selector_t* selector) {
    int specificity = 0;
    
    lxb_css_selector_t* current = selector;
    while (current) {
        switch (current->type) {
            case LXB_CSS_SELECTOR_TYPE_ID:
                specificity += 100;
                break;
            case LXB_CSS_SELECTOR_TYPE_CLASS:
            case LXB_CSS_SELECTOR_TYPE_ATTRIBUTE:
            case LXB_CSS_SELECTOR_TYPE_PSEUDO_CLASS:
            case LXB_CSS_SELECTOR_TYPE_PSEUDO_CLASS_FUNCTION:
                specificity += 10;
                break;
            case LXB_CSS_SELECTOR_TYPE_ELEMENT:
            case LXB_CSS_SELECTOR_TYPE_PSEUDO_ELEMENT:
            case LXB_CSS_SELECTOR_TYPE_PSEUDO_ELEMENT_FUNCTION:
                specificity += 1;
                break;
            default:
                break;
        }
        
        current = lxb_css_selector_next(current);
    }
    
    return specificity;
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

size_t CSSProcessor::getRulesCount() const {
    size_t total = 0;
    for (auto* stylesheet : stylesheets_) {
        lxb_css_rule_list_t* rules = lxb_css_stylesheet_rules(stylesheet);
        if (rules) {
            total += lxb_css_rule_list_length(rules);
        }
    }
    return total;
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