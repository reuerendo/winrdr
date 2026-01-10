#pragma once

#include "dom_node.h"
#include "dom_builder.h"
#include "style_resolver.h"
#include "layout_engine.h"
#include "image_cache.h"
#include <string>
#include <memory>
#include <vector>

namespace epub {

class ZipHandler;

class HTMLParserNew {
public:
    HTMLParserNew();
    ~HTMLParserNew();
    
    std::vector<RenderLine> parse(const std::string& html, ZipHandler* zip, 
                                  const std::string& base_path,
                                  int viewport_width, int default_font_size);
    
    void setImageCache(ImageCache* cache);

private:
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