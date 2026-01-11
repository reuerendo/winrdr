#pragma once

#include "formatted_text.h"
#include "image_cache.h"
#include <string>
#include <vector>
#include <memory>

// Forward declarations for lexbor
typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_css_stylesheet lxb_css_stylesheet_t;
typedef struct lxb_css_selector lxb_css_selector_t;
typedef struct lxb_selectors lxb_selectors_t;

namespace epub {

class ZipHandler;

struct ComputedStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool monospace = false;
    float font_size_multiplier = 1.0f;
    
    enum class VerticalAlign { Baseline, Sub, Super } vertical_align = VerticalAlign::Baseline;
    enum class TextAlign { Left, Right, Center, Justify } text_align = TextAlign::Left;
    enum class Display { None, Block, Inline } display = Display::Inline;
};

class HTMLProcessor {
public:
    HTMLProcessor();
    ~HTMLProcessor();
    
    FormattedContent parse(const std::string& html, ZipHandler* zip, 
                          const std::string& base_path);
    
    void setImageCache(ImageCache* cache);

private:
    void processNode(lxb_dom_node_t* node, FormattedContent& output, 
                    TextStyle current_style, TextAlign current_align,
                    ElementType block_type, int list_level);
    
    void extractAndLoadImages(lxb_html_document_t* document, ZipHandler* zip,
                             const std::string& base_path);
    
    void extractStylesheets(lxb_html_document_t* document, 
                           std::vector<std::string>& stylesheets);
    
    ComputedStyle computeStyle(lxb_dom_node_t* node, lxb_selectors_t* selectors,
                              const std::vector<lxb_css_stylesheet_t*>& stylesheets);
    
    TextStyle applyComputedStyle(const ComputedStyle& computed, TextStyle base_style);
    TextAlign getTextAlign(const ComputedStyle& computed);
    
    std::string normalizePath(const std::string& base, const std::string& relative);
    std::string getNodeText(lxb_dom_node_t* node);
    std::string getAttributeValue(lxb_dom_node_t* node, const char* attr_name);
    
    ImageCache* image_cache_;
};

} // namespace epub