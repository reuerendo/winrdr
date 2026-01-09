#include "style_resolver.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

namespace epub {

StyleResolver::StyleResolver() {}

void StyleResolver::addStylesheet(const std::string& css) {
    LOG_INFO("Adding stylesheet, size:", css.length());
    
    size_t pos = 0;
    int rules_parsed = 0;
    
    while (pos < css.length()) {
        // Skip whitespace and comments
        pos = skipWhitespaceAndComments(css, pos);
        if (pos >= css.length()) break;
        
        // Find selector (everything before {)
        size_t brace_open = css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(css.substr(pos, brace_open - pos));
        
        // Find declarations (everything between { and })
        size_t brace_close = css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string declarations_str = css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        // Handle comma-separated selectors (e.g., "h1, h2, h3")
        std::vector<std::string> selectors = splitSelectors(selector);
        
        for (const std::string& sel : selectors) {
            CSSRule rule;
            rule.selector = trim(sel);
            rule.specificity = calculateSpecificity(rule.selector);
            parseDeclarations(declarations_str, rule.declarations);
            
            if (!rule.declarations.empty()) {
                rules_.push_back(rule);
                rules_parsed++;
                
                // Log first few rules to debug
                if (rules_parsed <= 10) {
                    LOG_DEBUG("CSS rule:", rule.selector, "->", rule.declarations.size(), "props");
                }
            }
        }
        
        pos = brace_close + 1;
    }
    
    // Sort by specificity (lower specificity first, so higher overwrites)
    std::sort(rules_.begin(), rules_.end(),
             [](const CSSRule& a, const CSSRule& b) {
                 return a.specificity < b.specificity;
             });
    
    LOG_INFO("Stylesheet parsed, rules added:", rules_parsed);
}

void StyleResolver::clear() {
    rules_.clear();
    LOG_DEBUG("Style resolver cleared");
}

std::vector<std::string> StyleResolver::splitSelectors(const std::string& selector) {
    std::vector<std::string> result;
    std::string current;
    int bracket_depth = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        char c = selector[i];
        
        if (c == '[') {
            bracket_depth++;
            current += c;
        } else if (c == ']') {
            bracket_depth--;
            current += c;
        } else if (c == ',' && bracket_depth == 0) {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                result.push_back(trimmed);
            }
            current.clear();
        } else {
            current += c;
        }
    }
    
    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        result.push_back(trimmed);
    }
    
    return result;
}

