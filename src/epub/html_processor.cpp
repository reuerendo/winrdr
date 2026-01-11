#include "html_processor.h"
#include "zip_handler.h"
#include "../utils/logger.h"
#include <lexbor/html/html.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cwctype>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace epub {

HTMLProcessor::HTMLProcessor() 
    : image_cache_(nullptr) 
{
}

HTMLProcessor::~HTMLProcessor() {
}

void HTMLProcessor::setImageCache(ImageCache* cache) {
    image_cache_ = cache;
}

FormattedContent HTMLProcessor::parse(const std::string& html, ZipHandler* zip,
                                     const std::string& base_path) {
    LOG_DEBUG("Parsing HTML with lexbor, length:", html.length());
    
    FormattedContent output;
    
    css_processor_.clear();
    
    lxb_html_document_t* document = lxb_html_document_create();
    if (!document) {
        LOG_ERROR("Failed to create HTML document");
        return output;
    }
    
    lxb_status_t status = lxb_html_document_parse(document, 
        reinterpret_cast<const lxb_char_t*>(html.c_str()), html.length());
    
    if (status != LXB_STATUS_OK) {
        LOG_ERROR("Failed to parse HTML document");
        lxb_html_document_destroy(document);
        return output;
    }
    
    LOG_DEBUG("HTML document parsed successfully");
    
    css_processor_.setDocument(document);
    
    if (zip && image_cache_) {
        extractAndLoadImages(document, zip, base_path);
    }
    
    if (zip) {
        extractStylesheets(document, zip, base_path);
    }
    
    LOG_DEBUG("CSS processor has", css_processor_.getRulesCount(), "rules loaded");
    
    lxb_dom_node_t* body = lxb_dom_interface_node(lxb_html_document_body_element(document));
    if (body) {
        processNode(body, output, TextStyle::Normal, TextAlign::Left, 
                   ElementType::Text, 0);
    }
    
    lxb_html_document_destroy(document);
    
    LOG_INFO("HTML parsing complete, elements:", output.size());
    
    return output;
}

