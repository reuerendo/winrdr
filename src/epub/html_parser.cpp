#include "html_parser.h"
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
    LOG_INFO("=== Starting HTML parsing ===");
    
    // Step 1: Build DOM tree
    auto document = dom_builder_.parse(html);
    
    LOG_DEBUG("DOM tree built, children:", document->children.size());
    
    // Step 2: Extract and parse CSS
    style_resolver_.clear();
    
    // Extract inline <style> tags and <link> tags
    std::vector<ElementNode*> style_elements;
    std::vector<ElementNode*> link_elements;
    std::vector<DOMNode*> queue;
    queue.push_back(document.get());
    
    while (!queue.empty()) {
        DOMNode* node = queue.back();
        queue.pop_back();
        
        if (node->getType() == NodeType::Element) {
            ElementNode* element = static_cast<ElementNode*>(node);
            
            if (element->getTagName() == "style") {
                style_elements.push_back(element);
            } else if (element->getTagName() == "link") {
                std::string rel = element->getAttribute("rel");
                if (rel == "stylesheet") {
                    link_elements.push_back(element);
                }
            }
        }
        
        for (auto& child : node->children) {
            queue.push_back(child.get());
        }
    }
    
    LOG_DEBUG("Found <style> tags:", style_elements.size(), "<link> stylesheets:", link_elements.size());
    
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
            style_resolver_.addStylesheet(css);
        }
    }
    
    // Load and parse external CSS files
    if (zip) {
        for (ElementNode* link_elem : link_elements) {
            std::string href = link_elem->getAttribute("href");
            if (!href.empty()) {
                loadExternalStylesheet(href, zip, base_path);
            }
        }
    }
    
    // Step 3: Resolve styles (cascade + compute)
    style_resolver_.resolveStyles(document.get());
    
    // Step 4: Load images
    if (zip && image_cache_) {
        extractAndLoadImages(document.get(), zip, base_path);
    }
    
    // Step 5: Layout
    FormattedContent content = layout_engine_.layout(document.get(), image_cache_);
    
    LOG_INFO("=== HTML parsing complete, elements:", content.size(), "===");
    
    return content;
}

void HTMLParserNew::loadExternalStylesheet(const std::string& href, ZipHandler* zip,
                                           const std::string& base_path) {
    std::string css_path = normalizePath(base_path, href);
    
    LOG_INFO("Loading external stylesheet:", css_path);
    
    std::string css_content = zip->extractTextFile(css_path);
    
    if (css_content.empty()) {
        // Try without base path
        css_path = href;
        while (css_path.find("../") == 0) {
            css_path = css_path.substr(3);
        }
        
        LOG_DEBUG("Retrying with path:", css_path);
        css_content = zip->extractTextFile(css_path);
    }
    
    if (!css_content.empty()) {
        LOG_INFO("External stylesheet loaded, length:", css_content.length());
        style_resolver_.addStylesheet(css_content);
    } else {
        LOG_WARNING("Failed to load external stylesheet:", href);
    }
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
    
    LOG_INFO("Found images:", img_elements.size());
    
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
    std::string current_base = base;
    
    while (path.find("../") == 0) {
        path = path.substr(3);
        
        // Remove last directory from base
        if (!current_base.empty()) {
            size_t last_slash = current_base.find_last_of('/', current_base.length() - 2);
            if (last_slash != std::string::npos) {
                current_base = current_base.substr(0, last_slash + 1);
            } else {
                current_base.clear();
            }
        }
    }
    
    // Combine with base
    if (!current_base.empty()) {
        return current_base + path;
    }
    
    return path;
}

} // namespace epub