#include "css_selector_matcher.h"
#include <lexbor/html/interfaces/element.h>
#include <lexbor/dom/interfaces/element.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cctype>

namespace epub {

int CSSSelectorMatcher::calculateSpecificity(const std::string& selector) {
    int specificity = 0;
    
    size_t id_count = 0;
    size_t class_count = 0;
    size_t element_count = 0;
    
    bool in_brackets = false;
    bool in_pseudo = false;
    
    for (size_t i = 0; i < selector.length(); i++) {
        if (selector[i] == '[') {
            in_brackets = true;
            class_count++;
        } else if (selector[i] == ']') {
            in_brackets = false;
        } else if (selector[i] == ':' && i + 1 < selector.length() && selector[i+1] != ':') {
            in_pseudo = true;
            class_count++;
        } else if (in_pseudo && (selector[i] == ' ' || selector[i] == '>' || selector[i] == '+' || selector[i] == '~')) {
            in_pseudo = false;
        } else if (!in_brackets && !in_pseudo) {
            if (selector[i] == '#') {
                id_count++;
            } else if (selector[i] == '.') {
                class_count++;
            } else if (std::isalpha(selector[i]) && 
                       (i == 0 || !std::isalnum(selector[i-1]))) {
                element_count++;
            }
        }
    }
    
    specificity = (id_count * 100) + (class_count * 10) + element_count;
    
    return specificity;
}

bool CSSSelectorMatcher::matchesSelector(lxb_dom_node_t* node, const std::string& selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return false;
    }
    
    std::string selector_trimmed = trim(selector);
    
    if (selector_trimmed.empty()) {
        return false;
    }
    
    if (selector_trimmed.find('>') != std::string::npos) {
        return matchesChildSelector(node, selector_trimmed);
    }
    
    if (selector_trimmed.find('+') != std::string::npos) {
        return matchesAdjacentSelector(node, selector_trimmed);
    }
    
    if (selector_trimmed.find('~') != std::string::npos) {
        return matchesSiblingSelector(node, selector_trimmed);
    }
    
    if (selector_trimmed.find(' ') != std::string::npos) {
        return matchesDescendantSelector(node, selector_trimmed);
    }
    
    return matchesSimpleSelector(node, selector_trimmed);
}

bool CSSSelectorMatcher::matchesDescendantSelector(lxb_dom_node_t* node, const std::string& selector) {
    size_t last_space = selector.find_last_of(' ');
    std::string last_part = trim(selector.substr(last_space + 1));
    
    if (!matchesSimpleSelector(node, last_part)) {
        return false;
    }
    
    std::string ancestor_part = trim(selector.substr(0, last_space));
    
    lxb_dom_node_t* parent = lxb_dom_node_parent(node);
    while (parent && parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        if (matchesSelector(parent, ancestor_part)) {
            return true;
        }
        parent = lxb_dom_node_parent(parent);
    }
    
    return false;
}

bool CSSSelectorMatcher::matchesChildSelector(lxb_dom_node_t* node, const std::string& selector) {
    size_t child_pos = selector.find_last_of('>');
    std::string child_part = trim(selector.substr(child_pos + 1));
    
    if (!matchesSimpleSelector(node, child_part)) {
        return false;
    }
    
    std::string parent_part = trim(selector.substr(0, child_pos));
    
    lxb_dom_node_t* parent = lxb_dom_node_parent(node);
    if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return false;
    }
    
    return matchesSelector(parent, parent_part);
}

bool CSSSelectorMatcher::matchesAdjacentSelector(lxb_dom_node_t* node, const std::string& selector) {
    size_t plus_pos = selector.find_last_of('+');
    std::string next_part = trim(selector.substr(plus_pos + 1));
    
    if (!matchesSimpleSelector(node, next_part)) {
        return false;
    }
    
    std::string prev_part = trim(selector.substr(0, plus_pos));
    
    lxb_dom_node_t* prev_sibling = lxb_dom_node_prev(node);
    while (prev_sibling && prev_sibling->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        prev_sibling = lxb_dom_node_prev(prev_sibling);
    }
    
    if (!prev_sibling) {
        return false;
    }
    
    return matchesSelector(prev_sibling, prev_part);
}

