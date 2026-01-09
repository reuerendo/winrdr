#pragma once

#include "formatted_text.h"
#include "image_cache.h"
#include "dom_node.h"
#include "dom_builder.h"
#include "style_resolver.h"
#include "layout_engine.h"
#include <string>
#include <memory>

namespace epub {

class ZipHandler;

class HTMLParserNew {
public:
    HTMLParserNew();
    ~HTMLParserNew();
    
    FormattedContent parse(const std::string& html, ZipHandler* zip, 
                          const std::string& base_path);
    
    void setImageCache(ImageCache* cache);

private:
    void loadExternalStylesheet(const std::string& href, ZipHandler* zip,
                               const std::string& base_path);
    void extractAndLoadImages(DocumentNode* document, ZipHandler* zip, 
                             const std::string& base_path);
    void processImageNode(ElementNode* element, ZipHandler* zip, 
                         const std::string& base_path);
    std::string normalizePath(const std::string& base, const std::string& relative);
    
    DOMBuilder dom_builder_;
    StyleResolver style_resolver_;
    LayoutEngine layout_engine_;
    ImageCache* image_cache_;
};

} // namespace epub