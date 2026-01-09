#include "html_parser_new.h"
#include "zip_handler.h"
#include "../utils/logger.h"
#include <algorithm>

namespace epub {

HTMLParserNew::HTMLParserNew() : image_cache_(nullptr) {}

HTMLParserNew::~HTMLParserNew() {}

void HTMLParserNew::setImageCache(ImageCache* cache) {
    image_cache_ = cache;
}

FormattedContent HTMLParserNew::parse(const std::string& html, ZipHandler* zip,
                                     const std::string& base_path) {
    LOG_DEBUG("Parsing HTML, length:", html.length());
    
    // Step 1: Build DOM tree
    auto document = dom_builder_.parse(html);
    
    LOG_DEBUG("DOM tree built, children:", document->children.size());
    
    // Step 2: Extract and parse CSS
    style_resolver_.clear();
    
    // Extract inline <style> tags
    std::vector<ElementNode*> style_elements;
    std::vector<DOMNode*> queue;
    queue.push_back(document.get());
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            
            if (element->getTagName() == "style") {
                style_elements.push_back(element);
            }
        }
        
        for (auto& child : node->children) {
            queue.push_back(child.get());
        }
    }
    
    // Parse CSS from <style> tags
    for (ElementNode* style_elem : style_elements) {
        std::string css;
        for (auto& child : style_elem->children) {
            if (child->getType() == NodeType::Text) {
                TextNode* text = static_cast<TextNode*>(child.get());
                css += text->getText();
            }
        }
        
        if (!css.empty()) {
            LOG_DEBUG("Parsing CSS stylesheet, length:", css.length());
            style_resolver_.addStylesheet(css);
        }
    }
    
    // Step 3: Resolve styles (cascade + compute)
    style_resolver_.resolveStyles(document.get());
    
    LOG_DEBUG("Styles resolved");
    
    // Step 4: Load images
    if (zip && image_cache_) {
        extractAndLoadImages(document.get(), zip, base_path);
    }
    
    // Step 5: Layout
    FormattedContent content = layout_engine_.layout(document.get(), image_cache_);
    
    LOG_INFO("HTML parsing complete, elements:", content.size());
    
    return content;
}

void HTMLParserNew::extractAndLoadImages(DocumentNode* document, ZipHandler* zip,
                                        const std::string& base_path) {
    std::vector<ElementNode*> img_elements;
    std::vector<DOMNode*> queue;
    queue.push_back(document);
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            
            if (element->getTagName() == "img") {
                img_elements.push_back(element);
            }
        }
        
        for (auto& child : node->children) {
            queue.push_back(child.get());
        }
    }
    
    LOG_DEBUG("Found images:", img_elements.size());
    
    for (ElementNode* img : img_elements) {
        processImageNode(img, zip, base_path);
    }
}

void HTMLParserNew::processImageNode(ElementNode* element, ZipHandler* zip,
                                    const std::string& base_path) {
    std::string src = element->getAttribute("src");
    if (src.empty()) return;
    
    std::string img_path = normalizePath(base_path, src);
    
    LOG_DEBUG("Loading image:", img_path);
    
    std::vector<char> img_data;
    if (zip->extractFile(img_path, img_data)) {
        if (image_cache_->loadImage(img_path, img_data)) {
            LOG_DEBUG("Image loaded successfully:", img_path);
        } else {
            LOG_WARNING("Failed to load image:", img_path);
        }
    } else {
        LOG_WARNING("Failed to extract image file:", img_path);
    }
}

std::string HTMLParserNew::normalizePath(const std::string& base, const std::string& relative) {
    if (relative.empty()) return "";
    
    // Absolute path
    if (relative[0] == '/') return relative.substr(1);
    
    // Remove leading ../
    std::string path = relative;
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    // Combine with base
    if (!base.empty()) {
        return base + path;
    }
    
    return path;
}

} // namespace epub