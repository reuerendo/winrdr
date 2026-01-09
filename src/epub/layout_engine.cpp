#include "layout_engine.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cwctype>

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
    , in_inline_context_(false)
{}

FormattedContent LayoutEngine::layout(DocumentNode* document, ImageCache* image_cache) {
    output_.clear();
    image_cache_ = image_cache;
    current_inline_text_.clear();
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
    
    // Skip nodes with display:none
    if (node->computed_style.display == DisplayType::None) {
        return;
    }
    
    // Apply margin-top spacing before block elements (converted to line breaks)
    if (node->getType() == NodeType::Element && 
        (node->computed_style.display == DisplayType::Block ||
         node->computed_style.display == DisplayType::ListItem)) {
        
        float margin_top = node->computed_style.margin_top;
        // Convert pixels to approximate line breaks (assuming ~20px per line)
        int line_breaks = static_cast<int>(margin_top / 20.0f);
        for (int i = 0; i < line_breaks && i < 3; i++) { // Cap at 3 line breaks
            addLineBreak();
        }
    }
    
    if (node->getType() == NodeType::Element) {
        layoutElement(static_cast<ElementNode*>(node), list_level);
    } else if (node->getType() == NodeType::Text) {
        layoutText(static_cast<TextNode*>(node));
    }
    
    // Apply margin-bottom spacing after block elements
    if (node->getType() == NodeType::Element && 
        (node->computed_style.display == DisplayType::Block ||
         node->computed_style.display == DisplayType::ListItem)) {
        
        float margin_bottom = node->computed_style.margin_bottom;
        int line_breaks = static_cast<int>(margin_bottom / 20.0f);
        for (int i = 0; i < line_breaks && i < 3; i++) {
            addLineBreak();
        }
    }
}

void LayoutEngine::layoutElement(ElementNode* element, int list_level) {
    const std::string& tag = element->getTagName();
    const ComputedStyle& style = element->computed_style;
    
    // Handle special elements
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
                output_.push_back(elem);
                addLineBreak();
            }
        }
        return;
    }
    
    // Block vs inline handling
    if (style.display == DisplayType::Block || 
        style.display == DisplayType::ListItem) {
        
        // Flush any pending inline content
        flushInlineContent();
        
        // Set block context
        ElementType old_block_type = current_block_type_;
        TextAlign old_align = current_inline_align_;
        
        // Determine block type from tag
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
        
        // Adjust list level for lists
        int new_list_level = list_level;
        if (tag == "ul" || tag == "ol") {
            new_list_level++;
        }
        
        // Layout children
        for (auto& child : element->children) {
            layoutNode(child.get(), new_list_level);
        }
        
        // Flush block content
        flushInlineContent();
        
        // Add spacing after block elements (except if next sibling is also block)
        if (current_block_type_ != ElementType::Text && !output_.empty()) {
            if (output_.back().type != ElementType::LineBreak) {
                addLineBreak();
            }
        }
        
        // Restore context
        current_block_type_ = old_block_type;
        current_inline_align_ = old_align;
    }
    else if (style.display == DisplayType::Inline || 
             style.display == DisplayType::InlineBlock) {
        
        // Enter inline context if not already
        bool was_inline = in_inline_context_;
        in_inline_context_ = true;
        
        // Save current style
        TextStyle old_style = current_inline_style_;
        
        // Apply inline styles additively
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
        
        // Special handling for <q> tag
        if (tag == "q") {
            current_inline_text_ += L"\"";
        }
        
        // Layout children
        for (auto& child : element->children) {
            layoutNode(child.get(), list_level);
        }
        
        // Closing quote for <q>
        if (tag == "q") {
            current_inline_text_ += L"\"";
        }
        
        // Restore style
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
    
    // Process whitespace according to parent's white-space style
    ComputedStyle::WhiteSpace ws = ComputedStyle::WhiteSpace::Normal;
    if (text->parent) {
        ws = text->parent->computed_style.white_space;
    }
    
    wide_text = processWhitespace(wide_text, ws);
    
    // DON'T apply text-transform here - it should only apply to specific elements
    // text-transform is NOT inherited in our implementation
    // It will be applied only if explicitly set on the parent element
    
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
    
    // Normal and nowrap: collapse whitespace
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
    
    // Trim leading/trailing space
    if (!result.empty() && result[0] == L' ') {
        result = result.substr(1);
    }
    if (!result.empty() && result.back() == L' ') {
        result.pop_back();
    }
    
    return result;
}

std::wstring LayoutEngine::applyTextTransform(const std::wstring& text,
                                              ComputedStyle::TextTransform transform) {
    if (transform == ComputedStyle::TextTransform::None) {
        return text;
    }
    
    std::wstring result = text;
    
    if (transform == ComputedStyle::TextTransform::Uppercase) {
        for (wchar_t& c : result) {
            c = towupper(c);
        }
    } else if (transform == ComputedStyle::TextTransform::Lowercase) {
        for (wchar_t& c : result) {
            c = towlower(c);
        }
    } else if (transform == ComputedStyle::TextTransform::Capitalize) {
        bool at_word_start = true;
        for (wchar_t& c : result) {
            if (iswalnum(c)) {
                if (at_word_start) {
                    c = towupper(c);
                    at_word_start = false;
                }
            } else {
                at_word_start = true;
            }
        }
    }
    
    return result;
}

} // namespace epub