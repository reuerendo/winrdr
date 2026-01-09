#include "style_resolver.h"
#include "../utils/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

namespace epub {

StyleResolver::StyleResolver() {}

void StyleResolver::addStylesheet(const std::string& css) {
    std::string cleaned_css = removeComments(css);
    
    size_t pos = 0;
    int rules_parsed = 0;
    
    while (pos < cleaned_css.length()) {
        while (pos < cleaned_css.length() && std::isspace(cleaned_css[pos])) {
            pos++;
        }
        
        if (pos >= cleaned_css.length()) break;
        
        size_t brace_open = cleaned_css.find('{', pos);
        if (brace_open == std::string::npos) break;
        
        std::string selector = trim(cleaned_css.substr(pos, brace_open - pos));
        
        size_t brace_close = cleaned_css.find('}', brace_open);
        if (brace_close == std::string::npos) break;
        
        std::string declarations_str = cleaned_css.substr(brace_open + 1, brace_close - brace_open - 1);
        
        // Split by comma for selector groups
        std::vector<std::string> selectors = splitSelectors(selector);
        
        for (const std::string& sel : selectors) {
            CSSRule rule;
            rule.selector = trim(sel);
            rule.specificity = calculateSpecificity(rule.selector);
            parseDeclarations(declarations_str, rule.declarations);
            
            if (!rule.declarations.empty()) {
                rules_.push_back(rule);
                rules_parsed++;
            }
        }
        
        pos = brace_close + 1;
    }
    
    std::sort(rules_.begin(), rules_.end(),
             [](const CSSRule& a, const CSSRule& b) {
                 return a.specificity < b.specificity;
             });
    
    LOG_INFO("Parsed CSS rules:", rules_parsed, "total rules:", rules_.size());
}

std::vector<std::string> StyleResolver::splitSelectors(const std::string& selector) {
    std::vector<std::string> result;
    std::string current;
    int bracket_depth = 0;
    int paren_depth = 0;
    
    for (char c : selector) {
        if (c == '[') bracket_depth++;
        else if (c == ']') bracket_depth--;
        else if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        else if (c == ',' && bracket_depth == 0 && paren_depth == 0) {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                result.push_back(trimmed);
            }
            current.clear();
            continue;
        }
        current += c;
    }
    
    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        result.push_back(trimmed);
    }
    
    return result;
}

