#include "layout_engine.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cwctype>
#include <cmath>

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
    , current_text_indent_(0.0f)
    , in_inline_context_(false)
    , current_margin_top_(0.0f)
    , current_margin_bottom_(0.0f)
    , current_font_family_("")
    , first_element_in_block_(true)
{}

FormattedContent LayoutEngine::layout(DocumentNode* document, ImageCache* image_cache) {
    output_.clear();
    image_cache_ = image_cache;
    current_inline_text_.clear();
    in_inline_context_ = false;
    current_list_level_ = 0;
    first_element_in_block_ = true;
    
    for (auto& child : document->children) {
        layoutNode(child.get(), 0);
    }
    
    flushInlineContent();
    
    LOG_INFO("Layout complete, elements:", output_.size());
    return output_;
}

void LayoutEngine::layoutNode(DOMNode* node, int list_level) {
    if (!node) return;
    
    // Check if element is hidden (display: none)
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
    const std::string tag = element->getTagName();
    const ComputedStyle& style = element->computed_style;
    
    if (tag == "br") {
        if (in_inline_context_) {
            flushInlineContent();
        }
        addLineBreak();
        first_element_in_block_ = true; 
        return;
    }
    
    if (tag == "hr") {
        flushInlineContent();
        TextElement elem;
        elem.type = ElementType::HorizontalRule;
        elem.margin_top = style.margin_top;
        elem.margin_bottom = style.margin_bottom;
        output_.push_back(elem);
        first_element_in_block_ = true;
        return;
    }
    
    if (tag == "img") {
        flushInlineContent();
        
        std::string src = element->getAttribute("src");
        if (!src.empty()) {
            TextElement elem;
            elem.type = ElementType::Image;
            elem.image_id = src;
            elem.margin_top = style.margin_top;
            elem.margin_bottom = style.margin_bottom;
            output_.push_back(elem);
            first_element_in_block_ = true;
        }
        return;
    }
    
    bool is_block = (style.display == DisplayType::Block || 
                     style.display == DisplayType::ListItem ||
                     style.display == DisplayType::Table ||
                     style.display == DisplayType::TableRow ||
                     style.display == DisplayType::TableCell);

    if (is_block) {
        flushInlineContent();
        
        // Save current block context to restore after recursion
        ElementType old_block_type = current_block_type_;
        TextAlign old_align = current_inline_align_;
        float old_text_indent = current_text_indent_;
        TextStyle old_style = current_inline_style_;
        float old_margin_top = current_margin_top_;
        float old_margin_bottom = current_margin_bottom_;
        std::string old_font_family = current_font_family_;
        bool was_inline = in_inline_context_;
        
        // Update context based on tag and style
        if (tag == "p") current_block_type_ = ElementType::Paragraph;
        else if (tag == "h1") current_block_type_ = ElementType::Heading1;
        else if (tag == "h2") current_block_type_ = ElementType::Heading2;
        else if (tag == "h3") current_block_type_ = ElementType::Heading3;
        else if (tag == "h4") current_block_type_ = ElementType::Heading4;
        else if (tag == "h5") current_block_type_ = ElementType::Heading5;
        else if (tag == "h6") current_block_type_ = ElementType::Heading6;
        else if (tag == "blockquote") current_block_type_ = ElementType::Quote;
        else if (tag == "li") current_block_type_ = ElementType::ListItem;
        else if (tag == "pre") current_block_type_ = ElementType::CodeBlock;
        else current_block_type_ = ElementType::Paragraph;
        
        current_inline_align_ = computeTextAlign(style);
        current_text_indent_ = style.text_indent;
        current_margin_top_ = style.margin_top;
        current_margin_bottom_ = style.margin_bottom;
        current_font_family_ = style.font_family;
        current_inline_style_ = computeTextStyle(style);
        
        int new_list_level = list_level;
        if (tag == "ul" || tag == "ol") {
            new_list_level++;
        }
        
        first_element_in_block_ = true;
        in_inline_context_ = false;
        
        for (auto& child : element->children) {
            layoutNode(child.get(), new_list_level);
        }
        
        flushInlineContent();
        
        // Restore previous context
        current_block_type_ = old_block_type;
        current_inline_align_ = old_align;
        current_text_indent_ = old_text_indent;
        current_inline_style_ = old_style;
        current_margin_top_ = old_margin_top;
        current_margin_bottom_ = old_margin_bottom;
        current_font_family_ = old_font_family;
        in_inline_context_ = was_inline;
        
    } else {
        // Inline element processing
        TextStyle old_style = current_inline_style_;
        TextStyle new_style = computeTextStyle(style);
        
        // Merge inline styles (e.g., bold + italic)
        current_inline_style_ = current_inline_style_ | new_style;
        
        bool was_inline = in_inline_context_;
        in_inline_context_ = true;
        
        for (auto& child : element->children) {
            layoutNode(child.get(), list_level);
        }
        
        current_inline_style_ = old_style;
        in_inline_context_ = was_inline;
    }
}

