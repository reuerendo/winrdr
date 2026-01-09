#pragma once

#include "formatted_text.h"
#include "css_parser.h"
#include "image_cache.h"
#include <string>
#include <stack>
#include <memory>

namespace epub {

class ZipHandler;

class HTMLParser {
public:
    HTMLParser();
    ~HTMLParser();
    
    FormattedContent parse(const std::string& html, ZipHandler* zip, 
                          const std::string& base_path);
    
    void setImageCache(ImageCache* cache);

private:
    struct ParseContext {
        TextStyle current_style;
        TextAlign current_align;
        ElementType current_element;
        int list_level;
        bool in_paragraph;
        
        ParseContext() 
            : current_style(TextStyle::Normal)
            , current_align(TextAlign::Left)
            , current_element(ElementType::Text)
            , list_level(0)
            , in_paragraph(false) 
        {}
    };
    
    void parseNode(const std::string& html, size_t& pos, FormattedContent& content,
                   ParseContext& ctx, ZipHandler* zip, const std::string& base_path);
    
    void handleOpenTag(const std::string& tag, const std::string& attributes,
                      ParseContext& ctx, FormattedContent& content,
                      ZipHandler* zip, const std::string& base_path);
    
    void handleCloseTag(const std::string& tag, ParseContext& ctx,
                       FormattedContent& content);
    
    void addText(const std::string& text, ParseContext& ctx,
                FormattedContent& content);
    
    bool isBlockElement(const std::string& tag) const;
    
    std::string extractTagName(const std::string& tag_content);
    std::string extractAttribute(const std::string& tag_content, 
                                 const std::string& attr_name);
    
    std::string decodeHTMLEntities(const std::string& text);
    std::wstring utf8ToWide(const std::string& str);
    std::string normalizePath(const std::string& base, const std::string& relative);
    
    ImageCache* image_cache_;
    CSSParser css_parser_;
    std::stack<ParseContext> context_stack_;
};

} // namespace epub