size_t StyleResolver::skipWhitespaceAndComments(const std::string& css, size_t pos) {
    while (pos < css.length()) {
        if (std::isspace(css[pos])) {
            pos++;
        } else if (pos + 1 < css.length() && css[pos] == '/' && css[pos + 1] == '*') {
            // Skip /* comment */
            pos = css.find("*/", pos + 2);
            if (pos == std::string::npos) return css.length();
            pos += 2;
        } else {
            break;
        }
    }
    return pos;
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
    LOG_INFO("Resolving styles for document");
    
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
    
    LOG_INFO("Style resolution complete");
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
    
    // Debug: log first few elements to see what's being processed
    static int debug_count = 0;
    if (debug_count < 5) {
        LOG_DEBUG("Processing element:", element->getTagName(), 
                 "class:", element->getAttribute("class"),
                 "id:", element->getAttribute("id"));
        debug_count++;
    }
    
    int matched = 0;
    
    // Apply matching CSS rules in order of specificity
    for (const CSSRule& rule : rules_) {
        if (matchesSelector(element, rule.selector)) {
            matched++;
            if (debug_count < 10) {
                LOG_DEBUG("  MATCH:", rule.selector);
            }
            for (const auto& decl : rule.declarations) {
                applyDeclaration(decl.first, decl.second, element->computed_style);
            }
        }
    }
    
    if (debug_count < 10 && matched > 0) {
        LOG_DEBUG("  Applied", matched, "rules to", element->getTagName());
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
    
    // Only inherit truly inheritable properties
    // text-transform, margins, padding do NOT inherit
    
    // Inherit text color (inheritable)
    if (style.text_color.r == 0 && style.text_color.g == 0 && style.text_color.b == 0) {
        if (parent_style.text_color.r != 0 || parent_style.text_color.g != 0 || 
            parent_style.text_color.b != 0) {
            style.text_color = parent_style.text_color;
        }
    }
    
    // Inherit font family (inheritable)
    if (style.font_family.empty() && !parent_style.font_family.empty()) {
        style.font_family = parent_style.font_family;
    }
    
    // Note: font-size, text-transform, margins, padding are NOT inherited
    // They are applied only when explicitly set
}

bool StyleResolver::matchesSelector(ElementNode* element, const std::string& selector) {
    std::string sel = trim(selector);
    
    if (sel.empty()) return false;
    
    // Universal selector
    if (sel == "*") return true;
    
    // Handle complex selectors with combinators
    if (sel.find(' ') != std::string::npos || 
        sel.find('>') != std::string::npos ||
        sel.find('+') != std::string::npos ||
        sel.find('~') != std::string::npos) {
        return matchesComplexSelector(element, sel);
    }
    
    // Simple selector matching
    return matchesSimpleSelector(element, sel);
}

bool StyleResolver::matchesSimpleSelector(ElementNode* element, const std::string& selector) {
    std::string sel = trim(selector);
    
    // Parse pseudo-classes and pseudo-elements
    size_t pseudo_pos = sel.find(':');
    std::string base_selector = sel;
    std::string pseudo;
    
    if (pseudo_pos != std::string::npos) {
        base_selector = sel.substr(0, pseudo_pos);
        pseudo = sel.substr(pseudo_pos);
        
        // Handle :not(), :is(), :where()
        if (pseudo.find(":not(") == 0 || pseudo.find(":is(") == 0 || 
            pseudo.find(":where(") == 0) {
            size_t paren_close = pseudo.find(')');
            if (paren_close != std::string::npos) {
                std::string inner = pseudo.substr(pseudo.find('(') + 1, 
                                                  paren_close - pseudo.find('(') - 1);
                
                bool matches_inner = matchesSimpleSelector(element, inner);
                
                if (pseudo.find(":not(") == 0) {
                    if (matches_inner) return false;
                } else {
                    if (!matches_inner) return false;
                }
                
                // Continue with base selector
                if (base_selector.empty()) return true;
            }
        }
        
        // For now, ignore other pseudo-classes/elements
        // (proper implementation would require DOM tree traversal)
    }
    
    if (base_selector.empty()) base_selector = "*";
    
    // Parse attribute selectors
    size_t bracket_pos = base_selector.find('[');
    std::string tag_part = base_selector;
    std::vector<std::string> attributes;
    
    while (bracket_pos != std::string::npos) {
        tag_part = base_selector.substr(0, bracket_pos);
        
        size_t bracket_close = base_selector.find(']', bracket_pos);
        if (bracket_close == std::string::npos) break;
        
        std::string attr = base_selector.substr(bracket_pos + 1, bracket_close - bracket_pos - 1);
        attributes.push_back(attr);
        
        base_selector = base_selector.substr(bracket_close + 1);
        bracket_pos = base_selector.find('[');
    }
    
    // Parse classes and IDs from tag_part
    std::string tag_name;
    std::vector<std::string> classes;
    std::string id;
    
    size_t pos = 0;
    while (pos < tag_part.length()) {
        if (tag_part[pos] == '.') {
            size_t next = tag_part.find_first_of(".#", pos + 1);
            if (next == std::string::npos) next = tag_part.length();
            classes.push_back(tag_part.substr(pos + 1, next - pos - 1));
            pos = next;
        } else if (tag_part[pos] == '#') {
            size_t next = tag_part.find_first_of(".#", pos + 1);
            if (next == std::string::npos) next = tag_part.length();
            id = tag_part.substr(pos + 1, next - pos - 1);
            pos = next;
        } else {
            size_t next = tag_part.find_first_of(".#", pos);
            if (next == std::string::npos) next = tag_part.length();
            tag_name = tag_part.substr(pos, next - pos);
            pos = next;
        }
    }
    
    // Match tag name
    if (!tag_name.empty() && tag_name != "*") {
        if (element->getTagName() != toLowerCase(tag_name)) {
            return false;
        }
    }
    
    // Match ID
    if (!id.empty()) {
        if (element->getAttribute("id") != id) {
            return false;
        }
    }
    
    // Match classes
    for (const std::string& class_name : classes) {
        std::string element_class = element->getAttribute("class");
        
        // Check if class_name is in element_class (space-separated list)
        size_t class_pos = element_class.find(class_name);
        if (class_pos == std::string::npos) return false;
        
        bool start_ok = (class_pos == 0 || std::isspace(element_class[class_pos - 1]));
        bool end_ok = (class_pos + class_name.length() == element_class.length() ||
                      std::isspace(element_class[class_pos + class_name.length()]));
        if (!start_ok || !end_ok) return false;
    }
    
    // Match attributes
    for (const std::string& attr : attributes) {
        if (!matchesAttributeSelector(element, attr)) {
            return false;
        }
    }
    
    return true;
}

bool StyleResolver::matchesAttributeSelector(ElementNode* element, const std::string& attr_selector) {
    std::string attr = trim(attr_selector);
    
    // Simple attribute presence: [attr]
    if (attr.find('=') == std::string::npos) {
        return element->hasAttribute(attr);
    }
    
    // Attribute with value: [attr="value"] or [attr=value]
    size_t eq_pos = attr.find('=');
    std::string attr_name = trim(attr.substr(0, eq_pos));
    std::string attr_value = trim(attr.substr(eq_pos + 1));
    
    // Remove quotes
    if (!attr_value.empty() && (attr_value[0] == '"' || attr_value[0] == '\'')) {
        attr_value = attr_value.substr(1, attr_value.length() - 2);
    }
    
    return element->getAttribute(attr_name) == attr_value;
}

bool StyleResolver::matchesComplexSelector(ElementNode* element, const std::string& selector) {
    // Simplified complex selector matching
    // Handle child combinator (>) before descendant combinator (space)
    
    // Find rightmost combinator
    size_t last_gt = selector.rfind('>');
    size_t last_space = std::string::npos;
    
    // Find last space that's not inside brackets or after '>'
    for (int i = static_cast<int>(selector.length()) - 1; i >= 0; i--) {
        if (selector[i] == ' ') {
            // Make sure it's not right after '>' or before '>'
            bool after_gt = (i > 0 && selector[i-1] == '>');
            bool before_gt = (i < static_cast<int>(selector.length()) - 1 && selector[i+1] == '>');
            
            if (!after_gt && !before_gt && (last_gt == std::string::npos || i < static_cast<int>(last_gt))) {
                last_space = i;
                break;
            }
        }
    }
    
    // Handle > combinator
    if (last_gt != std::string::npos && (last_space == std::string::npos || last_gt > last_space)) {
        std::string right = trim(selector.substr(last_gt + 1));
        std::string left = trim(selector.substr(0, last_gt));
        
        if (!matchesSimpleSelector(element, right)) return false;
        if (!element->parent || element->parent->getType() != NodeType::Element) return false;
        
        return matchesSelector(static_cast<ElementNode*>(element->parent), left);
    }
    
    // Handle descendant combinator (space)
    if (last_space != std::string::npos) {
        std::string right = trim(selector.substr(last_space + 1));
        std::string left = trim(selector.substr(0, last_space));
        
        if (!matchesSimpleSelector(element, right)) return false;
        
        // Check ancestors
        DOMNode* ancestor = element->parent;
        while (ancestor) {
            if (ancestor->getType() == NodeType::Element) {
                if (matchesSelector(static_cast<ElementNode*>(ancestor), left)) {
                    return true;
                }
            }
            ancestor = ancestor->parent;
        }
        return false;
    }
    
    // No combinators found, treat as simple selector
    return matchesSimpleSelector(element, selector);
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    // CSS specificity: (inline, IDs, classes/attrs, elements)
    // We simplify to a single number: ID=100, class=10, element=1
    
    int specificity = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '#') specificity += 100;
        else if (selector[i] == '.') specificity += 10;
        else if (selector[i] == '[') specificity += 10;
    }
    
    // Count element selectors (simplified)
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
        // Store original value for font family
        style.font_family = value; // Keep original case
        
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
    else if (prop == "text-align-last") {
        style.text_align_last = parseTextAlignLast(val);
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
        style.margin_top = parseLengthToPixels(val);
    }
    else if (prop == "margin-bottom") {
        style.margin_bottom = parseLengthToPixels(val);
    }
    else if (prop == "margin-left") {
        style.margin_left = parseLengthToPixels(val);
    }
    else if (prop == "margin-right") {
        style.margin_right = parseLengthToPixels(val);
    }
    else if (prop == "margin") {
        float m = parseLengthToPixels(val);
        style.margin_top = style.margin_bottom = style.margin_left = style.margin_right = m;
    }
    else if (prop == "padding-top") {
        style.padding_top = parseLengthToPixels(val);
    }
    else if (prop == "padding-bottom") {
        style.padding_bottom = parseLengthToPixels(val);
    }
    else if (prop == "padding-left") {
        style.padding_left = parseLengthToPixels(val);
    }
    else if (prop == "padding-right") {
        style.padding_right = parseLengthToPixels(val);
    }
    else if (prop == "padding") {
        float p = parseLengthToPixels(val);
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
    else if (prop == "letter-spacing") {
        style.letter_spacing = parseLength(val, 1.0f);
    }
    else if (prop == "text-transform") {
        if (val == "uppercase") {
            style.text_transform = ComputedStyle::TextTransform::Uppercase;
        } else if (val == "lowercase") {
            style.text_transform = ComputedStyle::TextTransform::Lowercase;
        } else if (val == "capitalize") {
            style.text_transform = ComputedStyle::TextTransform::Capitalize;
        } else {
            style.text_transform = ComputedStyle::TextTransform::None;
        }
    }
    else if (prop == "font-variant-caps") {
        if (val == "small-caps") {
            style.font_variant_caps = ComputedStyle::FontVariantCaps::SmallCaps;
        } else if (val == "all-small-caps") {
            style.font_variant_caps = ComputedStyle::FontVariantCaps::AllSmallCaps;
        } else {
            style.font_variant_caps = ComputedStyle::FontVariantCaps::Normal;
        }
    }
    else if (prop == "hyphens") {
        if (val == "none") {
            style.hyphens = ComputedStyle::Hyphens::None;
        } else if (val == "manual") {
            style.hyphens = ComputedStyle::Hyphens::Manual;
        } else if (val == "auto") {
            style.hyphens = ComputedStyle::Hyphens::Auto;
        }
    }
    else if (prop == "text-indent") {
        style.text_indent = parseLength(val, 1.0f);
    }
    else if (prop == "page-break-before") {
        style.page_break_before = parsePageBreak(val);
    }
    else if (prop == "page-break-after") {
        style.page_break_after = parsePageBreak(val);
    }
    else if (prop == "page-break-inside") {
        style.page_break_inside = parsePageBreak(val);
    }
    else if (prop == "text-rendering") {
        if (val == "optimizespeed") {
            style.text_rendering = ComputedStyle::TextRendering::OptimizeSpeed;
        } else if (val == "optimizelegibility") {
            style.text_rendering = ComputedStyle::TextRendering::OptimizeLegibility;
        } else {
            style.text_rendering = ComputedStyle::TextRendering::Auto;
        }
    }
    // Ignore unsupported properties silently
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

ComputedStyle::TextAlignLast StyleResolver::parseTextAlignLast(const std::string& value) {
    if (value == "left") return ComputedStyle::TextAlignLast::Left;
    if (value == "right") return ComputedStyle::TextAlignLast::Right;
    if (value == "center") return ComputedStyle::TextAlignLast::Center;
    if (value == "justify") return ComputedStyle::TextAlignLast::Justify;
    return ComputedStyle::TextAlignLast::Auto;
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

ComputedStyle::PageBreak StyleResolver::parsePageBreak(const std::string& value) {
    if (value == "always") return ComputedStyle::PageBreak::Always;
    if (value == "avoid") return ComputedStyle::PageBreak::Avoid;
    return ComputedStyle::PageBreak::Auto;
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
            try {
                unsigned int color_val = std::stoul(hex, nullptr, 16);
                return ComputedStyle::Color(
                    (color_val >> 16) & 0xFF,
                    (color_val >> 8) & 0xFF,
                    color_val & 0xFF
                );
            } catch (...) {
                return ComputedStyle::Color(0, 0, 0);
            }
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
        return num;  // Return as em units (will be multiplied by font size later)
    } else if (unit == "rem") {
        return num;  // Return as rem units
    } else if (unit == "%") {
        return num / 100.0f;
    } else if (unit == "pt") {
        return num * 1.333f;  // 1pt = 1.333px
    }
    
    return num;
}

float StyleResolver::parseLengthToPixels(const std::string& value) {
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
        return num * 16.0f;  // Assume 16px base font
    } else if (unit == "rem") {
        return num * 16.0f;  // 16px root font size
    } else if (unit == "%") {
        return (num / 100.0f) * 16.0f;
    } else if (unit == "pt") {
        return num * 1.333f;  // 1pt = 1.333px
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