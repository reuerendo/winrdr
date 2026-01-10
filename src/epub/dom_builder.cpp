#include "dom_builder.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cctype>

namespace epub {

DOMBuilder::DOMBuilder() {}

std::unique_ptr<DocumentNode> DOMBuilder::parse(const std::string& html) {
    ParserState state;
    state.document = std::make_unique<DocumentNode>();
    state.current_node = state.document.get();
    state.skip_content = false;
    
    size_t pos = 0;
    
    // Skip XML declaration if present
    if (html.find("<?xml") == 0) {
        size_t xml_end = html.find("?>");
        if (xml_end != std::string::npos) {
            pos = xml_end + 2;
            LOG_DEBUG("Skipped XML declaration");
        }
    }
    
    parseContent(html, pos, state);
    
    return std::move(state.document);
}

void DOMBuilder::parseContent(const std::string& html, size_t& pos, ParserState& state) {
    while (pos < html.length()) {
        if (html[pos] == '<') {
            handleTag(html, pos, state);
        } else {
            size_t next_tag = html.find('<', pos);
            if (next_tag == std::string::npos) {
                next_tag = html.length();
            }
            
            std::string text = html.substr(pos, next_tag - pos);
            if (!state.skip_content && !text.empty()) {
                handleText(text, state);
            }
            
            pos = next_tag;
        }
    }
}

void DOMBuilder::handleTag(const std::string& html, size_t& pos, ParserState& state) {
    // Handle XML processing instructions (skip them)
    if (pos + 1 < html.length() && html[pos] == '<' && html[pos + 1] == '?') {
        size_t end = html.find("?>", pos + 2);
        if (end != std::string::npos) {
            pos = end + 2;
            return;
        }
    }
    
    // Handle comments
    if (pos + 3 < html.length() && html.substr(pos, 4) == "<!--") {
        size_t end = html.find("-->", pos + 4);
        if (end != std::string::npos) {
            pos = end + 3;
            return;
        }
    }
    
    // Handle DOCTYPE
    if (pos + 8 < html.length() && 
        (html.substr(pos, 9) == "<!DOCTYPE" || html.substr(pos, 9) == "<!doctype")) {
        size_t end = html.find('>', pos);
        if (end != std::string::npos) {
            pos = end + 1;
            return;
        }
    }
    
    size_t tag_end = html.find('>', pos);
    if (tag_end == std::string::npos) {
        pos = html.length();
        return;
    }
    
    std::string tag_content = html.substr(pos + 1, tag_end - pos - 1);
    
    // Closing tag
    if (!tag_content.empty() && tag_content[0] == '/') {
        std::string tag_name = toLowerCase(trim(tag_content.substr(1)));
        handleCloseTag(tag_name, state);
        pos = tag_end + 1;
        return;
    }
    
    // Self-closing or opening tag
    bool self_closing = false;
    if (!tag_content.empty() && tag_content.back() == '/') {
        self_closing = true;
        tag_content = tag_content.substr(0, tag_content.length() - 1);
    }
    
    std::string tag_name = extractTagName(tag_content);
    auto attributes = extractAttributes(tag_content);
    
    // Void elements are always self-closing
    if (isVoidElement(tag_name)) {
        self_closing = true;
    }
    
    handleOpenTag(tag_name, attributes, self_closing, state);
    
    pos = tag_end + 1;
}

void DOMBuilder::handleOpenTag(const std::string& tag_name,
                               const std::unordered_map<std::string, std::string>& attributes,
                               bool self_closing, ParserState& state) {
    // Check if we should skip content
    if (isSkipContentTag(tag_name)) {
        state.skip_content = true;
        state.skip_tags.push(tag_name);
        return;
    }
    
    // Create element node
    auto element = std::make_unique<ElementNode>(tag_name);
    
    for (const auto& attr : attributes) {
        element->setAttribute(attr.first, attr.second);
    }
    
    ElementNode* element_ptr = element.get();
    element_ptr->parent = state.current_node;
    
    state.current_node->children.push_back(std::move(element));
    
    // If not self-closing, make this the current node
    if (!self_closing) {
        state.current_node = element_ptr;
    }
}

void DOMBuilder::handleCloseTag(const std::string& tag_name, ParserState& state) {
    // Handle skip tags
    if (!state.skip_tags.empty() && state.skip_tags.top() == tag_name) {
        state.skip_tags.pop();
        if (state.skip_tags.empty()) {
            state.skip_content = false;
        }
        return;
    }
    
    if (state.skip_content) {
        return;
    }
    
    // Find matching parent element
    DOMNode* node = state.current_node;
    while (node && node->getType() == NodeType::Element) {
        ElementNode* elem = static_cast<ElementNode*>(node);
        if (elem->getTagName() == tag_name) {
            state.current_node = node->parent ? node->parent : state.document.get();
            return;
        }
        node = node->parent;
    }
    
    // Tag mismatch - just ignore
}

void DOMBuilder::handleText(const std::string& text, ParserState& state) {
    std::string decoded = decodeHTMLEntities(text);
    
    if (decoded.empty()) {
        return;
    }
    
    auto text_node = std::make_unique<TextNode>(decoded);
    text_node->parent = state.current_node;
    state.current_node->children.push_back(std::move(text_node));
}

std::string DOMBuilder::extractTagName(const std::string& tag_content) {
    size_t space = tag_content.find(' ');
    size_t tab = tag_content.find('\t');
    size_t newline = tag_content.find('\n');
    
    size_t end = std::min({space, tab, newline});
    if (end == std::string::npos) {
        end = tag_content.length();
    }
    
    return toLowerCase(trim(tag_content.substr(0, end)));
}

std::unordered_map<std::string, std::string> DOMBuilder::extractAttributes(const std::string& tag_content) {
    std::unordered_map<std::string, std::string> attributes;
    
    size_t space_pos = tag_content.find(' ');
    if (space_pos == std::string::npos) {
        return attributes;
    }
    
    std::string attrs_str = tag_content.substr(space_pos + 1);
    size_t pos = 0;
    
    while (pos < attrs_str.length()) {
        // Skip whitespace
        while (pos < attrs_str.length() && std::isspace(attrs_str[pos])) {
            pos++;
        }
        
        if (pos >= attrs_str.length()) break;
        
        // Find attribute name
        size_t name_start = pos;
        while (pos < attrs_str.length() && attrs_str[pos] != '=' && 
               !std::isspace(attrs_str[pos])) {
            pos++;
        }
        
        std::string name = toLowerCase(trim(attrs_str.substr(name_start, pos - name_start)));
        
        if (name.empty()) break;
        
        // Skip whitespace and '='
        while (pos < attrs_str.length() && std::isspace(attrs_str[pos])) {
            pos++;
        }
        
        if (pos >= attrs_str.length() || attrs_str[pos] != '=') {
            // Boolean attribute
            attributes[name] = "";
            continue;
        }
        
        pos++; // Skip '='
        
        // Skip whitespace
        while (pos < attrs_str.length() && std::isspace(attrs_str[pos])) {
            pos++;
        }
        
        if (pos >= attrs_str.length()) break;
        
        // Parse value
        std::string value;
        if (attrs_str[pos] == '"' || attrs_str[pos] == '\'') {
            char quote = attrs_str[pos];
            pos++;
            size_t value_start = pos;
            while (pos < attrs_str.length() && attrs_str[pos] != quote) {
                pos++;
            }
            value = attrs_str.substr(value_start, pos - value_start);
            if (pos < attrs_str.length()) pos++; // Skip closing quote
        } else {
            size_t value_start = pos;
            while (pos < attrs_str.length() && !std::isspace(attrs_str[pos])) {
                pos++;
            }
            value = attrs_str.substr(value_start, pos - value_start);
        }
        
        attributes[name] = value;
    }
    
    return attributes;
}

std::string DOMBuilder::decodeHTMLEntities(const std::string& text) {
    std::string result;
    
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '&') {
            bool decoded = false;
            
            // Common entities
            const struct { const char* entity; char replacement; size_t len; } entities[] = {
                {"&nbsp;", ' ', 6},
                {"&lt;", '<', 4},
                {"&gt;", '>', 4},
                {"&amp;", '&', 5},
                {"&quot;", '"', 6},
                {"&apos;", '\'', 6},
                {"&#160;", ' ', 6}
            };
            
            for (const auto& ent : entities) {
                if (i + ent.len <= text.length() && 
                    text.substr(i, ent.len) == ent.entity) {
                    result += ent.replacement;
                    i += ent.len - 1;
                    decoded = true;
                    break;
                }
            }
            
            // Numeric entities &#NNNN; or &#xHHHH;
            if (!decoded && i + 3 < text.length() && text[i + 1] == '#') {
                size_t end = text.find(';', i + 2);
                if (end != std::string::npos && end - i <= 8) {
                    std::string num_str = text.substr(i + 2, end - i - 2);
                    int code = 0;
                    
                    try {
                        if (!num_str.empty() && (num_str[0] == 'x' || num_str[0] == 'X')) {
                            // Hexadecimal
                            code = std::stoi(num_str.substr(1), nullptr, 16);
                        } else {
                            // Decimal
                            code = std::stoi(num_str);
                        }
                        
                        if (code > 0 && code < 128) {
                            result += static_cast<char>(code);
                            i = end;
                            decoded = true;
                        }
                    } catch (...) {
                        // Invalid numeric entity
                    }
                }
            }
            
            if (!decoded) {
                result += text[i];
            }
        } else {
            result += text[i];
        }
    }
    
    return result;
}

std::string DOMBuilder::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string DOMBuilder::trim(const std::string& str) {
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

bool DOMBuilder::isVoidElement(const std::string& tag) {
    static const std::vector<std::string> void_elements = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    
    return std::find(void_elements.begin(), void_elements.end(), tag) != void_elements.end();
}

bool DOMBuilder::isSkipContentTag(const std::string& tag) {
    return tag == "script" || tag == "style" || tag == "noscript";
}

} // namespace epub