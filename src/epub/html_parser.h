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
    struct StyleState {
        TextStyle style;
        ElementType element_type;
        TextAlign align;
        bool skip;
        
        StyleState()
            : style(TextStyle::Normal)
            , element_type(ElementType::Text)
            , align(TextAlign::Left)
            , skip(false)
        {}
    };
    
    struct ParseContext {
        TextStyle current_style;
        TextAlign current_align;
        ElementType block_element_type;
        int list_level;
        bool skip_content;
        std::wstring current_text;
        
        ParseContext() 
            : current_style(TextStyle::Normal)
            , current_align(TextAlign::Left)
            , block_element_type(ElementType::Text)
            , list_level(0)
            , skip_content(false)
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
    
    bool isBlockElement(const std::string& tag);
    void flushCurrentText(ParseContext& ctx, FormattedContent& content);
    
    std::string extractTagName(const std::string& tag_content);
    std::string extractAttribute(const std::string& tag_content, 
                                 const std::string& attr_name);
    
    std::string decodeHTMLEntities(const std::string& text);
    std::wstring utf8ToWide(const std::string& str);
    std::string normalizePath(const std::string& base, const std::string& relative);
    
    ImageCache* image_cache_;
    CSSParser css_parser_;
    std::stack<StyleState> style_stack_;
};

} // namespace epub