void LayoutEngine::layoutText(TextNode* text) {
    if (!in_inline_context_) {
        in_inline_context_ = true;
    }
    
    std::wstring wide_text = utf8ToWide(text->getText());
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
    elem.text_indent = current_text_indent_;
    
    // Apply block-level styles
    elem.margin_top = current_margin_top_;
    elem.margin_bottom = current_margin_bottom_;
    elem.font_family = current_font_family_;
    
    output_.push_back(elem);
    
    current_inline_text_.clear();
    first_element_in_block_ = false;
}

void LayoutEngine::addLineBreak() {
    TextElement elem;
    elem.type = ElementType::LineBreak;
    output_.push_back(elem);
}

TextStyle LayoutEngine::computeTextStyle(const ComputedStyle& style) {
    TextStyle ts = TextStyle::Normal;
    if (style.bold) ts = ts | TextStyle::Bold;
    if (style.italic) ts = ts | TextStyle::Italic;
    if (style.underline) ts = ts | TextStyle::Underline;
    if (style.strikethrough) ts = ts | TextStyle::Strikethrough;
    if (style.monospace) ts = ts | TextStyle::Monospace;
    
    // Scale-based styles
    if (style.font_size_multiplier < 0.9f) ts = ts | TextStyle::Small;
    
    if (style.vertical_align == ComputedStyle::VerticalAlign::Sub) ts = ts | TextStyle::Subscript;
    else if (style.vertical_align == ComputedStyle::VerticalAlign::Super) ts = ts | TextStyle::Superscript;
    
    return ts;
}

TextAlign LayoutEngine::computeTextAlign(const ComputedStyle& style) {
    switch (style.text_align) {
        case ComputedStyle::TextAlign::Center: return TextAlign::Center;
        case ComputedStyle::TextAlign::Right:  return TextAlign::Right;
        case ComputedStyle::TextAlign::Justify: return TextAlign::Justify;
        default: return TextAlign::Left;
    }
}

std::wstring LayoutEngine::utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring LayoutEngine::processWhitespace(const std::wstring& text, ComputedStyle::WhiteSpace ws) {
    if (ws == ComputedStyle::WhiteSpace::Pre || ws == ComputedStyle::WhiteSpace::PreWrap) return text;
    
    std::wstring result;
    bool prev_was_space = false;
    for (wchar_t c : text) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            if (!prev_was_space) { result += L' '; prev_was_space = true; }
        } else {
            result += c; prev_was_space = false;
        }
    }
    
    if (!result.empty() && result[0] == L' ') result = result.substr(1);
    if (!result.empty() && result.back() == L' ') result.pop_back();
    
    return result;
}

std::wstring LayoutEngine::applyTextTransform(const std::wstring& text, ComputedStyle::TextTransform transform) {
    if (transform == ComputedStyle::TextTransform::None) return text;
    std::wstring result = text;
    if (transform == ComputedStyle::TextTransform::Uppercase) {
        for (wchar_t& c : result) c = towupper(c);
    } else if (transform == ComputedStyle::TextTransform::Lowercase) {
        for (wchar_t& c : result) c = towlower(c);
    }
    return result;
}

} // namespace epub