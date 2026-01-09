#include "style_resolver.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

namespace epub {

StyleResolver::StyleResolver() {}

void StyleResolver::addStylesheet(const std::string& css) {
    // Remove CSS comments first
    std::string cleaned_css = removeComments(css);
    
    size_t pos = 0;
    int rules_parsed = 0;
    
    while (pos < cleaned_css.length()) {
        // Skip whitespace
        while (pos < cleaned_css.length() && std::isspace(cleaned_css[pos])) {
            pos++;
        }
        
        if (pos >= cleaned_css.length()) break;
        
        // Find selector (everything before {)
        size_t brace_open = cleaned_css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(cleaned_css.substr(pos, brace_open - pos));
        
        // Find declarations (everything between { and })
        size_t brace_close = cleaned_css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string declarations_str = cleaned_css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        CSSRule rule;
        rule.selector = selector;
        rule.specificity = calculateSpecificity(selector);
        parseDeclarations(declarations_str, rule.declarations);
        
        if (!rule.declarations.empty()) {
            rules_.push_back(rule);
            rules_parsed++;
            LOG_DEBUG("CSS rule:", selector, "declarations:", rule.declarations.size());
        }
        
        pos = brace_close + 1;
    }
    
    // Sort by specificity (lower specificity first, so higher overwrites)
    std::sort(rules_.begin(), rules_.end(),
             [](const CSSRule& a, const CSSRule& b) {
                 return a.specificity < b.specificity;
             });
    
    LOG_INFO("Parsed CSS rules:", rules_parsed, "total rules:", rules_.size());
}

std::string StyleResolver::removeComments(const std::string& css) {
    std::string result;
    size_t pos = 0;
    
    while (pos < css.length()) {
        // Check for comment start
        if (pos + 1 < css.length() && css[pos] == '/' && css[pos + 1] == '*') {
            // Find comment end
            size_t comment_end = css.find("*/", pos + 2);
            if (comment_end != std::string::npos) {
                pos = comment_end + 2;
            } else {
                // Unclosed comment - skip to end
                break;
            }
        } else {
            result += css[pos];
            pos++;
        }
    }
    
    return result;
}

void StyleResolver::clear() {
    rules_.clear();
}

void StyleResolver::parseDeclarations(const std::string& declarations_str,
                                     std::unordered_map<std::string, std::string>& out) {
    size_t pos = 0;
    
    while (pos < declarations_str.length()) {
        // Find property name
        size_t colon = declarations_str.find(':', pos);
        if (colon == std::string::npos) break;
        
        std::string property = toLowerCase(trim(declarations_str.substr(pos, colon - pos)));
        
        // Find value
        size_t semicolon = declarations_str.find(';', colon);
        if (semicolon == std::string::npos) {
            semicolon = declarations_str.length();
        }
        
        std::string value = trim(declarations_str.substr(colon + 1, semicolon - colon - 1));
        
        if (!property.empty() && !value.empty()) {
            out[property] = value;
        }
        
        pos = semicolon + 1;
    }
}

void StyleResolver::resolveStyles(DocumentNode* document) {
    // Traverse DOM tree
    std::vector<DOMNode*> queue;
    queue.push_back(document);
    
    int elements_processed = 0;
    int styles_applied = 0;
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        // Apply styles in order: default -> CSS rules -> inline -> inheritance
        applyDefaultStyles(node);
        
        int applied = applyCSSRules(node);
        styles_applied += applied;
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            applyInlineStyle(element);
            elements_processed++;
        }
        
        inheritStyles(node);
        
        // Add children to queue
        for (auto& child : node->children) {
            queue.push_back(child.get());
        }
    }
    
    LOG_DEBUG("Style resolution: elements:", elements_processed, "CSS rules applied:", styles_applied);
}

