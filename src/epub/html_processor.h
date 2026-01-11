#pragma once

#include "formatted_text.h"
#include "image_cache.h"
#include "css_processor.h"
#include <string>
#include <vector>
#include <memory>

typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_dom_node lxb_dom_node_t;

namespace epub {

class ZipHandler;

class HTMLProcessor {
public:
    HTMLProcessor();
    ~HTMLProcessor();
    
    FormattedContent parse(const std::string& html, ZipHandler* zip, 
                          const std::string& base_path);
    
    void setImageCache(ImageCache* cache);
    
    CSSProcessor& getCSSProcessor() { return css_processor_; }
    const CSSProcessor& getCSSProcessor() const { return css_processor_; }

private:
    void processNode(lxb_dom_node_t* node, FormattedContent& output, 
                    TextStyle inherited_style, TextAlign inherited_align,
                    ElementType block_type, int list_level);
    
    void extractAndLoadImages(lxb_html_document_t* document, ZipHandler* zip,
                             const std::string& base_path);
    
    void extractStylesheets(lxb_html_document_t* document, ZipHandler* zip,
                           const std::string& base_path);
    
    std::string normalizePath(const std::string& base, const std::string& relative);
    std::string getNodeText(lxb_dom_node_t* node);
    std::string getAttributeValue(lxb_dom_node_t* node, const char* attr_name);
    
    std::wstring applyTextTransform(const std::wstring& text, CSSTextTransform transform);
    
    ImageCache* image_cache_;
    CSSProcessor css_processor_;
};

} // namespace epub