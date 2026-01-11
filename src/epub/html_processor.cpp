#include "html_processor.h"
#include "zip_handler.h"
#include "../utils/logger.h"
#include <lexbor/html/html.h>
#include <algorithm>
#include <cstring>

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
    
    if (zip && image_cache_) {
        extractAndLoadImages(document, zip, base_path);
    }
    
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
                                TextStyle current_style, TextAlign current_align,
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
            elem.style = current_style;
            elem.align = current_align;
            elem.list_level = list_level;
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
        TextStyle new_style = current_style;
        TextAlign new_align = current_align;
        int new_list_level = list_level;
        
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
        
        std::string inline_style = getAttributeValue(node, "style");
        if (!inline_style.empty()) {
            if (inline_style.find("font-weight") != std::string::npos &&
                (inline_style.find("bold") != std::string::npos || 
                 inline_style.find("700") != std::string::npos)) {
                new_style = new_style | TextStyle::Bold;
            }
            if (inline_style.find("font-style") != std::string::npos &&
                inline_style.find("italic") != std::string::npos) {
                new_style = new_style | TextStyle::Italic;
            }
            if (inline_style.find("text-decoration") != std::string::npos &&
                inline_style.find("underline") != std::string::npos) {
                new_style = new_style | TextStyle::Underline;
            }
            if (inline_style.find("text-align") != std::string::npos) {
                if (inline_style.find("center") != std::string::npos) {
                    new_align = TextAlign::Center;
                } else if (inline_style.find("right") != std::string::npos) {
                    new_align = TextAlign::Right;
                } else if (inline_style.find("justify") != std::string::npos) {
                    new_align = TextAlign::Justify;
                }
            }
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
        processNode(child, output, current_style, current_align, block_type, list_level);
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

void HTMLProcessor::extractStylesheets(lxb_html_document_t* document,
                                      std::vector<std::string>& stylesheets) {
    lxb_dom_collection_t* collection = lxb_dom_collection_create(&document->dom_document);
    if (!collection) return;
    
    lxb_status_t status = lxb_dom_collection_init(collection, 16);
    if (status != LXB_STATUS_OK) {
        lxb_dom_collection_destroy(collection, true);
        return;
    }
    
    lxb_html_head_element_t* head_element = lxb_html_document_head_element(document);
    if (!head_element) {
        lxb_dom_collection_destroy(collection, true);
        return;
    }
    
    lxb_dom_element_t* head = lxb_dom_interface_element(head_element);
    
    status = lxb_dom_elements_by_tag_name(
        head,
        collection,
        reinterpret_cast<const lxb_char_t*>("style"),
        5
    );
    
    if (status == LXB_STATUS_OK) {
        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++) {
            lxb_dom_element_t* element = lxb_dom_collection_element(collection, i);
            std::string css_text = getNodeText(lxb_dom_interface_node(element));
            if (!css_text.empty()) {
                stylesheets.push_back(css_text);
                LOG_DEBUG("Extracted stylesheet, length:", css_text.length());
            }
        }
    }
    
    lxb_dom_collection_destroy(collection, true);
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

ComputedStyle HTMLProcessor::computeStyle(lxb_dom_node_t* node, lxb_selectors_t* selectors,
                                         const std::vector<lxb_css_stylesheet_t*>& stylesheets) {
    ComputedStyle style;
    return style;
}

TextStyle HTMLProcessor::applyComputedStyle(const ComputedStyle& computed, TextStyle base_style) {
    TextStyle result = base_style;
    
    if (computed.bold) result = result | TextStyle::Bold;
    if (computed.italic) result = result | TextStyle::Italic;
    if (computed.underline) result = result | TextStyle::Underline;
    if (computed.strikethrough) result = result | TextStyle::Strikethrough;
    if (computed.monospace) result = result | TextStyle::Monospace;
    
    if (computed.vertical_align == ComputedStyle::VerticalAlign::Sub) {
        result = result | TextStyle::Subscript;
    } else if (computed.vertical_align == ComputedStyle::VerticalAlign::Super) {
        result = result | TextStyle::Superscript;
    }
    
    return result;
}

TextAlign HTMLProcessor::getTextAlign(const ComputedStyle& computed) {
    switch (computed.text_align) {
        case ComputedStyle::TextAlign::Left: return TextAlign::Left;
        case ComputedStyle::TextAlign::Right: return TextAlign::Right;
        case ComputedStyle::TextAlign::Center: return TextAlign::Center;
        case ComputedStyle::TextAlign::Justify: return TextAlign::Justify;
    }
    return TextAlign::Left;
}

} // namespace epub