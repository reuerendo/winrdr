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
        pos = skipWhitespaceAndComments(css, pos);
        if (pos >= css.length()) break;
        
        size_t brace_open = css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(css.substr(pos, brace_open - pos));
        
        size_t brace_close = css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string declarations_str = css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        std::vector<std::string> selectors = splitSelectors(selector);
        
        for (const std::string& sel : selectors) {
            CSSRule rule;
            rule.selector = trim(sel);
            rule.specificity = calculateSpecificity(rule.selector);
            parseDeclarations(declarations_str, rule.declarations);
            
            if (!rule.declarations.empty()) {
                rules_.push_back(rule);
                rules_parsed++;
                
                if (rules_parsed <= 10) {
                    LOG_DEBUG("CSS rule:", rule.selector, "->", rule.declarations.size(), "props");
                }
            }
        }
        
        pos = brace_close + 1;
    }
    
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
    int paren_depth = 0;
    int bracket_depth = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        char c = selector[i];
        
        if (c == '(') {
            paren_depth++;
            current += c;
        } else if (c == ')') {
            paren_depth--;
            current += c;
        } else if (c == '[') {
            bracket_depth++;
            current += c;
        } else if (c == ']') {
            bracket_depth--;
            current += c;
        } else if (c == ',' && paren_depth == 0 && bracket_depth == 0) {
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
        size_t colon = declarations_str.find(':', pos);
        if (colon == std::string::npos) break;
        
        std::string property = toLowerCase(trim(declarations_str.substr(pos, colon - pos)));
        
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
    
    std::vector<DOMNode*> queue;
    queue.push_back(document);
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        applyDefaultStyles(node);
        applyCSSRules(node);
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            applyInlineStyle(element);
        }
        
        inheritStyles(node);
        
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
    
    if (tag == "div" || tag == "p" || tag == "section" || tag == "article" ||
        tag == "aside" || tag == "header" || tag == "footer" || tag == "main" ||
        tag == "nav" || tag == "blockquote" || tag == "pre" || tag == "figure" ||
        tag == "figcaption" || tag == "address" || tag == "center") {
        style.display = DisplayType::Block;
        style.margin_bottom = 1.0f;
    }
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
    else if (tag == "ul" || tag == "ol") {
        style.display = DisplayType::Block;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
        style.padding_left = 40.0f;
    }
    else if (tag == "li") {
        style.display = DisplayType::ListItem;
    }
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
    else if (tag == "a") {
        style.display = DisplayType::Inline;
        style.underline = true;
        style.text_color = ComputedStyle::Color(0, 0, 255);
    }
    else if (tag == "blockquote") {
        style.display = DisplayType::Block;
        style.margin_left = 40.0f;
        style.margin_right = 40.0f;
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    else if (tag == "table") {
        style.display = DisplayType::Block;  // Changed from Table to Block
        style.margin_top = 1.0f;
        style.margin_bottom = 1.0f;
    }
    else if (tag == "tr") {
        style.display = DisplayType::Block;  // Changed from TableRow to Block
    }
    else if (tag == "td" || tag == "th") {
        style.display = DisplayType::Block;  // Changed from TableCell to Block
        style.padding_left = 2.0f;
        style.padding_right = 2.0f;
        if (tag == "th") {
            style.bold = true;
        }
    }
    else if (tag == "hr") {
        style.display = DisplayType::Block;
        style.margin_top = 0.5f;
        style.margin_bottom = 0.5f;
    }
    else if (tag == "br") {
        style.display = DisplayType::Inline;
    }
    else if (tag == "script" || tag == "style" || tag == "noscript" ||
             tag == "head" || tag == "title" || tag == "meta" || tag == "link") {
        style.display = DisplayType::None;
    }
    else {
        style.display = DisplayType::Inline;
    }
}

