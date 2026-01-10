#include "style_resolver.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

namespace epub {

// Base font size for rem/em calculations (16px standard)
static constexpr float BASE_FONT_SIZE = 16.0f;

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
    
    // Collect all matching rules with their specificity
    std::vector<std::pair<int, const CSSRule*>> matching_rules;
    
    for (const CSSRule& rule : rules_) {
        if (matchesSelector(element, rule.selector)) {
            matching_rules.push_back({rule.specificity, &rule});
        }
    }
    
    // Sort by specificity (lower first, so higher specificity overwrites)
    std::sort(matching_rules.begin(), matching_rules.end(),
             [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Apply in order of specificity
    for (const auto& pair : matching_rules) {
        for (const auto& decl : pair.second->declarations) {
            applyDeclaration(decl.first, decl.second, element->computed_style);
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
    
    // Inherit text color if not explicitly set
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
    
    // Inherit line height
    if (style.line_height == 1.2f && parent_style.line_height != 1.2f) {
        style.line_height = parent_style.line_height;
    }
    
    // Inherit font size multiplier (relative to parent)
    if (style.font_size_multiplier == 1.0f && parent_style.font_size_multiplier != 1.0f) {
        // Child inherits parent's computed font size
        style.font_size_multiplier = parent_style.font_size_multiplier;
    }
    
    // Inherit text transform
    if (style.text_transform == ComputedStyle::TextTransform::None && 
        parent_style.text_transform != ComputedStyle::TextTransform::None) {
        style.text_transform = parent_style.text_transform;
    }
    
    // Inherit letter spacing
    if (style.letter_spacing == 0.0f && parent_style.letter_spacing != 0.0f) {
        style.letter_spacing = parent_style.letter_spacing;
    }
    
    // Special handling for text-indent inheritance in paragraphs
    if (node->getType() == NodeType::Element) {
        ElementNode* elem = static_cast<ElementNode*>(node);
        const std::string& tag = elem->getTagName();
        
        // Handle p + p text-indent inheritance pattern
        if (tag == "p") {
            // Check if this paragraph should inherit text-indent
            // This happens when:
            // 1. The paragraph itself has no explicit text-indent
            // 2. There's a previous paragraph sibling
            // 3. The parent has text-indent rules for subsequent paragraphs
            
            if (style.text_indent == 0.0f && node->parent) {
                bool is_subsequent_paragraph = false;
                ElementNode* prev_paragraph = nullptr;
                
                // Find if there's a previous paragraph sibling
                for (auto& child : node->parent->children) {
                    if (child.get() == node) {
                        // We've reached current node
                        if (prev_paragraph) {
                            is_subsequent_paragraph = true;
                        }
                        break;
                    }
                    
                    if (child->getType() == NodeType::Element) {
                        ElementNode* sibling = static_cast<ElementNode*>(child.get());
                        if (sibling->getTagName() == "p") {
                            prev_paragraph = sibling;
                        } else {
                            // Non-paragraph element breaks the sequence
                            prev_paragraph = nullptr;
                        }
                    } else if (child->getType() == NodeType::Text) {
                        // Check if it's just whitespace
                        TextNode* text_node = static_cast<TextNode*>(child.get());
                        const std::string& text = text_node->getText();
                        bool only_whitespace = true;
                        for (char c : text) {
                            if (!std::isspace(c)) {
                                only_whitespace = false;
                                break;
                            }
                        }
                        if (!only_whitespace) {
                            // Non-whitespace text breaks the sequence
                            prev_paragraph = nullptr;
                        }
                    }
                }
                
                // If this is a subsequent paragraph and the previous paragraph has text-indent,
                // inherit it (this handles the p + p { text-indent: X } pattern)
                if (is_subsequent_paragraph && prev_paragraph && 
                    prev_paragraph->computed_style.text_indent != 0.0f) {
                    style.text_indent = prev_paragraph->computed_style.text_indent;
                }
            }
        }
        
        // Handle list item indentation inheritance
        if (tag == "li") {
            // List items inherit padding from their list parent
            if (node->parent && node->parent->getType() == NodeType::Element) {
                ElementNode* parent_elem = static_cast<ElementNode*>(node->parent);
                const std::string& parent_tag = parent_elem->getTagName();
                
                if (parent_tag == "ul" || parent_tag == "ol") {
                    // Inherit list style type
                    if (style.list_style == ComputedStyle::ListStyleType::Disc && 
                        parent_style.list_style != ComputedStyle::ListStyleType::Disc) {
                        style.list_style = parent_style.list_style;
                    }
                }
            }
        }
        
        // Handle blockquote styling inheritance
        if (tag == "blockquote") {
            // Blockquotes can have special text-indent rules
            // Make sure child paragraphs don't double-indent
            if (style.text_indent == 0.0f && parent_style.text_indent != 0.0f) {
                // Don't inherit text-indent inside blockquotes unless explicitly set
                // This prevents double indentation
            }
        }
    }
    
    // Inherit white-space handling
    if (style.white_space == ComputedStyle::WhiteSpace::Normal && 
        parent_style.white_space != ComputedStyle::WhiteSpace::Normal) {
        style.white_space = parent_style.white_space;
    }
    
    // Inherit text alignment (for inline elements)
    if (node->getType() == NodeType::Element) {
        ElementNode* elem = static_cast<ElementNode*>(node);
        if (style.display == DisplayType::Inline || 
            style.display == DisplayType::InlineBlock) {
            // Inline elements inherit text-align from parent
            if (style.text_align == ComputedStyle::TextAlign::Left && 
                parent_style.text_align != ComputedStyle::TextAlign::Left) {
                style.text_align = parent_style.text_align;
            }
        }
    }
    
    // Inherit hyphens setting
    if (style.hyphens == ComputedStyle::Hyphens::Manual && 
        parent_style.hyphens != ComputedStyle::Hyphens::Manual) {
        style.hyphens = parent_style.hyphens;
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
    
    if (sel.empty()) return false;
    if (sel == "*") return true;
    
    // Separate base selector from pseudo-classes
    size_t pseudo_pos = sel.find(':');
    std::string base_selector = sel;
    std::vector<std::string> pseudo_classes;
    
    if (pseudo_pos != std::string::npos) {
        base_selector = sel.substr(0, pseudo_pos);
        std::string pseudo_part = sel.substr(pseudo_pos);
        
        // Parse pseudo-classes carefully
        size_t pos = 0;
        while (pos < pseudo_part.length()) {
            if (pseudo_part[pos] == ':') {
                size_t next = pseudo_part.find(':', pos + 1);
                
                // Check for functional pseudo-classes like :not(), :is(), :where()
                if (pos + 1 < pseudo_part.length()) {
                    std::string func_name = "";
                    size_t paren = pseudo_part.find('(', pos + 1);
                    
                    if (paren != std::string::npos && (next == std::string::npos || paren < next)) {
                        func_name = pseudo_part.substr(pos + 1, paren - pos - 1);
                        
                        // Find matching closing parenthesis
                        int paren_depth = 1;
                        size_t i = paren + 1;
                        while (i < pseudo_part.length() && paren_depth > 0) {
                            if (pseudo_part[i] == '(') paren_depth++;
                            else if (pseudo_part[i] == ')') paren_depth--;
                            i++;
                        }
                        
                        if (paren_depth == 0) {
                            next = i;
                        }
                    }
                }
                
                if (next == std::string::npos) next = pseudo_part.length();
                
                pseudo_classes.push_back(pseudo_part.substr(pos, next - pos));
                pos = next;
            } else {
                pos++;
            }
        }
    }
    
    // Process pseudo-classes FIRST (before checking tag/class/id)
    // This allows :not() to properly filter
    for (const std::string& pseudo : pseudo_classes) {
        if (pseudo == ":first-child") {
            if (!isFirstChild(element)) return false;
        }
        else if (pseudo == ":last-child") {
            if (!isLastChild(element)) return false;
        }
        else if (pseudo.find(":first-of-type") == 0) {
            if (!isFirstOfType(element)) return false;
        }
        else if (pseudo.find(":last-of-type") == 0) {
            if (!isLastOfType(element)) return false;
        }
        else if (pseudo.find(":not(") == 0) {
            size_t paren_close = pseudo.rfind(')');
            if (paren_close != std::string::npos) {
                std::string inner = pseudo.substr(5, paren_close - 5);
                std::vector<std::string> not_selectors = splitSelectors(inner);
                
                // Element must NOT match ANY of the inner selectors
                for (const std::string& not_sel : not_selectors) {
                    std::string trimmed_not = trim(not_sel);
                    if (matchesSimpleSelector(element, trimmed_not)) {
                        return false;  // Element matches :not() argument, so fail
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
                
                if (!any_match) return false;
            }
        }
    }
    
    // If base_selector is empty, we only had pseudo-classes
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
    
    // Parse tag, classes, and id from tag_part
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
    
    // Match classes (each class must be present)
    for (const std::string& class_name : classes) {
        std::string element_class = element->getAttribute("class");
        
        // Check if class_name is a word in element_class
        bool found = false;
        size_t search_pos = 0;
        while (search_pos < element_class.length()) {
            size_t class_pos = element_class.find(class_name, search_pos);
            if (class_pos == std::string::npos) break;
            
            // Check word boundaries
            bool start_ok = (class_pos == 0 || std::isspace(element_class[class_pos - 1]));
            bool end_ok = (class_pos + class_name.length() == element_class.length() ||
                          std::isspace(element_class[class_pos + class_name.length()]));
            
            if (start_ok && end_ok) {
                found = true;
                break;
            }
            
            search_pos = class_pos + 1;
        }
        
        if (!found) return false;
    }
    
    // Match attributes
    for (const std::string& attr : attributes) {
        if (!matchesAttributeSelector(element, attr)) {
            return false;
        }
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
    
    // Check for operators: =, ~=, |=, ^=, $=, *=
    size_t eq_pos = attr.find('=');
    
    if (eq_pos == std::string::npos) {
        // Simple attribute existence check [attr]
        return element->hasAttribute(attr);
    }
    
    // Determine operator type
    char op = '=';
    size_t op_start = eq_pos;
    
    if (eq_pos > 0) {
        char prev = attr[eq_pos - 1];
        if (prev == '~' || prev == '|' || prev == '^' || prev == '$' || prev == '*') {
            op = prev;
            op_start = eq_pos - 1;
        }
    }
    
    std::string attr_name = trim(attr.substr(0, op_start));
    std::string attr_value = trim(attr.substr(eq_pos + 1));
    
    // Remove quotes
    if (!attr_value.empty() && (attr_value[0] == '"' || attr_value[0] == '\'')) {
        attr_value = attr_value.substr(1, attr_value.length() - 2);
    }
    
    std::string element_value = element->getAttribute(attr_name);
    
    switch (op) {
        case '=':  // Exact match
            return element_value == attr_value;
            
        case '~':  // Word match (space-separated)
            {
                size_t pos = 0;
                while (pos < element_value.length()) {
                    while (pos < element_value.length() && std::isspace(element_value[pos])) pos++;
                    size_t start = pos;
                    while (pos < element_value.length() && !std::isspace(element_value[pos])) pos++;
                    
                    if (start < pos) {
                        std::string word = element_value.substr(start, pos - start);
                        if (word == attr_value) return true;
                    }
                }
                return false;
            }
            
        case '|':  // Starts with value or value-
            return element_value == attr_value || 
                   (element_value.find(attr_value + "-") == 0);
            
        case '^':  // Starts with
            return element_value.find(attr_value) == 0;
            
        case '$':  // Ends with
            if (attr_value.length() > element_value.length()) return false;
            return element_value.substr(element_value.length() - attr_value.length()) == attr_value;
            
        case '*':  // Contains
            return element_value.find(attr_value) != std::string::npos;
    }
    
    return false;
}

bool StyleResolver::matchesComplexSelector(ElementNode* element, const std::string& selector) {
    struct Combinator {
        size_t pos;
        char type;
    };
    
    std::vector<Combinator> combinators;
    
    // Find all combinators, respecting nesting
    int paren_depth = 0;
    int bracket_depth = 0;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '(') paren_depth++;
        else if (selector[i] == ')') paren_depth--;
        else if (selector[i] == '[') bracket_depth++;
        else if (selector[i] == ']') bracket_depth--;
        
        if (paren_depth == 0 && bracket_depth == 0) {
            if (selector[i] == '>') {
                combinators.push_back({i, '>'});
            } else if (selector[i] == '+') {
                combinators.push_back({i, '+'});
            } else if (selector[i] == '~') {
                combinators.push_back({i, '~'});
            } else if (selector[i] == ' ') {
                // Only count meaningful spaces (not adjacent to other combinators)
                if (i > 0 && selector[i-1] != '>' && selector[i-1] != '+' && 
                    selector[i-1] != '~' && selector[i-1] != ' ') {
                    if (i + 1 < selector.length() && selector[i+1] != '>' && 
                        selector[i+1] != '+' && selector[i+1] != '~' && selector[i+1] != ' ') {
                        combinators.push_back({i, ' '});
                    }
                }
            }
        }
    }
    
    if (combinators.empty()) {
        return matchesSimpleSelector(element, selector);
    }
    
    // Use rightmost combinator
    Combinator rightmost = combinators.back();
    
    std::string right = trim(selector.substr(rightmost.pos + 1));
    std::string left = trim(selector.substr(0, rightmost.pos));
    
    // Element must match the rightmost selector
    if (!matchesSimpleSelector(element, right)) {
        return false;
    }
    
    // Handle each combinator type
    if (rightmost.type == '>') {
        // Direct child: parent must match left selector
        if (!element->parent || element->parent->getType() != NodeType::Element) {
            return false;
        }
        return matchesSelector(static_cast<ElementNode*>(element->parent), left);
    }
    
    if (rightmost.type == '+') {
        // Adjacent sibling: immediate previous element sibling must match
        // ВАЖНО: пропускаем текстовые узлы содержащие только whitespace
        if (!element->parent) return false;
        
        ElementNode* prev_sibling = nullptr;
        
        for (auto& child : element->parent->children) {
            if (child.get() == element) {
                break;  // Found current element
            }
            
            if (child->getType() == NodeType::Element) {
                prev_sibling = static_cast<ElementNode*>(child.get());
            }
            else if (child->getType() == NodeType::Text) {
                // Check if text node has non-whitespace content
                TextNode* text_node = static_cast<TextNode*>(child.get());
                const std::string& text = text_node->getText();
                
                bool has_content = false;
                for (char c : text) {
                    if (!std::isspace(static_cast<unsigned char>(c))) {
                        has_content = true;
                        break;
                    }
                }
                
                // Non-whitespace text breaks adjacency
                if (has_content) {
                    prev_sibling = nullptr;
                }
            }
        }
        
        if (!prev_sibling) return false;
        return matchesSelector(prev_sibling, left);
    }
    
    if (rightmost.type == '~') {
        // General sibling: any previous sibling must match
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
    
    if (rightmost.type == ' ') {
        // Descendant: any ancestor must match
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
    
    return false;
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    int id_count = 0;
    int class_count = 0;
    int type_count = 0;
    
    int paren_depth = 0;
    int bracket_depth = 0;
    bool in_not = false;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '(') {
            paren_depth++;
            // Check if entering :not()
            if (i >= 5 && selector.substr(i-5, 5) == ":not(") {
                in_not = true;
            }
        } else if (selector[i] == ')') {
            paren_depth--;
            if (paren_depth == 0) in_not = false;
        } else if (selector[i] == '[') {
            bracket_depth++;
        } else if (selector[i] == ']') {
            bracket_depth--;
        }
        
        // Don't count specificity inside :not() - it doesn't add to specificity
        if (paren_depth == 0 && bracket_depth == 0) {
            if (selector[i] == '#') {
                id_count++;
            } else if (selector[i] == '.') {
                class_count++;
            } else if (bracket_depth == 1) {
                // Inside attribute selector
                class_count++;
            }
        }
        
        // Pseudo-classes add to class count
        if (selector[i] == ':' && i + 1 < selector.length() && selector[i+1] != ':') {
            if (!in_not) class_count++;
        }
    }
    
    // Count type selectors (simplified - could be improved)
    std::string temp = selector;
    for (char c : {'>', '+', '~', ' ', '.', '#', '[', ':'}) {
        size_t pos = 0;
        while ((pos = temp.find(c, pos)) != std::string::npos) {
            temp[pos] = '|';
            pos++;
        }
    }
    
    std::istringstream iss(temp);
    std::string part;
    while (std::getline(iss, part, '|')) {
        part = trim(part);
        if (!part.empty() && part != "*") {
            type_count++;
        }
    }
    
    // CSS specificity: (id, class, type)
    return id_count * 100 + class_count * 10 + type_count;
}

void StyleResolver::applyDeclaration(const std::string& property, const std::string& value,
                                    ComputedStyle& style) {
    std::string prop = toLowerCase(trim(property));
    std::string val = toLowerCase(trim(value));
    
    // Skip vendor-specific and unsupported properties
    if (prop.find("-cr-") == 0 || prop.find("-webkit-") == 0 || 
        prop.find("-moz-") == 0 || prop.find("-ms-") == 0 ||
        prop.find("-o-") == 0 || prop.find("-epub-") == 0) {
        return;
    }
    
    // Skip font variant properties we don't support
    if (prop == "font-kerning" || prop == "font-variant-ligatures" ||
        prop == "font-variant-numeric" || prop == "font-feature-settings") {
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
        // Parse value with unit
        float num = 0.0f;
        size_t unit_pos = 0;
        
        try {
            num = std::stof(val, &unit_pos);
        } catch (...) {
            return;
        }
        
        std::string unit = trim(val.substr(unit_pos));
        
        // Store in em units for proper scaling
        if (unit.empty() || unit == "px") {
            style.text_indent = num / BASE_FONT_SIZE; // Convert px to em
        } else if (unit == "em") {
            style.text_indent = num;  // Already in em
        } else if (unit == "rem") {
            style.text_indent = num;  // rem same as em for text-indent
        } else if (unit == "%") {
            style.text_indent = num / 100.0f;
        }
        
        LOG_DEBUG("Applied text-indent:", num, unit, "->", style.text_indent, "em");
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
    if (toLowerCase(value) == "auto") return 0.0f;
    
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
        return num * BASE_FONT_SIZE;  // 1em = 16px
    } else if (unit == "rem") {
        return num * BASE_FONT_SIZE;  // 1rem = 16px
    } else if (unit == "%") {
        return (num / 100.0f) * BASE_FONT_SIZE;
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