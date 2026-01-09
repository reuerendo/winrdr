#pragma once

#include "dom_node.h"
#include <string>
#include <stack>
#include <memory>

namespace epub {

class DOMBuilder {
public:
    DOMBuilder();
    
    std::unique_ptr<DocumentNode> parse(const std::string& html);

private:
    struct ParserState {
        std::unique_ptr<DocumentNode> document;
        DOMNode* current_node;
        bool skip_content;
        std::stack<std::string> skip_tags;
    };
    
    void parseContent(const std::string& html, size_t& pos, ParserState& state);
    void handleTag(const std::string& html, size_t& pos, ParserState& state);
    void handleOpenTag(const std::string& tag_name, 
                      const std::unordered_map<std::string, std::string>& attributes,
                      bool self_closing, ParserState& state);
    void handleCloseTag(const std::string& tag_name, ParserState& state);
    void handleText(const std::string& text, ParserState& state);
    
    std::string extractTagName(const std::string& tag_content);
    std::unordered_map<std::string, std::string> extractAttributes(const std::string& tag_content);
    std::string decodeHTMLEntities(const std::string& text);
    std::string toLowerCase(const std::string& str);
    std::string trim(const std::string& str);
    
    bool isVoidElement(const std::string& tag);
    bool isSkipContentTag(const std::string& tag);
};

} // namespace epub