void StyleResolver::applyDefaultStyles(DOMNode* node) {
    if (node->getType() != NodeType::Element) {
        node->computed_style.display = DisplayType::Inline;
        return;
    }
    
    ElementNode* element = static_cast<ElementNode*>(node);
    const std::string& tag = element->getTagName();
    ComputedStyle& style = element->computed_style;
    
    // Block elements
    if (tag == "div" || tag == "p" || tag == "section" || tag == "article" ||
        tag == "aside" || tag == "header" || tag == "footer" || tag == "main" ||
        tag == "nav" || tag == "blockquote" || tag == "pre" || tag == "figure" ||
        tag == "figcaption" || tag == "address" || tag == "center") {
        style.display = DisplayType::Block;
        style.margin_bottom = 1.0f;
    }
    
    // Headings
    else if (tag == "h1" || tag == "h2" || tag == "h3" || 
             tag == "h4" || tag == "h5" || tag == "h6") {
        style.display = DisplayType::Block;
        style.bold = true;
        
        if (tag == "h1") style.font_size_multiplier = 2.0f;
        else if (tag == "h2") style.font_size_multiplier = 1.5f;
        else if (tag == "h3") style.font_size_multiplier = 1.17f;
        else if (tag == "h4") style.font_size_multiplier = 1.0f;
        else if (tag == "h5") style.font_size_multiplier = 0.83f;
        else if (tag == "h6") style.font_size_multiplier = 0.67f;
        
        style.margin_top = 0.67f;
        style.margin_bottom = 0.67f;
    }
    
    // Lists
    else if (tag == "ul" || tag == "ol") {
        style.display = DisplayType::Block;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
        style.padding_left = 40.0f;
    }
    else if (tag == "li") {
        style.display = DisplayType::ListItem;
    }
    
    // Text formatting
    else if (tag == "b" || tag == "strong") {
        style.display = DisplayType::Inline;
        style.bold = true;
    }
    else if (tag == "i" || tag == "em" || tag == "cite" || tag == "var") {
        style.display = DisplayType::Inline;
        style.italic = true;
    }
    else if (tag == "u" || tag == "ins") {
        style.display = DisplayType::Inline;
        style.underline = true;
    }
    else if (tag == "s" || tag == "strike" || tag == "del") {
        style.display = DisplayType::Inline;
        style.strikethrough = true;
    }
    else if (tag == "code" || tag == "kbd" || tag == "samp" || tag == "tt") {
        style.display = DisplayType::Inline;
        style.monospace = true;
    }
    else if (tag == "pre") {
        style.display = DisplayType::Block;
        style.monospace = true;
        style.white_space = ComputedStyle::WhiteSpace::Pre;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    else if (tag == "small") {
        style.display = DisplayType::Inline;
        style.font_size_multiplier = 0.85f;
    }
    else if (tag == "big") {
        style.display = DisplayType::Inline;
        style.font_size_multiplier = 1.17f;
    }
    else if (tag == "sub") {
        style.display = DisplayType::Inline;
        style.vertical_align = ComputedStyle::VerticalAlign::Sub;
        style.font_size_multiplier = 0.83f;
    }
    else if (tag == "sup") {
        style.display = DisplayType::Inline;
        style.vertical_align = ComputedStyle::VerticalAlign::Super;
        style.font_size_multiplier = 0.83f;
    }
    
    // Links
    else if (tag == "a") {
        style.display = DisplayType::Inline;
        style.underline = true;
        style.text_color = ComputedStyle::Color(0, 0, 255);
    }
    
    // Quotes
    else if (tag == "blockquote") {
        style.display = DisplayType::Block;
        style.margin_left = 40.0f;
        style.margin_right = 40.0f;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    
    // Tables
    else if (tag == "table") {
        style.display = DisplayType::Table;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    else if (tag == "tr") {
        style.display = DisplayType::TableRow;
    }
    else if (tag == "td" || tag == "th") {
        style.display = DisplayType::TableCell;
        style.padding_left = 2.0f;
        style.padding_right = 2.0f;
        if (tag == "th") {
            style.bold = true;
        }
    }
    
    // Horizontal rule
    else if (tag == "hr") {
        style.display = DisplayType::Block;
        style.margin_top = 0.5f;
        style.margin_bottom = 0.5f;
    }
    
    // Line break
    else if (tag == "br") {
        style.display = DisplayType::Inline;
    }
    
    // Hidden elements
    else if (tag == "script" || tag == "style" || tag == "noscript" ||
             tag == "head" || tag == "title" || tag == "meta" || tag == "link") {
        style.display = DisplayType::None;
    }
    
    // Default: inline
    else {
        style.display = DisplayType::Inline;
    }
}

int StyleResolver::applyCSSRules(DOMNode* node) {
    if (node->getType() != NodeType::Element) return 0;
    
    ElementNode* element = static_cast<ElementNode*>(node);
    int applied_count = 0;
    
    // Apply matching CSS rules in order of specificity
    for (const CSSRule& rule : rules_) {
        if (matchesSelector(element, rule.selector)) {
            for (const auto& decl : rule.declarations) {
                applyDeclaration(decl.first, decl.second, element->computed_style);
            }
            applied_count++;
        }
    }
    
    return applied_count;
}

void StyleResolver::applyInlineStyle(ElementNode* element) {
    std::string inline_style = element->getAttribute("style");
    if (inline_style.empty()) return;
    
    std::unordered_map<std::string, std::string> declarations;
    parseDeclarations(inline_style, declarations);
    
    for (const auto& decl : declarations) {
        applyDeclaration(decl.first, decl.second, element->computed_style);
    }
}

void StyleResolver::inheritStyles(DOMNode* node) {
    if (!node->parent) return;
    
    ComputedStyle& style = node->computed_style;
    ComputedStyle& parent_style = node->parent->computed_style;
    
    // Inherit font properties if not explicitly set
    // (This is simplified - in real browser, tracking "set" vs "inherited" is complex)
    
    // Inherit font properties
    if (style.font_size_multiplier == 1.0f && parent_style.font_size_multiplier != 1.0f) {
        style.font_size_multiplier = parent_style.font_size_multiplier;
    }
    
    // Inherit text color
    if (style.text_color.r == 0 && style.text_color.g == 0 && style.text_color.b == 0) {
        if (parent_style.text_color.r != 0 || parent_style.text_color.g != 0 || 
            parent_style.text_color.b != 0) {
            style.text_color = parent_style.text_color;
        }
    }
}

bool StyleResolver::matchesSelector(ElementNode* element, const std::string& selector) {
    std::string sel = trim(selector);
    
    if (sel.empty()) return false;
    
    // Universal selector
    if (sel == "*") return true;
    
    // Parse compound selector (e.g., "p.indent", "div#main", "span.bold.italic")
    std::string tag_part;
    std::vector<std::string> classes;
    std::string id_part;
    
    size_t pos = 0;
    
    // Extract tag name (if present)
    if (sel[0] != '.' && sel[0] != '#') {
        while (pos < sel.length() && sel[pos] != '.' && sel[pos] != '#') {
            tag_part += sel[pos];
            pos++;
        }
    }
    
    // Extract classes and ID
    while (pos < sel.length()) {
        if (sel[pos] == '.') {
            pos++;
            std::string class_name;
            while (pos < sel.length() && sel[pos] != '.' && sel[pos] != '#') {
                class_name += sel[pos];
                pos++;
            }
            if (!class_name.empty()) {
                classes.push_back(class_name);
            }
        } else if (sel[pos] == '#') {
            pos++;
            while (pos < sel.length() && sel[pos] != '.' && sel[pos] != '#') {
                id_part += sel[pos];
                pos++;
            }
        } else {
            pos++;
        }
    }
    
    // Check tag name
    if (!tag_part.empty()) {
        if (element->getTagName() != toLowerCase(tag_part)) {
            return false;
        }
    }
    
    // Check ID
    if (!id_part.empty()) {
        if (element->getAttribute("id") != id_part) {
            return false;
        }
    }
    
    // Check classes
    if (!classes.empty()) {
        std::string element_class = element->getAttribute("class");
        
        for (const std::string& class_name : classes) {
            bool found = false;
            
            // Check if class_name is in element_class (space-separated list)
            size_t search_pos = element_class.find(class_name);
            if (search_pos != std::string::npos) {
                bool start_ok = (search_pos == 0 || std::isspace(element_class[search_pos - 1]));
                bool end_ok = (search_pos + class_name.length() == element_class.length() ||
                              std::isspace(element_class[search_pos + class_name.length()]));
                if (start_ok && end_ok) {
                    found = true;
                }
            }
            
            if (!found) {
                return false;
            }
        }
    }
    
    return true;
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    // Simplified specificity: count IDs (100), classes (10), and elements (1)
    // Real CSS specificity is more complex
    
    int specificity = 0;
    
    // Count IDs
    for (char c : selector) {
        if (c == '#') specificity += 100;
    }
    
    // Count classes
    for (char c : selector) {
        if (c == '.') specificity += 10;
    }
    
    // If no class or ID, check if it's a tag selector
    if (specificity == 0 && !selector.empty() && selector[0] != '*') {
        specificity = 1;
    }
    
    return specificity;
}

void StyleResolver::applyDeclaration(const std::string& property, const std::string& value,
                                    ComputedStyle& style) {
    std::string prop = toLowerCase(trim(property));
    std::string val = toLowerCase(trim(value));
    
    if (prop == "display") {
        style.display = parseDisplay(val);
    }
    else if (prop == "font-weight") {
        if (val == "bold" || val == "bolder" || val == "700" || 
            val == "800" || val == "900") {
            style.bold = true;
        } else if (val == "normal" || val == "400") {
            style.bold = false;
        }
    }
    else if (prop == "font-style") {
        if (val == "italic" || val == "oblique") {
            style.italic = true;
        } else if (val == "normal") {
            style.italic = false;
        }
    }
    else if (prop == "text-decoration" || prop == "text-decoration-line") {
        if (val.find("underline") != std::string::npos) {
            style.underline = true;
        }
        if (val.find("line-through") != std::string::npos) {
            style.strikethrough = true;
        }
        if (val == "none") {
            style.underline = false;
            style.strikethrough = false;
        }
    }
    else if (prop == "font-family") {
        if (val.find("monospace") != std::string::npos ||
            val.find("courier") != std::string::npos ||
            val.find("mono") != std::string::npos ||
            val.find("consolas") != std::string::npos) {
            style.monospace = true;
        }
    }
    else if (prop == "font-size") {
        float multiplier = parseLength(val, 1.0f);
        if (multiplier > 0) {
            style.font_size_multiplier = multiplier;
        }
    }
    else if (prop == "text-align") {
        style.text_align = parseTextAlign(val);
    }
    else if (prop == "vertical-align") {
        style.vertical_align = parseVerticalAlign(val);
    }
    else if (prop == "white-space") {
        style.white_space = parseWhiteSpace(val);
    }
    else if (prop == "color") {
        style.text_color = parseColor(val);
    }
    else if (prop == "background-color" || prop == "background") {
        style.background_color = parseColor(val);
        style.has_background = true;
    }
    else if (prop == "margin-top") {
        style.margin_top = parseLength(val, 16.0f);
    }
    else if (prop == "margin-bottom") {
        style.margin_bottom = parseLength(val, 16.0f);
    }
    else if (prop == "margin-left") {
        style.margin_left = parseLength(val, 16.0f);
    }
    else if (prop == "margin-right") {
        style.margin_right = parseLength(val, 16.0f);
    }
    else if (prop == "margin") {
        float m = parseLength(val, 16.0f);
        style.margin_top = style.margin_bottom = style.margin_left = style.margin_right = m;
    }
    else if (prop == "padding-top") {
        style.padding_top = parseLength(val, 16.0f);
    }
    else if (prop == "padding-bottom") {
        style.padding_bottom = parseLength(val, 16.0f);
    }
    else if (prop == "padding-left") {
        style.padding_left = parseLength(val, 16.0f);
    }
    else if (prop == "padding-right") {
        style.padding_right = parseLength(val, 16.0f);
    }
    else if (prop == "padding") {
        float p = parseLength(val, 16.0f);
        style.padding_top = style.padding_bottom = style.padding_left = style.padding_right = p;
    }
    else if (prop == "line-height") {
        style.line_height = parseLength(val, 1.2f);
    }
    else if (prop == "list-style-type" || prop == "list-style") {
        if (val == "none") {
            style.list_style = ComputedStyle::ListStyleType::None;
        } else if (val == "disc") {
            style.list_style = ComputedStyle::ListStyleType::Disc;
        } else if (val == "circle") {
            style.list_style = ComputedStyle::ListStyleType::Circle;
        } else if (val == "square") {
            style.list_style = ComputedStyle::ListStyleType::Square;
        } else if (val == "decimal") {
            style.list_style = ComputedStyle::ListStyleType::Decimal;
        }
    }
    else if (prop == "text-indent") {
        style.padding_left = parseLength(val, 16.0f);
    }
}

DisplayType StyleResolver::parseDisplay(const std::string& value) {
    if (value == "block") return DisplayType::Block;
    if (value == "inline") return DisplayType::Inline;
    if (value == "inline-block") return DisplayType::InlineBlock;
    if (value == "none") return DisplayType::None;
    if (value == "list-item") return DisplayType::ListItem;
    if (value == "table") return DisplayType::Table;
    if (value == "table-row") return DisplayType::TableRow;
    if (value == "table-cell") return DisplayType::TableCell;
    return DisplayType::Inline;
}

ComputedStyle::TextAlign StyleResolver::parseTextAlign(const std::string& value) {
    if (value == "left") return ComputedStyle::TextAlign::Left;
    if (value == "right") return ComputedStyle::TextAlign::Right;
    if (value == "center") return ComputedStyle::TextAlign::Center;
    if (value == "justify") return ComputedStyle::TextAlign::Justify;
    return ComputedStyle::TextAlign::Left;
}

ComputedStyle::VerticalAlign StyleResolver::parseVerticalAlign(const std::string& value) {
    if (value == "sub" || value == "subscript") return ComputedStyle::VerticalAlign::Sub;
    if (value == "super" || value == "sup" || value == "superscript") {
        return ComputedStyle::VerticalAlign::Super;
    }
    return ComputedStyle::VerticalAlign::Baseline;
}

ComputedStyle::WhiteSpace StyleResolver::parseWhiteSpace(const std::string& value) {
    if (value == "pre") return ComputedStyle::WhiteSpace::Pre;
    if (value == "nowrap") return ComputedStyle::WhiteSpace::Nowrap;
    if (value == "pre-wrap") return ComputedStyle::WhiteSpace::PreWrap;
    return ComputedStyle::WhiteSpace::Normal;
}

ComputedStyle::Color StyleResolver::parseColor(const std::string& value) {
    // Named colors
    static const std::unordered_map<std::string, ComputedStyle::Color> named_colors = {
        {"black", {0, 0, 0}},
        {"white", {255, 255, 255}},
        {"red", {255, 0, 0}},
        {"green", {0, 128, 0}},
        {"blue", {0, 0, 255}},
        {"yellow", {255, 255, 0}},
        {"cyan", {0, 255, 255}},
        {"magenta", {255, 0, 255}},
        {"gray", {128, 128, 128}},
        {"grey", {128, 128, 128}}
    };
    
    auto it = named_colors.find(value);
    if (it != named_colors.end()) {
        return it->second;
    }
    
    // Hex colors
    if (!value.empty() && value[0] == '#') {
        std::string hex = value.substr(1);
        
        if (hex.length() == 3) {
            // #RGB -> #RRGGBB
            hex = std::string() + hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
        }
        
        if (hex.length() == 6) {
            unsigned int color_val = std::stoul(hex, nullptr, 16);
            return ComputedStyle::Color(
                (color_val >> 16) & 0xFF,
                (color_val >> 8) & 0xFF,
                color_val & 0xFF
            );
        }
    }
    
    // rgb(r, g, b)
    if (value.find("rgb(") == 0) {
        size_t start = 4;
        size_t end = value.find(')', start);
        if (end != std::string::npos) {
            std::string rgb = value.substr(start, end - start);
            int r = 0, g = 0, b = 0;
            char comma;
            std::istringstream iss(rgb);
            iss >> r >> comma >> g >> comma >> b;
            return ComputedStyle::Color(
                std::max(0, std::min(255, r)),
                std::max(0, std::min(255, g)),
                std::max(0, std::min(255, b))
            );
        }
    }
    
    return ComputedStyle::Color(0, 0, 0);
}

float StyleResolver::parseLength(const std::string& value, float base_size) {
    if (value.empty()) return 0.0f;
    
    // Extract number
    float num = 0.0f;
    size_t unit_pos = 0;
    
    try {
        num = std::stof(value, &unit_pos);
    } catch (...) {
        return 0.0f;
    }
    
    // Extract unit
    std::string unit = trim(value.substr(unit_pos));
    
    if (unit.empty() || unit == "px") {
        return num;
    } else if (unit == "em") {
        return num * base_size;
    } else if (unit == "rem") {
        return num * 16.0f;
    } else if (unit == "%") {
        return num * base_size / 100.0f;
    } else if (unit == "pt") {
        return num * 1.333f;
    }
    
    return num;
}

std::string StyleResolver::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string StyleResolver::trim(const std::string& str) {
    size_t start = 0;
    while (start < str.length() && std::isspace(str[start])) {
        start++;
    }
    
    size_t end = str.length();
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    
    return str.substr(start, end - start);
}

} // namespace epub