bool CSSSelectorMatcher::matchesSiblingSelector(lxb_dom_node_t* node, const std::string& selector) {
    size_t tilde_pos = selector.find_last_of('~');
    std::string sibling_part = trim(selector.substr(tilde_pos + 1));
    
    if (!matchesSimpleSelector(node, sibling_part)) {
        return false;
    }
    
    std::string prev_part = trim(selector.substr(0, tilde_pos));
    
    lxb_dom_node_t* prev_sibling = lxb_dom_node_prev(node);
    while (prev_sibling) {
        if (prev_sibling->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            if (matchesSelector(prev_sibling, prev_part)) {
                return true;
            }
        }
        prev_sibling = lxb_dom_node_prev(prev_sibling);
    }
    
    return false;
}

bool CSSSelectorMatcher::matchesSimpleSelector(lxb_dom_node_t* node, const std::string& selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return false;
    }
    
    std::string selector_trimmed = trim(selector);
    
    if (selector_trimmed.empty() || selector_trimmed == "*") {
        return true;
    }
    
    size_t pseudo_pos = selector_trimmed.find(':');
    std::string base_selector = selector_trimmed;
    std::string pseudo_class;
    
    if (pseudo_pos != std::string::npos) {
        base_selector = selector_trimmed.substr(0, pseudo_pos);
        pseudo_class = selector_trimmed.substr(pseudo_pos);
    }
    
    if (!base_selector.empty() && !matchesBasicSelector(node, base_selector)) {
        return false;
    }
    
    if (!pseudo_class.empty() && !matchesPseudoClass(node, pseudo_class)) {
        return false;
    }
    
    return true;
}

bool CSSSelectorMatcher::matchesBasicSelector(lxb_dom_node_t* node, const std::string& selector) {
    std::string selector_lower = toLowerCase(trim(selector));
    
    if (selector_lower.empty() || selector_lower == "*") {
        return true;
    }
    
    std::string tag_name = toLowerCase(getTagName(node));
    std::string class_name = getClassName(node);
    std::string id_name = getIdName(node);
    
    size_t pos = 0;
    std::string expected_tag;
    
    if (selector_lower[0] != '.' && selector_lower[0] != '#' && selector_lower[0] != '[') {
        size_t tag_end = selector_lower.find_first_of(".#[");
        if (tag_end == std::string::npos) {
            tag_end = selector_lower.length();
        }
        expected_tag = selector_lower.substr(0, tag_end);
        pos = tag_end;
        
        if (!expected_tag.empty() && expected_tag != "*" && expected_tag != tag_name) {
            return false;
        }
    }
    
    while (pos < selector_lower.length()) {
        char current = selector_lower[pos];
        
        if (current == '#') {
            pos++;
            size_t end = selector_lower.find_first_of(".#[", pos);
            if (end == std::string::npos) {
                end = selector_lower.length();
            }
            std::string id_to_match = selector_lower.substr(pos, end - pos);
            if (id_name != id_to_match) {
                return false;
            }
            pos = end;
        }
        else if (current == '.') {
            pos++;
            size_t end = selector_lower.find_first_of(".#[", pos);
            if (end == std::string::npos) {
                end = selector_lower.length();
            }
            std::string class_to_match = selector_lower.substr(pos, end - pos);
            
            bool found = false;
            std::istringstream class_stream(class_name);
            std::string cls;
            while (class_stream >> cls) {
                if (toLowerCase(cls) == class_to_match) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                return false;
            }
            pos = end;
        }
        else if (current == '[') {
            size_t bracket_end = selector_lower.find(']', pos);
            if (bracket_end == std::string::npos) {
                return false;
            }
            
            std::string attr_expr = selector_lower.substr(pos + 1, bracket_end - pos - 1);
            
            size_t eq_pos = attr_expr.find('=');
            if (eq_pos != std::string::npos) {
                std::string attr_name = trim(attr_expr.substr(0, eq_pos));
                std::string attr_value = trim(attr_expr.substr(eq_pos + 1));
                
                if (!attr_value.empty() && (attr_value.front() == '"' || attr_value.front() == '\'')) {
                    attr_value = attr_value.substr(1, attr_value.length() - 2);
                }
                
                std::string actual_value = getAttributeValue(node, attr_name);
                if (toLowerCase(actual_value) != toLowerCase(attr_value)) {
                    return false;
                }
            } else {
                std::string attr_name = trim(attr_expr);
                if (getAttributeValue(node, attr_name).empty()) {
                    return false;
                }
            }
            
            pos = bracket_end + 1;
        }
        else {
            pos++;
        }
    }
    
    return true;
}