void HTMLProcessor::processNode(lxb_dom_node_t* node, FormattedContent& output,
                                TextStyle inherited_style, TextAlign inherited_align,
                                ElementType block_type, int list_level) {
    if (!node) return;
    
    lxb_dom_node_type_t node_type = node->type;
    
    if (node_type == LXB_DOM_NODE_TYPE_TEXT) {
        std::string text = getNodeText(node);
        if (text.empty()) {
            goto process_children;
        }
        
        std::wstring wide_text;
#ifdef _WIN32
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (size > 0) {
            wide_text.resize(size - 1);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide_text[0], size);
        }
#else
        for (char c : text) {
            wide_text += static_cast<wchar_t>(static_cast<unsigned char>(c));
        }
#endif
        
        if (!wide_text.empty()) {
            TextElement elem;
            elem.type = block_type;
            elem.content = wide_text;
            elem.style = inherited_style;
            elem.align = inherited_align;
            elem.list_level = list_level;
            
            lxb_dom_node_t* parent = lxb_dom_node_parent(node);
            if (parent && parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                CSSComputedStyle parent_css = css_processor_.computeStyle(parent);
                elem.css_font_size = parent_css.font_size;
                elem.css_line_height = parent_css.line_height;
                elem.css_letter_spacing = parent_css.letter_spacing;
                elem.css_margin_top = parent_css.margin_top;
                elem.css_margin_bottom = parent_css.margin_bottom;
                elem.css_margin_left = parent_css.margin_left;
                elem.css_margin_right = parent_css.margin_right;
                elem.css_small_caps = parent_css.small_caps;
            }
            
            output.push_back(elem);
        }
        
        goto process_children;
    }
    
    if (node_type == LXB_DOM_NODE_TYPE_ELEMENT) {
        lxb_dom_element_t* element = lxb_dom_interface_element(node);
        const lxb_char_t* tag_name_raw = lxb_dom_element_qualified_name(element, nullptr);
        std::string tag_name(reinterpret_cast<const char*>(tag_name_raw));
        
        if (tag_name == "script" || tag_name == "style" || tag_name == "noscript") {
            return;
        }
        
        CSSComputedStyle css_style = css_processor_.computeStyle(node);
        
        if (css_style.display == CSSDisplay::None) {
            return;
        }
        
        std::string inline_style = getAttributeValue(node, "style");
        if (!inline_style.empty()) {
            css_processor_.addInlineStyle(element, inline_style);
            css_style = css_processor_.computeStyle(node);
        }
        
        if (tag_name == "br") {
            TextElement elem;
            elem.type = ElementType::LineBreak;
            output.push_back(elem);
            return;
        }
        
        if (tag_name == "hr") {
            TextElement elem;
            elem.type = ElementType::HorizontalRule;
            output.push_back(elem);
            return;
        }
        
        if (tag_name == "img") {
            std::string src = getAttributeValue(node, "src");
            if (!src.empty() && image_cache_) {
                const ImageData* img = image_cache_->getImage(src);
                if (img) {
                    TextElement elem;
                    elem.type = ElementType::Image;
                    elem.image_id = src;
                    output.push_back(elem);
                }
            }
            return;
        }
        
        ElementType new_block_type = block_type;
        TextStyle new_style = inherited_style;
        TextAlign new_align = inherited_align;
        int new_list_level = list_level;
        
        TextStyle css_text_style = css_processor_.convertToTextStyle(css_style);
        new_style = new_style | css_text_style;
        
        TextAlign css_text_align = css_processor_.convertToTextAlign(css_style);
        
        const bool is_block_element = (tag_name == "p" || tag_name == "div" || 
                                      tag_name == "h1" || tag_name == "h2" || 
                                      tag_name == "h3" || tag_name == "h4" || 
                                      tag_name == "h5" || tag_name == "h6" ||
                                      tag_name == "blockquote");
        
        if (is_block_element) {
            new_align = css_text_align;
        } else if (css_text_align != TextAlign::Left) {
            new_align = css_text_align;
        }
        
        if (tag_name == "p") {
            new_block_type = ElementType::Paragraph;
        } else if (tag_name == "h1") {
            new_block_type = ElementType::Heading1;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "h2") {
            new_block_type = ElementType::Heading2;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "h3") {
            new_block_type = ElementType::Heading3;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "h4") {
            new_block_type = ElementType::Heading4;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "h5") {
            new_block_type = ElementType::Heading5;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "h6") {
            new_block_type = ElementType::Heading6;
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "blockquote") {
            new_block_type = ElementType::Quote;
        } else if (tag_name == "pre" || tag_name == "code") {
            new_block_type = ElementType::CodeBlock;
            new_style = new_style | TextStyle::Monospace;
        } else if (tag_name == "li") {
            new_block_type = ElementType::ListItem;
        } else if (tag_name == "ul" || tag_name == "ol") {
            new_list_level++;
        }
        
        if (tag_name == "b" || tag_name == "strong") {
            new_style = new_style | TextStyle::Bold;
        } else if (tag_name == "i" || tag_name == "em" || tag_name == "cite") {
            new_style = new_style | TextStyle::Italic;
        } else if (tag_name == "u" || tag_name == "ins") {
            new_style = new_style | TextStyle::Underline;
        } else if (tag_name == "s" || tag_name == "strike" || tag_name == "del") {
            new_style = new_style | TextStyle::Strikethrough;
        } else if (tag_name == "code" || tag_name == "kbd" || tag_name == "tt") {
            new_style = new_style | TextStyle::Monospace;
        } else if (tag_name == "small") {
            new_style = new_style | TextStyle::Small;
        } else if (tag_name == "sub") {
            new_style = new_style | TextStyle::Subscript;
        } else if (tag_name == "sup") {
            new_style = new_style | TextStyle::Superscript;
        } else if (tag_name == "a") {
            new_block_type = ElementType::Link;
            new_style = new_style | TextStyle::Underline;
            std::string href = getAttributeValue(node, "href");
        }
        
        lxb_dom_node_t* child = lxb_dom_node_first_child(node);
        while (child) {
            processNode(child, output, new_style, new_align, new_block_type, new_list_level);
            child = lxb_dom_node_next(child);
        }
        
        if (tag_name == "p" || tag_name == "div" || tag_name == "blockquote" ||
            tag_name == "h1" || tag_name == "h2" || tag_name == "h3" ||
            tag_name == "h4" || tag_name == "h5" || tag_name == "h6" ||
            tag_name == "pre" || tag_name == "li") {
            if (!output.empty() && output.back().type != ElementType::LineBreak) {
                TextElement elem;
                elem.type = ElementType::LineBreak;
                output.push_back(elem);
            }
        }
        
        return;
    }
    
