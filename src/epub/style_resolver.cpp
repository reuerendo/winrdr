#include "style_resolver.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace epub {

StyleResolver::StyleResolver() {}

void StyleResolver::addStylesheet(const std::string& css) {
<<<<<<< Updated upstream
    size_t pos = 0;
    
    while (pos < css.length()) {
        // Skip whitespace
        while (pos < css.length() && std::isspace(css[pos])) {
            pos++;
        }
        
        if (pos >= css.length()) break;
        
        // Find selector (everything before {)
=======
    parseStylesheet(css, rules_);
    LOG_DEBUG("Added stylesheet, total rules:", rules_.size());
}

void StyleResolver::clear() {
    rules_.clear();
}

void StyleResolver::resolveStyles(DocumentNode* document) {
    LOG_DEBUG("Resolving styles for document");
    
    ComputedStyle root_style;
    
    // Start cascade from root
    for (auto& child : document->children) {
        cascadeStyles(child.get(), root_style);
    }
    
    LOG_DEBUG("Style resolution complete");
}

void StyleResolver::cascadeStyles(DOMNode* node, ComputedStyle parent_style) {
    if (!node) return;
    
    if (node->getType() == NodeType::Element) {
        ElementNode* element = static_cast<ElementNode*>(node);
        
        ComputedStyle style = parent_style;
        
        // Step 1: Apply default/UA styles
        applyDefaultStyles(element, style);
        
        // Step 2: Apply matching CSS rules
        applyMatchingRules(element, style);
        
        // Step 3: Apply inline styles
        applyInlineStyle(element, style);
        
        // Step 4: Compute final values
        computeFinalStyle(style, parent_style);
        
        element->computed_style = style;
        
        // Cascade to children
        for (auto& child : element->children) {
            cascadeStyles(child.get(), style);
        }
    }
    else if (node->getType() == NodeType::Text) {
        // Text nodes inherit parent style
        node->computed_style = parent_style;
    }
}

void StyleResolver::applyDefaultStyles(ElementNode* element, ComputedStyle& style) {
    std::string tag = element->getTagName();
    
    // Block elements
    if (tag == "div" || tag == "p" || tag == "h1" || tag == "h2" || 
        tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" ||
        tag == "blockquote" || tag == "pre" || tag == "section" ||
        tag == "article" || tag == "header" || tag == "footer") {
        style.display = DisplayType::Block;
    }
    
    // Headings
    if (tag == "h1") {
        style.font_size_multiplier = 2.0f;
        style.bold = true;
        style.margin_top = 0.67f;
        style.margin_bottom = 0.67f;
    } else if (tag == "h2") {
        style.font_size_multiplier = 1.5f;
        style.bold = true;
        style.margin_top = 0.75f;
        style.margin_bottom = 0.75f;
    } else if (tag == "h3") {
        style.font_size_multiplier = 1.17f;
        style.bold = true;
        style.margin_top = 0.83f;
        style.margin_bottom = 0.83f;
    } else if (tag == "h4") {
        style.bold = true;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    } else if (tag == "h5") {
        style.font_size_multiplier = 0.83f;
        style.bold = true;
        style.margin_top = 1.5f;
        style.margin_bottom = 1.5f;
    } else if (tag == "h6") {
        style.font_size_multiplier = 0.67f;
        style.bold = true;
        style.margin_top = 2.33f;
        style.margin_bottom = 2.33f;
    }
    
    // Paragraph
    if (tag == "p") {
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    
    // Text formatting
    if (tag == "b" || tag == "strong") {
        style.bold = true;
    }
    if (tag == "i" || tag == "em" || tag == "cite") {
        style.italic = true;
    }
    if (tag == "u") {
        style.underline = true;
    }
    if (tag == "s" || tag == "strike" || tag == "del") {
        style.strikethrough = true;
    }
    if (tag == "code" || tag == "pre" || tag == "tt") {
        style.monospace = true;
    }
    if (tag == "small") {
        style.font_size_multiplier = 0.83f;
    }
    if (tag == "big") {
        style.font_size_multiplier = 1.17f;
    }
    
    // Subscript/superscript
    if (tag == "sub") {
        style.vertical_align = ComputedStyle::VerticalAlign::Sub;
        style.font_size_multiplier = 0.83f;
    }
    if (tag == "sup") {
        style.vertical_align = ComputedStyle::VerticalAlign::Super;
        style.font_size_multiplier = 0.83f;
    }
    
    // Blockquote
    if (tag == "blockquote") {
        style.margin_left = 1.5f;
        style.margin_right = 1.5f;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    
    // Pre
    if (tag == "pre") {
        style.white_space = ComputedStyle::WhiteSpace::Pre;
    }
    
    // Center
    if (tag == "center") {
        style.display = DisplayType::Block;
        style.text_align = ComputedStyle::TextAlign::Center;
    }
    
    // List items
    if (tag == "li") {
        style.display = DisplayType::ListItem;
        style.margin_left = 1.0f;
    }
    
    // Table elements
    if (tag == "table") {
        style.display = DisplayType::Table;
    }
    if (tag == "tr") {
        style.display = DisplayType::TableRow;
    }
    if (tag == "td" || tag == "th") {
        style.display = DisplayType::TableCell;
        style.padding_left = 0.5f;
        style.padding_right = 0.5f;
    }
    if (tag == "th") {
        style.bold = true;
    }
}

void StyleResolver::applyMatchingRules(ElementNode* element, ComputedStyle& style) {
    struct MatchedRule {
        const CSSRule* rule;
        int specificity;
        size_t order;
    };
    
    std::vector<MatchedRule> matched;
    
    for (size_t i = 0; i < rules_.size(); i++) {
        if (selectorMatches(rules_[i].selector, element)) {
            matched.push_back({&rules_[i], rules_[i].specificity, i});
        }
    }
    
    // Sort by specificity, then by order
    std::sort(matched.begin(), matched.end(),
        [](const MatchedRule& a, const MatchedRule& b) {
            if (a.specificity != b.specificity) {
                return a.specificity < b.specificity;
            }
            return a.order < b.order;
        });
    
    // Apply rules in order
    for (const auto& m : matched) {
        for (const auto& prop : m.rule->properties) {
            parseProperty(prop.first, prop.second, style);
        }
    }
}

void StyleResolver::applyInlineStyle(ElementNode* element, ComputedStyle& style) {
    std::string inline_style = element->getAttribute("style");
    if (inline_style.empty()) return;
    
    size_t pos = 0;
    while (pos < inline_style.length()) {
        size_t colon = inline_style.find(':', pos);
        if (colon == std::string::npos) break;
        
        std::string name = trim(inline_style.substr(pos, colon - pos));
        
        size_t semicolon = inline_style.find(';', colon);
        if (semicolon == std::string::npos) {
            semicolon = inline_style.length();
        }
        
        std::string value = trim(inline_style.substr(colon + 1, semicolon - colon - 1));
        
        parseProperty(name, value, style);
        
        pos = semicolon + 1;
    }
}

void StyleResolver::computeFinalStyle(ComputedStyle& style, const ComputedStyle& parent) {
    // Inherit properties that should be inherited
    if (!style.has_background) {
        style.background_color = parent.background_color;
    }
}

void StyleResolver::parseProperty(const std::string& name, const std::string& value,
                                  ComputedStyle& style) {
    std::string name_lower = toLowerCase(name);
    std::string value_lower = toLowerCase(value);
    
    if (name_lower == "display") {
        if (value_lower == "none") style.display = DisplayType::None;
        else if (value_lower == "block") style.display = DisplayType::Block;
        else if (value_lower == "inline") style.display = DisplayType::Inline;
        else if (value_lower == "inline-block") style.display = DisplayType::InlineBlock;
        else if (value_lower == "list-item") style.display = DisplayType::ListItem;
    }
    else if (name_lower == "font-weight") {
        if (value_lower == "bold" || value_lower == "bolder" ||
            value_lower == "700" || value_lower == "800" || value_lower == "900") {
            style.bold = true;
        }
    }
    else if (name_lower == "font-style") {
        if (value_lower == "italic" || value_lower == "oblique") {
            style.italic = true;
        }
    }
    else if (name_lower == "text-decoration") {
        if (value_lower.find("underline") != std::string::npos) {
            style.underline = true;
        }
        if (value_lower.find("line-through") != std::string::npos) {
            style.strikethrough = true;
        }
    }
    else if (name_lower == "font-family") {
        if (value_lower.find("monospace") != std::string::npos ||
            value_lower.find("courier") != std::string::npos) {
            style.monospace = true;
        }
    }
    else if (name_lower == "font-size") {
        float size = parseLength(value);
        if (size > 0) {
            style.font_size_multiplier = size;
        }
    }
    else if (name_lower == "text-align") {
        if (value_lower == "left") style.text_align = ComputedStyle::TextAlign::Left;
        else if (value_lower == "right") style.text_align = ComputedStyle::TextAlign::Right;
        else if (value_lower == "center") style.text_align = ComputedStyle::TextAlign::Center;
        else if (value_lower == "justify") style.text_align = ComputedStyle::TextAlign::Justify;
    }
    else if (name_lower == "vertical-align") {
        if (value_lower == "sub") style.vertical_align = ComputedStyle::VerticalAlign::Sub;
        else if (value_lower == "super") style.vertical_align = ComputedStyle::VerticalAlign::Super;
    }
    else if (name_lower == "margin-top") {
        style.margin_top = parseLength(value);
    }
    else if (name_lower == "margin-bottom") {
        style.margin_bottom = parseLength(value);
    }
    else if (name_lower == "margin-left") {
        style.margin_left = parseLength(value);
    }
    else if (name_lower == "margin-right") {
        style.margin_right = parseLength(value);
    }
    else if (name_lower == "padding-top") {
        style.padding_top = parseLength(value);
    }
    else if (name_lower == "padding-bottom") {
        style.padding_bottom = parseLength(value);
    }
    else if (name_lower == "color") {
        style.text_color = parseColor(value);
    }
    else if (name_lower == "background-color") {
        style.background_color = parseColor(value);
        style.has_background = true;
    }
    else if (name_lower == "line-height") {
        float lh = parseLength(value);
        if (lh > 0) {
            style.line_height = lh;
        }
    }
}

bool StyleResolver::selectorMatches(const std::string& selector, ElementNode* element) {
    std::string sel = trim(selector);
    
    // Universal selector
    if (sel == "*") return true;
    
    // Tag selector
    if (sel == element->getTagName()) return true;
    
    // Class selector
    if (sel[0] == '.') {
        std::string class_name = sel.substr(1);
        std::string elem_class = element->getAttribute("class");
        if (elem_class.find(class_name) != std::string::npos) {
            return true;
        }
    }
    
    // ID selector
    if (sel[0] == '#') {
        std::string id = sel.substr(1);
        if (element->getAttribute("id") == id) {
            return true;
        }
    }
    
    return false;
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    int spec = 0;
    
    // ID: 100
    if (selector.find('#') != std::string::npos) {
        spec += 100;
    }
    
    // Class: 10
    if (selector.find('.') != std::string::npos) {
        spec += 10;
    }
    
    // Tag: 1
    if (selector[0] != '.' && selector[0] != '#') {
        spec += 1;
    }
    
    return spec;
}

ComputedStyle::Color StyleResolver::parseColor(const std::string& color_str) {
    std::string color = toLowerCase(trim(color_str));
    
    // Named colors
    if (color == "black") return ComputedStyle::Color(0, 0, 0);
    if (color == "white") return ComputedStyle::Color(255, 255, 255);
    if (color == "red") return ComputedStyle::Color(255, 0, 0);
    if (color == "green") return ComputedStyle::Color(0, 128, 0);
    if (color == "blue") return ComputedStyle::Color(0, 0, 255);
    
    // Hex color
    if (color[0] == '#') {
        std::string hex = color.substr(1);
        if (hex.length() == 6) {
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return ComputedStyle::Color(r, g, b);
        }
    }
    
    return ComputedStyle::Color(0, 0, 0);
}

float StyleResolver::parseLength(const std::string& length_str) {
    std::string value = trim(length_str);
    
    if (value.empty()) return 0;
    
    // Remove units
    if (value.find("em") != std::string::npos) {
        value = value.substr(0, value.find("em"));
        return std::stof(value);
    }
    if (value.find("rem") != std::string::npos) {
        value = value.substr(0, value.find("rem"));
        return std::stof(value);
    }
    if (value.find("px") != std::string::npos) {
        value = value.substr(0, value.find("px"));
        return std::stof(value) / 16.0f; // Convert to em
    }
    if (value.find("%") != std::string::npos) {
        value = value.substr(0, value.find("%"));
        return std::stof(value) / 100.0f;
    }
    
    // Just a number
    try {
        return std::stof(value);
    } catch (...) {
        return 0;
    }
}

void StyleResolver::parseStylesheet(const std::string& css, std::vector<CSSRule>& rules) {
    size_t pos = 0;
    
    while (pos < css.length()) {
>>>>>>> Stashed changes
        size_t brace_open = css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(css.substr(pos, brace_open - pos));
        
        // Find declarations (everything between { and })
        size_t brace_close = css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string properties = css.substr(brace_open + 1, brace_close - brace_open - 1);
        
<<<<<<< Updated upstream
        CSSRule rule;
        rule.selector = selector;
        rule.specificity = calculateSpecificity(selector);
        parseDeclarations(declarations_str, rule.declarations);
        
        if (!rule.declarations.empty()) {
            rules_.push_back(rule);
        }
        
        pos = brace_close + 1;
    }
    
    // Sort by specificity (lower specificity first, so higher overwrites)
    std::sort(rules_.begin(), rules_.end(),
             [](const CSSRule& a, const CSSRule& b) {
                 return a.specificity < b.specificity;
             });
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
=======
        parseRule(selector, properties, rules);
        
        pos = brace_close + 1;
    }
}

void StyleResolver::parseRule(const std::string& selector_str, const std::string& properties_str,
                              std::vector<CSSRule>& rules) {
    CSSRule rule;
    rule.selector = toLowerCase(trim(selector_str));
    rule.specificity = calculateSpecificity(rule.selector);
    
    size_t pos = 0;
    while (pos < properties_str.length()) {
        size_t colon = properties_str.find(':', pos);
>>>>>>> Stashed changes
        if (colon == std::string::npos) break;
        
        std::string name = toLowerCase(trim(properties_str.substr(pos, colon - pos)));
        
<<<<<<< Updated upstream
        // Find value
        size_t semicolon = declarations_str.find(';', colon);
=======
        size_t semicolon = properties_str.find(';', colon);
>>>>>>> Stashed changes
        if (semicolon == std::string::npos) {
            semicolon = properties_str.length();
        }
        
        std::string value = trim(properties_str.substr(colon + 1, semicolon - colon - 1));
        
        if (!name.empty() && !value.empty()) {
            rule.properties[name] = value;
        }
        
        pos = semicolon + 1;
    }
<<<<<<< Updated upstream
}

void StyleResolver::resolveStyles(DocumentNode* document) {
    // Traverse DOM tree
    std::vector<DOMNode*> queue;
    queue.push_back(document);
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        // Apply styles in order: default -> CSS rules -> inline -> inheritance
        applyDefaultStyles(node);
        applyCSSRules(node);
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            applyInlineStyle(element);
        }
        
        inheritStyles(node);
        
        // Add children to queue
        for (auto& child : node->children) {
            queue.push_back(child.get());
        }
    }
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

void StyleResolver::applyCSSRules(DOMNode* node) {
    if (node->getType() != NodeType::Element) return;
    
    ElementNode* element = static_cast<ElementNode*>(node);
    
    // Apply matching CSS rules in order of specificity
    for (const CSSRule& rule : rules_) {
        if (matchesSelector(element, rule.selector)) {
            for (const auto& decl : rule.declarations) {
                applyDeclaration(decl.first, decl.second, element->computed_style);
            }
        }
    }
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
    
    // Inherit text properties if not explicitly set
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
    
    // Simple selector matching (tag, class, id)
    // This is simplified - real CSS selector matching is much more complex
    
    if (sel.empty()) return false;
    
    // Universal selector
    if (sel == "*") return true;
    
    // Class selector
    if (sel[0] == '.') {
        std::string class_name = sel.substr(1);
        std::string element_class = element->getAttribute("class");
        
        // Check if class_name is in element_class (space-separated list)
        size_t pos = element_class.find(class_name);
        if (pos != std::string::npos) {
            bool start_ok = (pos == 0 || std::isspace(element_class[pos - 1]));
            bool end_ok = (pos + class_name.length() == element_class.length() ||
                          std::isspace(element_class[pos + class_name.length()]));
            if (start_ok && end_ok) return true;
        }
        return false;
    }
    
    // ID selector
    if (sel[0] == '#') {
        std::string id = sel.substr(1);
        return element->getAttribute("id") == id;
    }
    
    // Tag selector
    return element->getTagName() == toLowerCase(sel);
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    // Simplified specificity: count IDs, classes, and elements
    // Real CSS specificity is more complex
    
    int specificity = 0;
    
    for (char c : selector) {
        if (c == '#') specificity += 100;
        else if (c == '.') specificity += 10;
    }
    
    // If no class or ID, it's a tag selector
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
=======
    
    if (!rule.properties.empty()) {
        rules.push_back(rule);
    }
>>>>>>> Stashed changes
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

std::string StyleResolver::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace epub