bool CSSSelectorMatcher::matchesPseudoClass(lxb_dom_node_t* node, const std::string& pseudo) {
    std::string pseudo_lower = toLowerCase(trim(pseudo));
    
    if (pseudo_lower == ":first-child") {
        lxb_dom_node_t* prev = lxb_dom_node_prev(node);
        while (prev && prev->type != LXB_DOM_NODE_TYPE_ELEMENT) {
            prev = lxb_dom_node_prev(prev);
        }
        return prev == nullptr;
    }
    
    if (pseudo_lower == ":last-child") {
        lxb_dom_node_t* next = lxb_dom_node_next(node);
        while (next && next->type != LXB_DOM_NODE_TYPE_ELEMENT) {
            next = lxb_dom_node_next(next);
        }
        return next == nullptr;
    }
    
    if (pseudo_lower == ":only-child") {
        lxb_dom_node_t* prev = lxb_dom_node_prev(node);
        while (prev && prev->type != LXB_DOM_NODE_TYPE_ELEMENT) {
            prev = lxb_dom_node_prev(prev);
        }
        
        lxb_dom_node_t* next = lxb_dom_node_next(node);
        while (next && next->type != LXB_DOM_NODE_TYPE_ELEMENT) {
            next = lxb_dom_node_next(next);
        }
        
        return (prev == nullptr && next == nullptr);
    }
    
    if (pseudo_lower == ":first-of-type") {
        std::string tag = getTagName(node);
        lxb_dom_node_t* prev = lxb_dom_node_prev(node);
        while (prev) {
            if (prev->type == LXB_DOM_NODE_TYPE_ELEMENT && getTagName(prev) == tag) {
                return false;
            }
            prev = lxb_dom_node_prev(prev);
        }
        return true;
    }
    
    if (pseudo_lower == ":last-of-type") {
        std::string tag = getTagName(node);
        lxb_dom_node_t* next = lxb_dom_node_next(node);
        while (next) {
            if (next->type == LXB_DOM_NODE_TYPE_ELEMENT && getTagName(next) == tag) {
                return false;
            }
            next = lxb_dom_node_next(next);
        }
        return true;
    }
    
    if (pseudo_lower.find(":not(") == 0) {
        size_t close_paren = pseudo_lower.find(')');
        if (close_paren != std::string::npos) {
            std::string not_selector = pseudo_lower.substr(5, close_paren - 5);
            return !matchesSimpleSelector(node, not_selector);
        }
    }
    
    return false;
}

std::string CSSSelectorMatcher::getTagName(lxb_dom_node_t* node) {
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

std::string CSSSelectorMatcher::getClassName(lxb_dom_node_t* node) {
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

std::string CSSSelectorMatcher::getIdName(lxb_dom_node_t* node) {
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

std::string CSSSelectorMatcher::getAttributeValue(lxb_dom_node_t* node, const std::string& attr_name) {
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

std::string CSSSelectorMatcher::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string CSSSelectorMatcher::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace epub