void StyleResolver::applyCSSRules(DOMNode* node) {
    if (node->getType() != NodeType::Element) return;
    
    ElementNode* element = static_cast<ElementNode*>(node);
    
    static int debug_count = 0;
    bool should_log = (debug_count < 5);
    
    if (should_log) {
        LOG_DEBUG("Processing element:", element->getTagName(), 
                 "class:", element->getAttribute("class"),
                 "id:", element->getAttribute("id"),
                 "role:", element->getAttribute("role"));
        debug_count++;
    }
    
    int matched = 0;
    
    for (const CSSRule& rule : rules_) {
        if (matchesSelector(element, rule.selector)) {
            matched++;
            if (should_log && matched <= 3) {
                LOG_DEBUG("  MATCH:", rule.selector);
                int prop_count = 0;
                for (const auto& decl : rule.declarations) {
                    if (prop_count < 3) {
                        LOG_DEBUG("    Property:", decl.first, "=", decl.second);
                        prop_count++;
                    }
                }
            }
            for (const auto& decl : rule.declarations) {
                applyDeclaration(decl.first, decl.second, element->computed_style);
            }
        }
    }
    
    if (should_log && matched > 0) {
        LOG_DEBUG("  Applied", matched, "rules to", element->getTagName());
        LOG_DEBUG("  Final: margin-top=", element->computed_style.margin_top,
                 "margin-bottom=", element->computed_style.margin_bottom,
                 "text-indent=", element->computed_style.text_indent);
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
    
    // Inherit text color
    if (style.text_color.r == 0 && style.text_color.g == 0 && style.text_color.b == 0) {
        if (parent_style.text_color.r != 0 || parent_style.text_color.g != 0 || 
            parent_style.text_color.b != 0) {
            style.text_color = parent_style.text_color;
        }
    }
    
    // Inherit font family
    if (style.font_family.empty() && !parent_style.font_family.empty()) {
        style.font_family = parent_style.font_family;
    }
    
    // Inherit text-indent for specific cases
    if (node->getType() == NodeType::Element) {
        ElementNode* elem = static_cast<ElementNode*>(node);
        const std::string& tag = elem->getTagName();
        
        // Inherit text-indent for paragraphs following paragraphs
        if (tag == "p" && style.text_indent == 0.0f && parent_style.text_indent != 0.0f) {
            // Check if this is a subsequent paragraph
            bool is_subsequent = false;
            if (node->parent) {
                for (size_t i = 0; i < node->parent->children.size(); i++) {
                    if (node->parent->children[i].get() == node && i > 0) {
                        for (int j = i - 1; j >= 0; j--) {
                            if (node->parent->children[j]->getType() == NodeType::Element) {
                                ElementNode* prev = static_cast<ElementNode*>(node->parent->children[j].get());
                                if (prev->getTagName() == "p") {
                                    is_subsequent = true;
                                }
                                break;
                            }
                        }
                    }
                    if (node->parent->children[i].get() == node) break;
                }
            }
        }
    }
}

bool StyleResolver::matchesSelector(ElementNode* element, const std::string& selector) {
    std::string sel = trim(selector);
    
    if (sel.empty()) return false;
    if (sel == "*") return true;
    
    if (sel.find(' ') != std::string::npos || 
        sel.find('>') != std::string::npos ||
        sel.find('+') != std::string::npos ||
        sel.find('~') != std::string::npos) {
        return matchesComplexSelector(element, sel);
    }
    
    return matchesSimpleSelector(element, sel);
}

bool StyleResolver::matchesSimpleSelector(ElementNode* element, const std::string& selector) {
    std::string sel = trim(selector);
    
    // Debug first few matches
    static int match_count = 0;
    bool should_debug = (match_count < 10);
    
    size_t pseudo_pos = sel.find(':');
    std::string base_selector = sel;
    std::vector<std::string> pseudo_classes;
    
    if (pseudo_pos != std::string::npos) {
        base_selector = sel.substr(0, pseudo_pos);
        std::string pseudo_part = sel.substr(pseudo_pos);
        
        size_t pos = 0;
        while (pos < pseudo_part.length()) {
            if (pseudo_part[pos] == ':') {
                size_t next = pseudo_part.find(':', pos + 1);
                if (next == std::string::npos) next = pseudo_part.length();
                
                if (pos + 1 < pseudo_part.length() && 
                    (pseudo_part.substr(pos).find("not(") == 1 ||
                     pseudo_part.substr(pos).find("is(") == 1 ||
                     pseudo_part.substr(pos).find("where(") == 1)) {
                    
                    int paren_depth = 0;
                    size_t i = pos;
                    while (i < pseudo_part.length()) {
                        if (pseudo_part[i] == '(') paren_depth++;
                        if (pseudo_part[i] == ')') {
                            paren_depth--;
                            if (paren_depth == 0) {
                                next = i + 1;
                                break;
                            }
                        }
                        i++;
                    }
                }
                
                pseudo_classes.push_back(pseudo_part.substr(pos, next - pos));
                pos = next;
            } else {
                pos++;
            }
        }
    }
    
    // Process pseudo-classes
    for (const std::string& pseudo : pseudo_classes) {
        if (should_debug) {
            LOG_DEBUG("      Checking pseudo:", pseudo, "on", element->getTagName());
        }
        
        if (pseudo == ":first-child") {
            if (!isFirstChild(element)) {
                if (should_debug) LOG_DEBUG("        Failed: not first child");
                return false;
            }
        }
        else if (pseudo == ":last-child") {
            if (!isLastChild(element)) {
                if (should_debug) LOG_DEBUG("        Failed: not last child");
                return false;
            }
        }
        else if (pseudo.find(":first-of-type") == 0) {
            if (!isFirstOfType(element)) {
                if (should_debug) LOG_DEBUG("        Failed: not first of type");
                return false;
            }
        }
        else if (pseudo.find(":last-of-type") == 0) {
            if (!isLastOfType(element)) {
                if (should_debug) LOG_DEBUG("        Failed: not last of type");
                return false;
            }
        }
        else if (pseudo.find(":not(") == 0) {
            size_t paren_close = pseudo.rfind(')');
            if (paren_close != std::string::npos) {
                std::string inner = pseudo.substr(5, paren_close - 5);
                std::vector<std::string> not_selectors = splitSelectors(inner);
                
                if (should_debug) {
                    LOG_DEBUG("        :not() inner selectors:", inner);
                }
                
                for (const std::string& not_sel : not_selectors) {
                    std::string trimmed_not = trim(not_sel);
                    if (should_debug) {
                        LOG_DEBUG("          Testing :not selector:", trimmed_not);
                    }
                    if (matchesSimpleSelector(element, trimmed_not)) {
                        if (should_debug) {
                            LOG_DEBUG("        Failed: element matches :not() selector");
                        }
                        return false;
                    }
                }
            }
        }
        else if (pseudo.find(":is(") == 0 || pseudo.find(":where(") == 0) {
            size_t paren_close = pseudo.rfind(')');
            if (paren_close != std::string::npos) {
                size_t paren_open = pseudo.find('(');
                std::string inner = pseudo.substr(paren_open + 1, paren_close - paren_open - 1);
                
                std::vector<std::string> inner_selectors = splitSelectors(inner);
                
                bool any_match = false;
                for (const std::string& inner_sel : inner_selectors) {
                    if (matchesSimpleSelector(element, trim(inner_sel))) {
                        any_match = true;
                        break;
                    }
                }
                
                if (!any_match) {
                    if (should_debug) LOG_DEBUG("        Failed: no :is() match");
                    return false;
                }
            }
        }
    }
    
    if (base_selector.empty()) base_selector = "*";
    
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
    
    if (!tag_name.empty() && tag_name != "*") {
        if (element->getTagName() != toLowerCase(tag_name)) {
            if (should_debug) {
                LOG_DEBUG("      Tag mismatch:", element->getTagName(), "!=", toLowerCase(tag_name));
            }
            return false;
        }
    }
    
    if (!id.empty()) {
        if (element->getAttribute("id") != id) {
            if (should_debug) LOG_DEBUG("      ID mismatch");
            return false;
        }
    }
    
    for (const std::string& class_name : classes) {
        std::string element_class = element->getAttribute("class");
        
        size_t class_pos = element_class.find(class_name);
        if (class_pos == std::string::npos) {
            if (should_debug) LOG_DEBUG("      Class not found:", class_name);
            return false;
        }
        
        bool start_ok = (class_pos == 0 || std::isspace(element_class[class_pos - 1]));
        bool end_ok = (class_pos + class_name.length() == element_class.length() ||
                      std::isspace(element_class[class_pos + class_name.length()]));
        if (!start_ok || !end_ok) {
            if (should_debug) LOG_DEBUG("      Class boundary mismatch");
            return false;
        }
    }
    
    for (const std::string& attr : attributes) {
        if (!matchesAttributeSelector(element, attr)) {
            if (should_debug) LOG_DEBUG("      Attribute mismatch:", attr);
            return false;
        }
    }
    
    if (should_debug && match_count < 10) {
        LOG_DEBUG("      SUCCESS: matched", selector);
        match_count++;
    }
    
    return true;
}

bool StyleResolver::isFirstChild(ElementNode* element) {
    if (!element->parent) return false;
    
    for (auto& child : element->parent->children) {
        if (child->getType() == NodeType::Element) {
            return child.get() == element;
        }
    }
    return false;
}

bool StyleResolver::isLastChild(ElementNode* element) {
    if (!element->parent) return false;
    
    for (auto it = element->parent->children.rbegin(); 
         it != element->parent->children.rend(); ++it) {
        if ((*it)->getType() == NodeType::Element) {
            return it->get() == element;
        }
    }
    return false;
}

bool StyleResolver::isFirstOfType(ElementNode* element) {
    if (!element->parent) return false;
    
    const std::string& tag = element->getTagName();
    
    for (auto& child : element->parent->children) {
        if (child->getType() == NodeType::Element) {
            ElementNode* elem = static_cast<ElementNode*>(child.get());
            if (elem->getTagName() == tag) {
                return elem == element;
            }
        }
    }
    return false;
}

bool StyleResolver::isLastOfType(ElementNode* element) {
    if (!element->parent) return false;
    
    const std::string& tag = element->getTagName();
    
    for (auto it = element->parent->children.rbegin(); 
         it != element->parent->children.rend(); ++it) {
        if ((*it)->getType() == NodeType::Element) {
            ElementNode* elem = static_cast<ElementNode*>(it->get());
            if (elem->getTagName() == tag) {
                return elem == element;
            }
        }
    }
    return false;
}

bool StyleResolver::matchesAttributeSelector(ElementNode* element, const std::string& attr_selector) {
    std::string attr = trim(attr_selector);
    
    if (attr.find('=') == std::string::npos) {
        return element->hasAttribute(attr);
    }
    
    size_t eq_pos = attr.find('=');
    std::string attr_name = trim(attr.substr(0, eq_pos));
    std::string attr_value = trim(attr.substr(eq_pos + 1));
    
    if (!attr_value.empty() && (attr_value[0] == '"' || attr_value[0] == '\'')) {
        attr_value = attr_value.substr(1, attr_value.length() - 2);
    }
    
    return element->getAttribute(attr_name) == attr_value;
}

bool StyleResolver::matchesComplexSelector(ElementNode* element, const std::string& selector) {
    // Find rightmost combinator
    size_t last_gt = selector.rfind('>');
    size_t last_plus = selector.rfind('+');
    size_t last_tilde = selector.rfind('~');
    size_t last_space = std::string::npos;
    
    // Find last meaningful space (not inside brackets/parens or adjacent to combinators)
    for (int i = static_cast<int>(selector.length()) - 1; i >= 0; i--) {
        if (selector[i] == ' ') {
            bool after_combinator = (i > 0 && (selector[i-1] == '>' || selector[i-1] == '+' || selector[i-1] == '~'));
            bool before_combinator = (i < static_cast<int>(selector.length()) - 1 && 
                                     (selector[i+1] == '>' || selector[i+1] == '+' || selector[i+1] == '~'));
            
            if (!after_combinator && !before_combinator) {
                if ((last_gt == std::string::npos || i < static_cast<int>(last_gt)) &&
                    (last_plus == std::string::npos || i < static_cast<int>(last_plus)) &&
                    (last_tilde == std::string::npos || i < static_cast<int>(last_tilde))) {
                    last_space = i;
                    break;
                }
            }
        }
    }
    
    size_t rightmost = std::string::npos;
    char combinator = ' ';
    
    if (last_gt != std::string::npos && 
        (rightmost == std::string::npos || last_gt > rightmost)) {
        rightmost = last_gt;
        combinator = '>';
    }
    if (last_plus != std::string::npos && 
        (rightmost == std::string::npos || last_plus > rightmost)) {
        rightmost = last_plus;
        combinator = '+';
    }
    if (last_tilde != std::string::npos && 
        (rightmost == std::string::npos || last_tilde > rightmost)) {
        rightmost = last_tilde;
        combinator = '~';
    }
    if (last_space != std::string::npos && 
        (rightmost == std::string::npos || last_space > rightmost)) {
        rightmost = last_space;
        combinator = ' ';
    }
    
    // Handle > combinator (direct child)
    if (combinator == '>') {
        std::string right = trim(selector.substr(rightmost + 1));
        std::string left = trim(selector.substr(0, rightmost));
        
        if (!matchesSimpleSelector(element, right)) return false;
        if (!element->parent || element->parent->getType() != NodeType::Element) return false;
        
        return matchesSelector(static_cast<ElementNode*>(element->parent), left);
    }
    
    // Handle + combinator (adjacent sibling)
    if (combinator == '+') {
        std::string right = trim(selector.substr(rightmost + 1));
        std::string left = trim(selector.substr(0, rightmost));
        
        if (!matchesSimpleSelector(element, right)) return false;
        
        if (!element->parent) return false;
        
        ElementNode* prev_sibling = nullptr;
        
        for (auto& child : element->parent->children) {
            if (child.get() == element) {
                break;
            }
            if (child->getType() == NodeType::Element) {
                prev_sibling = static_cast<ElementNode*>(child.get());
            }
        }
        
        if (!prev_sibling) return false;
        
        return matchesSelector(prev_sibling, left);
    }
    
    // Handle ~ combinator (general sibling)
    if (combinator == '~') {
        std::string right = trim(selector.substr(rightmost + 1));
        std::string left = trim(selector.substr(0, rightmost));
        
        if (!matchesSimpleSelector(element, right)) return false;
        
        if (!element->parent) return false;
        
        for (auto& child : element->parent->children) {
            if (child.get() == element) break;
            
            if (child->getType() == NodeType::Element) {
                ElementNode* sibling = static_cast<ElementNode*>(child.get());
                if (matchesSelector(sibling, left)) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // Handle space combinator (descendant) - FIXED
    if (combinator == ' ') {
        std::string right = trim(selector.substr(rightmost + 1));
        std::string left = trim(selector.substr(0, rightmost));
        
        if (!matchesSimpleSelector(element, right)) return false;
        
        // Walk up ancestor chain
        DOMNode* ancestor = element->parent;
        while (ancestor) {
            if (ancestor->getType() == NodeType::Element) {
                ElementNode* ancestor_elem = static_cast<ElementNode*>(ancestor);
                if (matchesSelector(ancestor_elem, left)) {
                    return true;
                }
            }
            ancestor = ancestor->parent;
        }
        return false;
    }
    
    return matchesSimpleSelector(element, selector);
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    int specificity = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '#') specificity += 100;
        else if (selector[i] == '.') specificity += 10;
        else if (selector[i] == '[') specificity += 10;
    }
    
    if (specificity == 0 && !selector.empty() && selector[0] != '*') {
        specificity = 1;
    }
    
    return specificity;
}

void StyleResolver::applyDeclaration(const std::string& property, const std::string& value,
                                    ComputedStyle& style) {
    std::string prop = toLowerCase(trim(property));
    std::string val = toLowerCase(trim(value));
    
    // Skip vendor-specific and unsupported properties
    if (prop.find("-cr-") == 0 || prop.find("-webkit-") == 0 || 
        prop.find("-moz-") == 0 || prop.find("-ms-") == 0 ||
        prop == "border" || prop == "font-kerning" || prop == "font-variant-ligatures" ||
        prop == "font-variant-numeric" || prop == "text-rendering") {
        return;
    }
    
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
        style.font_family = value;
        
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
}

DisplayType StyleResolver::parseDisplay(const std::string& value) {
    if (value == "block") return DisplayType::Block;
    if (value == "inline") return DisplayType::Inline;
    if (value == "inline-block") return DisplayType::InlineBlock;
    if (value == "none") return DisplayType::None;
    if (value == "list-item") return DisplayType::ListItem;
    if (value == "table") return DisplayType::Block;  // Treat table as block
    if (value == "table-row") return DisplayType::Block;
    if (value == "table-cell") return DisplayType::Block;
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
    
    if (!value.empty() && value[0] == '#') {
        std::string hex = value.substr(1);
        
        if (hex.length() == 3) {
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
    
    float num = 0.0f;
    size_t unit_pos = 0;
    
    try {
        num = std::stof(value, &unit_pos);
    } catch (...) {
        return 0.0f;
    }
    
    std::string unit = trim(value.substr(unit_pos));
    
    if (unit.empty() || unit == "px") {
        return num;
    } else if (unit == "em") {
        return num;
    } else if (unit == "rem") {
        return num;
    } else if (unit == "%") {
        return num / 100.0f;
    } else if (unit == "pt") {
        return num * 1.333f;
    }
    
    return num;
}

float StyleResolver::parseLengthToPixels(const std::string& value) {
    if (value.empty()) return 0.0f;
    
    // Handle "auto" keyword
    if (toLowerCase(value) == "auto") {
        return 0.0f;
    }
    
    float num = 0.0f;
    size_t unit_pos = 0;
    
    try {
        num = std::stof(value, &unit_pos);
    } catch (...) {
        return 0.0f;
    }
    
    std::string unit = trim(value.substr(unit_pos));
    
    if (unit.empty() || unit == "px") {
        return num;
    } else if (unit == "em") {
        return num * 16.0f;
    } else if (unit == "rem") {
        return num * 16.0f;
    } else if (unit == "%") {
        return (num / 100.0f) * 16.0f;
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