#include "layout_engine.h"
#include "../utils/logger.h"
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace epub {

LayoutEngine::LayoutEngine() 
    : image_cache_(nullptr)
    , current_inline_style_(TextStyle::Normal)
    , current_inline_align_(TextAlign::Left)
    , current_block_type_(ElementType::Text)
    , current_list_level_(0)
    , current_block_node_(nullptr)
    , in_inline_context_(false)
{}

FormattedContent LayoutEngine::layout(DocumentNode* document, ImageCache* image_cache) {
    output_.clear();
    image_cache_ = image_cache;
    current_inline_text_.clear();
    current_block_node_ = nullptr;
    in_inline_context_ = false;
    
    for (auto& child : document->children) {
        layoutNode(child.get(), 0);
    }
    
    flushInlineContent();
    
    LOG_INFO("Layout complete, elements:", output_.size());
    return output_;
}

void LayoutEngine::layoutNode(DOMNode* node, int list_level) {
    if (!node) return;
    
    if (node->computed_style.display == DisplayType::None) {
        return;
    }
    
    if (node->getType() == NodeType::Element) {
        layoutElement(static_cast<ElementNode*>(node), list_level);
    } else if (node->getType() == NodeType::Text) {
        layoutText(static_cast<TextNode*>(node));
    }
}

void LayoutEngine::layoutElement(ElementNode* element, int list_level) {
    const std::string& tag = element->getTagName();
    const ComputedStyle& style = element->computed_style;
    
    if (tag == "br") {
        if (in_inline_context_) {
            flushInlineContent();
        }
        addLineBreak();
        return;
    }
    
    if (tag == "hr") {
        flushInlineContent();
        TextElement elem;
        elem.type = ElementType::HorizontalRule;
        elem.margin_top = style.margin_top;
        elem.margin_bottom = style.margin_bottom;
        output_.push_back(elem);
        addLineBreak();
        return;
    }
    
    if (tag == "img") {
        flushInlineContent();
        
        std::string src = element->getAttribute("src");
        if (!src.empty() && image_cache_) {
            const ImageData* img = image_cache_->getImage(src);
            if (img) {
                TextElement elem;
                elem.type = ElementType::Image;
                elem.image_id = src;
                elem.margin_top = style.margin_top;
                elem.margin_bottom = style.margin_bottom;
                output_.push_back(elem);
                addLineBreak();
            }
        }
        return;
    }
    
    if (style.display == DisplayType::Block || 
        style.display == DisplayType::ListItem) {
        
        flushInlineContent();
        
        ElementType old_block_type = current_block_type_;
        TextAlign old_align = current_inline_align_;
        DOMNode* old_block_node = current_block_node_;
        
        // Set current block node to get CSS properties
        current_block_node_ = element;
        
        if (tag == "p") {
            current_block_type_ = ElementType::Paragraph;
        } else if (tag == "h1") {
            current_block_type_ = ElementType::Heading1;
        } else if (tag == "h2") {
            current_block_type_ = ElementType::Heading2;
        } else if (tag == "h3") {
            current_block_type_ = ElementType::Heading3;
        } else if (tag == "h4") {
            current_block_type_ = ElementType::Heading4;
        } else if (tag == "h5") {
            current_block_type_ = ElementType::Heading5;
        } else if (tag == "h6") {
            current_block_type_ = ElementType::Heading6;
        } else if (tag == "blockquote") {
            current_block_type_ = ElementType::Quote;
        } else if (tag == "li") {
            current_block_type_ = ElementType::ListItem;
        } else if (tag == "pre") {
            current_block_type_ = ElementType::CodeBlock;
        } else {
            current_block_type_ = ElementType::Paragraph;
        }
        
        current_inline_align_ = computeTextAlign(style);
        
        int new_list_level = list_level;
        if (tag == "ul" || tag == "ol") {
            new_list_level++;
        }
        
        for (auto& child : element->children) {
            layoutNode(child.get(), new_list_level);
        }
        
        flushInlineContent();
        
        if (current_block_type_ != ElementType::Text && !output_.empty()) {
            if (output_.back().type != ElementType::LineBreak) {
                addLineBreak();
            }
        }
        
        current_block_type_ = old_block_type;
        current_inline_align_ = old_align;
        current_block_node_ = old_block_node;
    }
    else if (style.display == DisplayType::Inline || 
             style.display == DisplayType::InlineBlock) {
        
        bool was_inline = in_inline_context_;
        in_inline_context_ = true;
        
        TextStyle old_style = current_inline_style_;
        
        TextStyle new_style = current_inline_style_;
        
        if (style.bold) {
            new_style = new_style | TextStyle::Bold;
        }
        if (style.italic) {
            new_style = new_style | TextStyle::Italic;
        }
        if (style.underline) {
            new_style = new_style | TextStyle::Underline;
        }
        if (style.strikethrough) {
            new_style = new_style | TextStyle::Strikethrough;
        }
        if (style.monospace) {
            new_style = new_style | TextStyle::Monospace;
        }
        if (style.font_size_multiplier < 0.9f) {
            new_style = new_style | TextStyle::Small;
        }
        if (style.vertical_align == ComputedStyle::VerticalAlign::Sub) {
            new_style = new_style | TextStyle::Subscript;
        }
        if (style.vertical_align == ComputedStyle::VerticalAlign::Super) {
            new_style = new_style | TextStyle::Superscript;
        }
        
        current_inline_style_ = new_style;
        
        if (tag == "q") {
            current_inline_text_ += L"\"";
        }
        
        for (auto& child : element->children) {
            layoutNode(child.get(), list_level);
        }
        
        if (tag == "q") {
            current_inline_text_ += L"\"";
        }
        
        current_inline_style_ = old_style;
        in_inline_context_ = was_inline;
    }
}

void LayoutEngine::layoutText(TextNode* text) {
    if (!in_inline_context_) {
        in_inline_context_ = true;
    }
    
    const std::string& utf8_text = text->getText();
    std::wstring wide_text = utf8ToWide(utf8_text);
    
    ComputedStyle::WhiteSpace ws = ComputedStyle::WhiteSpace::Normal;
    if (text->parent) {
        ws = text->parent->computed_style.white_space;
    }
    
    wide_text = processWhitespace(wide_text, ws);
    
    if (!wide_text.empty()) {
        current_inline_text_ += wide_text;
    }
}

void LayoutEngine::flushInlineContent() {
    if (current_inline_text_.empty()) {
        return;
    }
    
    TextElement elem;
    elem.type = current_block_type_;
    elem.content = current_inline_text_;
    elem.style = current_inline_style_;
    elem.align = current_inline_align_;
    elem.list_level = current_list_level_;
    
    // Copy CSS spacing from current block node
    if (current_block_node_) {
        const ComputedStyle& cs = current_block_node_->computed_style;
        
        // Spacing
        elem.margin_top = cs.margin_top;
        elem.margin_bottom = cs.margin_bottom;
        elem.padding_left = cs.padding_left;
        elem.text_indent = cs.padding_left; // For now, use padding_left as text-indent
        elem.has_css_spacing = true;
        
        // Text properties
        elem.font_size_multiplier = cs.font_size_multiplier;
        elem.line_height = cs.line_height;
        
        // Text color
        elem.text_color_r = cs.text_color.r;
        elem.text_color_g = cs.text_color.g;
        elem.text_color_b = cs.text_color.b;
        elem.has_text_color = (cs.text_color.r != 0 || cs.text_color.g != 0 || cs.text_color.b != 0);
        
        LOG_DEBUG("Element CSS applied:", "type=", (int)elem.type, 
                  "margin_bottom=", elem.margin_bottom, 
                  "font_size=", elem.font_size_multiplier,
                  "color=", (int)elem.text_color_r, (int)elem.text_color_g, (int)elem.text_color_b);
    }
    
    output_.push_back(elem);
    
    current_inline_text_.clear();
    in_inline_context_ = false;
}

void LayoutEngine::addLineBreak() {
    TextElement elem;
    elem.type = ElementType::LineBreak;
    output_.push_back(elem);
}

TextStyle LayoutEngine::computeTextStyle(const ComputedStyle& style) {
    TextStyle result = TextStyle::Normal;
    
    if (style.bold) result = result | TextStyle::Bold;
    if (style.italic) result = result | TextStyle::Italic;
    if (style.underline) result = result | TextStyle::Underline;
    if (style.strikethrough) result = result | TextStyle::Strikethrough;
    if (style.monospace) result = result | TextStyle::Monospace;
    if (style.font_size_multiplier < 0.9f) result = result | TextStyle::Small;
    
    if (style.vertical_align == ComputedStyle::VerticalAlign::Sub) {
        result = result | TextStyle::Subscript;
    } else if (style.vertical_align == ComputedStyle::VerticalAlign::Super) {
        result = result | TextStyle::Superscript;
    }
    
    return result;
}

TextAlign LayoutEngine::computeTextAlign(const ComputedStyle& style) {
    switch (style.text_align) {
        case ComputedStyle::TextAlign::Left: return TextAlign::Left;
        case ComputedStyle::TextAlign::Right: return TextAlign::Right;
        case ComputedStyle::TextAlign::Center: return TextAlign::Center;
        case ComputedStyle::TextAlign::Justify: return TextAlign::Justify;
    }
    return TextAlign::Left;
}

std::wstring LayoutEngine::utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
#else
    std::wstring result;
    for (char c : str) {
        result += static_cast<wchar_t>(static_cast<unsigned char>(c));
    }
    return result;
#endif
}

std::wstring LayoutEngine::processWhitespace(const std::wstring& text, 
                                            ComputedStyle::WhiteSpace ws) {
    if (ws == ComputedStyle::WhiteSpace::Pre || 
        ws == ComputedStyle::WhiteSpace::PreWrap) {
        return text;
    }
    
    std::wstring result;
    bool prev_was_space = false;
    
    for (wchar_t c : text) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            if (!prev_was_space) {
                result += L' ';
                prev_was_space = true;
            }
        } else {
            result += c;
            prev_was_space = false;
        }
    }
    
    if (!result.empty() && result[0] == L' ') {
        result = result.substr(1);
    }
    if (!result.empty() && result.back() == L' ') {
        result.pop_back();
    }
    
    return result;
}

} // namespace epub