process_children:
    lxb_dom_node_t* child = lxb_dom_node_first_child(node);
    while (child) {
        processNode(child, output, inherited_style, inherited_align, block_type, list_level);
        child = lxb_dom_node_next(child);
    }
}

void HTMLProcessor::extractAndLoadImages(lxb_html_document_t* document, ZipHandler* zip,
                                        const std::string& base_path) {
    lxb_dom_collection_t* collection = lxb_dom_collection_create(&document->dom_document);
    if (!collection) return;
    
    lxb_status_t status = lxb_dom_collection_init(collection, 128);
    if (status != LXB_STATUS_OK) {
        lxb_dom_collection_destroy(collection, true);
        return;
    }
    
    lxb_html_body_element_t* body_element = lxb_html_document_body_element(document);
    if (!body_element) {
        lxb_dom_collection_destroy(collection, true);
        return;
    }
    
    lxb_dom_element_t* body = lxb_dom_interface_element(body_element);
    
    status = lxb_dom_elements_by_tag_name(
        body,
        collection,
        reinterpret_cast<const lxb_char_t*>("img"),
        3
    );
    
    if (status != LXB_STATUS_OK) {
        lxb_dom_collection_destroy(collection, true);
        return;
    }
    
    LOG_DEBUG("Found images:", lxb_dom_collection_length(collection));
    
    for (size_t i = 0; i < lxb_dom_collection_length(collection); i++) {
        lxb_dom_element_t* element = lxb_dom_collection_element(collection, i);
        
        std::string src = getAttributeValue(lxb_dom_interface_node(element), "src");
        if (src.empty()) continue;
        
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
    
    lxb_dom_collection_destroy(collection, true);
}

void HTMLProcessor::extractStylesheets(lxb_html_document_t* document, ZipHandler* zip,
                                      const std::string& base_path) {
    lxb_dom_collection_t* style_collection = lxb_dom_collection_create(&document->dom_document);
    if (!style_collection) return;
    
    lxb_status_t status = lxb_dom_collection_init(style_collection, 16);
    if (status != LXB_STATUS_OK) {
        lxb_dom_collection_destroy(style_collection, true);
        return;
    }
    
    lxb_html_head_element_t* head_element = lxb_html_document_head_element(document);
    if (head_element) {
        lxb_dom_element_t* head = lxb_dom_interface_element(head_element);
        
        status = lxb_dom_elements_by_tag_name(
            head,
            style_collection,
            reinterpret_cast<const lxb_char_t*>("style"),
            5
        );
        
        if (status == LXB_STATUS_OK) {
            for (size_t i = 0; i < lxb_dom_collection_length(style_collection); i++) {
                lxb_dom_element_t* element = lxb_dom_collection_element(style_collection, i);
                std::string css_text = getNodeText(lxb_dom_interface_node(element));
                if (!css_text.empty()) {
                    LOG_DEBUG("Parsing inline stylesheet, length:", css_text.length());
                    css_processor_.parseStylesheet(css_text);
                }
            }
        }
    }
    
    lxb_dom_collection_destroy(style_collection, true);
    
    lxb_dom_collection_t* link_collection = lxb_dom_collection_create(&document->dom_document);
    if (!link_collection) return;
    
    status = lxb_dom_collection_init(link_collection, 16);
    if (status != LXB_STATUS_OK) {
        lxb_dom_collection_destroy(link_collection, true);
        return;
    }
    
    if (head_element) {
        lxb_dom_element_t* head = lxb_dom_interface_element(head_element);
        
        status = lxb_dom_elements_by_tag_name(
            head,
            link_collection,
            reinterpret_cast<const lxb_char_t*>("link"),
            4
        );
        
        if (status == LXB_STATUS_OK) {
            for (size_t i = 0; i < lxb_dom_collection_length(link_collection); i++) {
                lxb_dom_element_t* element = lxb_dom_collection_element(link_collection, i);
                lxb_dom_node_t* node = lxb_dom_interface_node(element);
                
                std::string rel = getAttributeValue(node, "rel");
                if (rel != "stylesheet") continue;
                
                std::string href = getAttributeValue(node, "href");
                if (href.empty()) continue;
                
                std::string css_path = normalizePath(base_path, href);
                LOG_DEBUG("Loading external stylesheet:", css_path);
                
                std::string css_content = zip->extractTextFile(css_path);
                if (css_content.empty()) {
                    std::string fallback_path = href;
                    while (fallback_path.find("../") == 0) {
                        fallback_path = fallback_path.substr(3);
                    }
                    LOG_DEBUG("Trying fallback path:", fallback_path);
                    css_content = zip->extractTextFile(fallback_path);
                }
                
                if (!css_content.empty()) {
                    LOG_DEBUG("Parsing external stylesheet, length:", css_content.length());
                    css_processor_.parseStylesheet(css_content);
                } else {
                    LOG_WARNING("Failed to load external stylesheet:", css_path);
                }
            }
        }
    }
    
    lxb_dom_collection_destroy(link_collection, true);
}

std::string HTMLProcessor::getNodeText(lxb_dom_node_t* node) {
    if (!node) return "";
    
    lxb_dom_node_type_t node_type = node->type;
    
    if (node_type == LXB_DOM_NODE_TYPE_TEXT) {
        lxb_dom_text_t* text_node = lxb_dom_interface_text(node);
        size_t length = 0;
        const lxb_char_t* text_data = lxb_dom_node_text_content(node, &length);
        if (text_data && length > 0) {
            return std::string(reinterpret_cast<const char*>(text_data), length);
        }
    } else if (node_type == LXB_DOM_NODE_TYPE_ELEMENT) {
        std::string result;
        lxb_dom_node_t* child = lxb_dom_node_first_child(node);
        while (child) {
            result += getNodeText(child);
            child = lxb_dom_node_next(child);
        }
        return result;
    }
    
    return "";
}

std::string HTMLProcessor::getAttributeValue(lxb_dom_node_t* node, const char* attr_name) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>(attr_name),
        strlen(attr_name),
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

std::string HTMLProcessor::normalizePath(const std::string& base, const std::string& relative) {
    if (relative.empty()) return "";
    
    if (relative[0] == '/') return relative.substr(1);
    
    std::string path = relative;
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    if (!base.empty()) {
        return base + path;
    }
    
    return path;
}

std::wstring HTMLProcessor::applyTextTransform(const std::wstring& text, CSSTextTransform transform) {
    std::wstring result = text;
    
    switch (transform) {
        case CSSTextTransform::Uppercase:
            std::transform(result.begin(), result.end(), result.begin(), ::towupper);
            break;
        case CSSTextTransform::Lowercase:
            std::transform(result.begin(), result.end(), result.begin(), ::towlower);
            break;
        case CSSTextTransform::Capitalize:
            if (!result.empty()) {
                bool capitalize_next = true;
                for (size_t i = 0; i < result.length(); i++) {
                    if (capitalize_next && ::iswalpha(result[i])) {
                        result[i] = ::towupper(result[i]);
                        capitalize_next = false;
                    } else if (::iswspace(result[i])) {
                        capitalize_next = true;
                    }
                }
            }
            break;
        default:
            break;
    }
    
    return result;
}

} // namespace epub