std::string StyleResolver::removeComments(const std::string& css) {
    std::string result;
    size_t pos = 0;
    
    while (pos < css.length()) {
        if (pos + 1 < css.length() && css[pos] == '/' && css[pos + 1] == '*') {
            size_t comment_end = css.find("*/", pos + 2);
            if (comment_end != std::string::npos) {
                pos = comment_end + 2;
            } else {
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
    std::vector<DOMNode*> queue;
    queue.push_back(document);
    
    int elements_processed = 0;
    int styles_applied = 0;
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        applyDefaultStyles(node);
        
        int applied = applyCSSRules(node);
        styles_applied += applied;
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            applyInlineStyle(element);
            elements_processed++;
        }
        
        inheritStyles(node);
        
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

int StyleResolver::applyCSSRules(DOMNode* node) {
    if (node->getType() != NodeType::Element) return 0;
    
    ElementNode* element = static_cast<ElementNode*>(node);
    int applied_count = 0;
    
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
    
    if (style.font_size_multiplier == 1.0f && parent_style.font_size_multiplier != 1.0f) {
        style.font_size_multiplier = parent_style.font_size_multiplier;
    }
    
    if (style.text_color.r == 0 && style.text_color.g == 0 && style.text_color.b == 0) {
        if (parent_style.text_color.r != 0 || parent_style.text_color.g != 0 || 
            parent_style.text_color.b != 0) {
            style.text_color = parent_style.text_color;
        }
    }
}

bool StyleResolver::matchesSelector(ElementNode* element, const std::string& selector) {
    // Split by combinators: ' ', '>', '+', '~'
    std::vector<SelectorPart> parts = parseComplexSelector(selector);
    
    // Match from right to left (last part must match element)
    if (parts.empty()) return false;
    
    // Match rightmost selector part against element
    if (!matchesSimpleSelector(element, parts.back().selector)) {
        return false;
    }
    
    // If only one part, we're done
    if (parts.size() == 1) return true;
    
    // Match remaining parts with combinators
    DOMNode* current = element->parent;
    
    for (int i = parts.size() - 2; i >= 0; i--) {
        const SelectorPart& part = parts[i];
        
        if (part.combinator == ' ') {
            // Descendant: find ancestor matching selector
            bool found = false;
            while (current) {
                if (current->getType() == NodeType::Element) {
                    ElementNode* elem = static_cast<ElementNode*>(current);
                    if (matchesSimpleSelector(elem, part.selector)) {
                        found = true;
                        current = current->parent;
                        break;
                    }
                }
                current = current->parent;
            }
            if (!found) return false;
        }
        else if (part.combinator == '>') {
            // Direct child: immediate parent must match
            if (!current || current->getType() != NodeType::Element) return false;
            ElementNode* elem = static_cast<ElementNode*>(current);
            if (!matchesSimpleSelector(elem, part.selector)) return false;
            current = current->parent;
        }
        else if (part.combinator == '+') {
            // Adjacent sibling: previous sibling must match
            if (!current) return false;
            
            // Find previous sibling element
            DOMNode* prev = nullptr;
            if (current->parent) {
                for (size_t j = 0; j < current->parent->children.size(); j++) {
                    if (current->parent->children[j].get() == current && j > 0) {
                        // Find previous element sibling
                        for (int k = j - 1; k >= 0; k--) {
                            if (current->parent->children[k]->getType() == NodeType::Element) {
                                prev = current->parent->children[k].get();
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            
            if (!prev || prev->getType() != NodeType::Element) return false;
            ElementNode* elem = static_cast<ElementNode*>(prev);
            if (!matchesSimpleSelector(elem, part.selector)) return false;
            current = prev->parent;
        }
        else if (part.combinator == '~') {
            // General sibling: any previous sibling must match
            if (!current || !current->parent) return false;
            
            bool found = false;
            for (size_t j = 0; j < current->parent->children.size(); j++) {
                if (current->parent->children[j].get() == current) {
                    // Check all previous siblings
                    for (int k = j - 1; k >= 0; k--) {
                        if (current->parent->children[k]->getType() == NodeType::Element) {
                            ElementNode* elem = static_cast<ElementNode*>(current->parent->children[k].get());
                            if (matchesSimpleSelector(elem, part.selector)) {
                                found = true;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            
            if (!found) return false;
            current = current->parent;
        }
    }
    
    return true;
}

std::vector<StyleResolver::SelectorPart> StyleResolver::parseComplexSelector(const std::string& selector) {
    std::vector<SelectorPart> parts;
    std::string current;
    char last_combinator = ' ';
    
    size_t i = 0;
    while (i < selector.length()) {
        char c = selector[i];
        
        // Check for combinators
        if (c == '>' || c == '+' || c == '~') {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                SelectorPart part;
                part.selector = trimmed;
                part.combinator = last_combinator;
                parts.push_back(part);
            }
            last_combinator = c;
            current.clear();
            i++;
            continue;
        }
        else if (c == ' ' && !current.empty()) {
            // Check if next non-space is a combinator
            size_t next = i + 1;
            while (next < selector.length() && selector[next] == ' ') next++;
            
            if (next < selector.length() && (selector[next] == '>' || selector[next] == '+' || selector[next] == '~')) {
                // Space before combinator, ignore
                i++;
                continue;
            }
            
            // Descendant combinator
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                SelectorPart part;
                part.selector = trimmed;
                part.combinator = last_combinator;
                parts.push_back(part);
            }
            last_combinator = ' ';
            current.clear();
            i++;
            continue;
        }
        
        current += c;
        i++;
    }
    
    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        SelectorPart part;
        part.selector = trimmed;
        part.combinator = last_combinator;
        parts.push_back(part);
    }
    
    return parts;
}

bool StyleResolver::matchesSimpleSelector(ElementNode* element, const std::string& selector) {
    // Parse simple selector: tag.class#id[attr]:pseudo
    
    size_t pos = 0;
    std::string tag;
    std::vector<std::string> classes;
    std::string id;
    std::vector<AttributeMatcher> attributes;
    std::vector<std::string> pseudo_classes;
    
    // Universal selector
    if (selector == "*") return true;
    
    // Parse tag
    if (pos < selector.length() && selector[pos] != '.' && selector[pos] != '#' && 
        selector[pos] != '[' && selector[pos] != ':') {
        while (pos < selector.length() && selector[pos] != '.' && selector[pos] != '#' && 
               selector[pos] != '[' && selector[pos] != ':') {
            tag += selector[pos];
            pos++;
        }
    }
    
    // Parse classes, id, attributes, pseudo-classes
    while (pos < selector.length()) {
        if (selector[pos] == '.') {
            pos++;
            std::string cls;
            while (pos < selector.length() && selector[pos] != '.' && selector[pos] != '#' && 
                   selector[pos] != '[' && selector[pos] != ':') {
                cls += selector[pos];
                pos++;
            }
            classes.push_back(cls);
        }
        else if (selector[pos] == '#') {
            pos++;
            while (pos < selector.length() && selector[pos] != '.' && selector[pos] != '#' && 
                   selector[pos] != '[' && selector[pos] != ':') {
                id += selector[pos];
                pos++;
            }
        }
        else if (selector[pos] == '[') {
            pos++;
            std::string attr_str;
            while (pos < selector.length() && selector[pos] != ']') {
                attr_str += selector[pos];
                pos++;
            }
            if (pos < selector.length()) pos++; // Skip ]
            
            attributes.push_back(parseAttributeSelector(attr_str));
        }
        else if (selector[pos] == ':') {
            pos++;
            // Skip second colon for pseudo-elements
            if (pos < selector.length() && selector[pos] == ':') pos++;
            
            std::string pseudo;
            while (pos < selector.length() && selector[pos] != '.' && selector[pos] != '#' && 
                   selector[pos] != '[' && selector[pos] != ':') {
                pseudo += selector[pos];
                pos++;
                
                // Handle pseudo with arguments like :not(...)
                if (pos < selector.length() && selector[pos] == '(') {
                    int paren_depth = 1;
                    pseudo += '(';
                    pos++;
                    while (pos < selector.length() && paren_depth > 0) {
                        if (selector[pos] == '(') paren_depth++;
                        else if (selector[pos] == ')') paren_depth--;
                        pseudo += selector[pos];
                        pos++;
                    }
                    break;
                }
            }
            pseudo_classes.push_back(pseudo);
        }
        else {
            pos++;
        }
    }
    
    // Check tag
    if (!tag.empty() && element->getTagName() != toLowerCase(tag)) {
        return false;
    }
    
    // Check ID
    if (!id.empty() && element->getAttribute("id") != id) {
        return false;
    }
    
    // Check classes
    for (const std::string& cls : classes) {
        if (!hasClass(element, cls)) {
            return false;
        }
    }
    
    // Check attributes
    for (const AttributeMatcher& attr : attributes) {
        if (!matchesAttribute(element, attr)) {
            return false;
        }
    }
    
    // Check pseudo-classes
    for (const std::string& pseudo : pseudo_classes) {
        if (!matchesPseudoClass(element, pseudo)) {
            return false;
        }
    }
    
    return true;
}

StyleResolver::AttributeMatcher StyleResolver::parseAttributeSelector(const std::string& attr_str) {
    AttributeMatcher matcher;
    
    // Find operator: =, ^=, $=, *=, ~=, |=
    size_t eq_pos = attr_str.find('=');
    
    if (eq_pos == std::string::npos) {
        // Just [attr] - check existence
        matcher.name = trim(attr_str);
        matcher.op = "exists";
        return matcher;
    }
    
    // Check for compound operators
    if (eq_pos > 0) {
        char prev = attr_str[eq_pos - 1];
        if (prev == '^' || prev == '$' || prev == '*' || prev == '~' || prev == '|') {
            matcher.name = trim(attr_str.substr(0, eq_pos - 1));
            matcher.op = std::string() + prev + '=';
        } else {
            matcher.name = trim(attr_str.substr(0, eq_pos));
            matcher.op = "=";
        }
    }
    
    // Extract value
    std::string value = trim(attr_str.substr(eq_pos + 1));
    if (!value.empty() && (value[0] == '"' || value[0] == '\'')) {
        value = value.substr(1, value.length() - 2);
    }
    matcher.value = value;
    
    return matcher;
}

bool StyleResolver::hasClass(ElementNode* element, const std::string& class_name) {
    std::string element_class = element->getAttribute("class");
    if (element_class.empty()) return false;
    
    size_t pos = element_class.find(class_name);
    if (pos == std::string::npos) return false;
    
    bool start_ok = (pos == 0 || std::isspace(element_class[pos - 1]));
    bool end_ok = (pos + class_name.length() == element_class.length() ||
                  std::isspace(element_class[pos + class_name.length()]));
    
    return start_ok && end_ok;
}

bool StyleResolver::matchesAttribute(ElementNode* element, const AttributeMatcher& matcher) {
    if (!element->hasAttribute(matcher.name)) {
        return false;
    }
    
    if (matcher.op == "exists") {
        return true;
    }
    
    std::string attr_value = element->getAttribute(matcher.name);
    
    if (matcher.op == "=") {
        return attr_value == matcher.value;
    }
    else if (matcher.op == "^=") {
        return attr_value.find(matcher.value) == 0;
    }
    else if (matcher.op == "$=") {
        if (attr_value.length() >= matcher.value.length()) {
            return attr_value.substr(attr_value.length() - matcher.value.length()) == matcher.value;
        }
        return false;
    }
    else if (matcher.op == "*=") {
        return attr_value.find(matcher.value) != std::string::npos;
    }
    else if (matcher.op == "~=") {
        // Space-separated list contains value
        size_t pos = attr_value.find(matcher.value);
        if (pos == std::string::npos) return false;
        
        bool start_ok = (pos == 0 || std::isspace(attr_value[pos - 1]));
        bool end_ok = (pos + matcher.value.length() == attr_value.length() ||
                      std::isspace(attr_value[pos + matcher.value.length()]));
        
        return start_ok && end_ok;
    }
    else if (matcher.op == "|=") {
        // Dash-separated: value or value-*
        return attr_value == matcher.value || 
               (attr_value.length() > matcher.value.length() && 
                attr_value.substr(0, matcher.value.length() + 1) == matcher.value + "-");
    }
    
    return false;
}

bool StyleResolver::matchesPseudoClass(ElementNode* element, const std::string& pseudo) {
    if (pseudo == "first-child") {
        if (!element->parent) return false;
        for (auto& child : element->parent->children) {
            if (child->getType() == NodeType::Element) {
                return child.get() == element;
            }
        }
        return false;
    }
    else if (pseudo == "last-child") {
        if (!element->parent) return false;
        for (int i = element->parent->children.size() - 1; i >= 0; i--) {
            if (element->parent->children[i]->getType() == NodeType::Element) {
                return element->parent->children[i].get() == element;
            }
        }
        return false;
    }
    else if (pseudo == "first-of-type") {
        if (!element->parent) return false;
        for (auto& child : element->parent->children) {
            if (child->getType() == NodeType::Element) {
                ElementNode* elem = static_cast<ElementNode*>(child.get());
                if (elem->getTagName() == element->getTagName()) {
                    return elem == element;
                }
            }
        }
        return false;
    }
    else if (pseudo.find("not(") == 0) {
        // Extract selector from :not(selector)
        size_t end = pseudo.rfind(')');
        if (end == std::string::npos) return true;
        
        std::string not_selector = pseudo.substr(4, end - 4);
        
        // Split by comma for multiple selectors
        std::vector<std::string> not_selectors = splitSelectors(not_selector);
        
        for (const std::string& sel : not_selectors) {
            if (matchesSimpleSelector(element, trim(sel))) {
                return false;  // Matches one of the :not() selectors, so fails
            }
        }
        
        return true;  // Doesn't match any :not() selector, so passes
    }
    else if (pseudo.find("is(") == 0 || pseudo.find("where(") == 0) {
        // Extract selectors from :is(sel1, sel2) or :where(sel1, sel2)
        size_t start = pseudo.find('(') + 1;
        size_t end = pseudo.rfind(')');
        if (end == std::string::npos) return false;
        
        std::string is_selector = pseudo.substr(start, end - start);
        std::vector<std::string> is_selectors = splitSelectors(is_selector);
        
        for (const std::string& sel : is_selectors) {
            if (matchesSimpleSelector(element, trim(sel))) {
                return true;  // Matches at least one selector
            }
        }
        
        return false;
    }
    
    // Unknown pseudo-class: return true to not break selector matching
    return true;
}

int StyleResolver::calculateSpecificity(const std::string& selector) {
    int specificity = 0;
    
    // Count IDs (100)
    for (char c : selector) {
        if (c == '#') specificity += 100;
    }
    
    // Count classes, attributes, pseudo-classes (10)
    for (char c : selector) {
        if (c == '.' || c == '[') specificity += 10;
    }
    
    // Count pseudo-classes
    size_t pos = 0;
    while ((pos = selector.find(':', pos)) != std::string::npos) {
        if (pos + 1 < selector.length() && selector[pos + 1] != ':') {
            specificity += 10;
        }
        pos++;
    }
    
    // Count elements (1)
    std::vector<SelectorPart> parts = parseComplexSelector(selector);
    for (const auto& part : parts) {
        // Very rough: if starts with letter, it's likely a tag
        if (!part.selector.empty() && std::isalpha(part.selector[0])) {
            specificity += 1;
        }
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
            unsigned int color_val = std::stoul(hex, nullptr, 16);
            return ComputedStyle::Color(
                (color_val >> 16) & 0xFF,
                (color_val >> 8) & 0xFF,
                color_val & 0xFF
            );
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