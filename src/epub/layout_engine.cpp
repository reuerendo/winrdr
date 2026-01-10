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
    
    // Reset initial state to defaults
    current_inline_style_ = TextStyle::Normal;
    current_font_family_ = "";
    current_inline_align_ = TextAlign::Left;
    
    for (auto& child : document->children) {
        layoutNode(child.get(), 0);
    }
    
    flushInlineContent();
    
    LOG_INFO("Layout complete, elements:", output_.size());
    return output_;
}

void LayoutEngine::layoutNode(DOMNode* node, int list_level) {
    if (!node) return;

    if (node->type == DOMNode::Type::Element) {
        layoutElement(static_cast<ElementNode*>(node), list_level);
    } else if (node->type == DOMNode::Type::Text) {
        layoutText(static_cast<TextNode*>(node));
    }
}

void LayoutEngine::layoutElement(ElementNode* element, int list_level) {
    // 1. Save current context to restore it after processing children (Handling nesting)
    TextStyle old_inline_style = current_inline_style_;
    std::string old_font_family = current_font_family_;
    TextAlign old_align = current_inline_align_;
    float old_indent = current_text_indent_;
    
    // 2. Determine if this is a block element
    bool is_block = (element->style.display == DisplayType::Block || 
                     element->style.display == DisplayType::ListItem);
    
    if (is_block) {
        flushInlineContent();
        in_inline_context_ = true;
        first_element_in_block_ = true;
        
        // Update block-level properties
        current_block_type_ = element->type;
        current_margin_top_ = element->style.margin_top;
        current_margin_bottom_ = element->style.margin_bottom;
        current_text_indent_ = element->style.text_indent;
    }

    // 3. Update style with inheritance check
    // If element has no specific font-family, keep the parent's one
    if (!element->style.font_family.empty()) {
        std::string cleaned_font = element->style.font_family;
        // Strip CSS quotes: "Arial" -> Arial
        cleaned_font.erase(std::remove(cleaned_font.begin(), cleaned_font.end(), '\"'), cleaned_font.end());
        cleaned_font.erase(std::remove(cleaned_font.begin(), cleaned_font.end(), '\''), cleaned_font.end());
        current_font_family_ = cleaned_font;
    }

    // Merge styles (e.g., if parent is Bold and this is Italic, result is Bold|Italic)
    current_inline_style_ = old_inline_style | computeTextStyle(element->style);
    
    if (element->style.text_align != ComputedStyle::TextAlign::Left) {
        current_inline_align_ = computeTextAlign(element->style);
    }

    // Special handling for specific tags
    if (element->tag_name == "br") {
        addLineBreak();
    } else if (element->tag_name == "hr") {
        flushInlineContent();
        TextElement hr;
        hr.type = ElementType::HorizontalRule;
        output_.push_back(hr);
    } else if (element->tag_name == "img") {
        flushInlineContent();
        auto it = element->attributes.find("src");
        if (it != element->attributes.end()) {
            TextElement img;
            img.type = ElementType::Image;
            img.image_id = it->second;
            img.margin_top = element->style.margin_top;
            img.margin_bottom = element->style.margin_bottom;
            output_.push_back(img);
        }
    } else {
        // Process children with updated context
        for (auto& child : element->children) {
            layoutNode(child.get(), list_level + (element->tag_name == "li" ? 1 : 0));
        }
    }

    // 4. Restore context after element is closed
    if (is_block) {
        flushInlineContent();
        in_inline_context_ = false;
    }
    
    current_inline_style_ = old_inline_style;
    current_font_family_ = old_font_family;
    current_inline_align_ = old_align;
    current_text_indent_ = old_indent;
}

void LayoutEngine::layoutText(TextNode* text) {
    if (!text || text->content.empty()) return;

    std::wstring processed = utf8ToWide(text->content);
    // Note: WhiteSpace processing should ideally happen here or in flush
    current_inline_text_ += processed;
}

void LayoutEngine::flushInlineContent() {
    if (current_inline_text_.empty()) return;

    TextElement element_out;
    element_out.type = current_block_type_;
    element_out.content = current_inline_text_;
    element_out.style = current_inline_style_;
    element_out.align = current_inline_align_;
    element_out.font_family = current_font_family_;
    
    // Assign margins and indents only for the start of the block
    if (first_element_in_block_) {
        element_out.margin_top = current_margin_top_;
        element_out.text_indent = current_text_indent_;
        element_out.is_inline_continuation = false;
        first_element_in_block_ = false;
    } else {
        element_out.is_inline_continuation = true;
    }
    
    // Bottom margin is attached to the last segment of the block
    element_out.margin_bottom = current_margin_bottom_;

    output_.push_back(element_out);
    current_inline_text_.clear();
}

void LayoutEngine::addLineBreak() {
    flushInlineContent();
    TextElement lb;
    lb.type = ElementType::LineBreak;
    output_.push_back(lb);
    first_element_in_block_ = true;
}

TextStyle LayoutEngine::computeTextStyle(const ComputedStyle& style) {
    TextStyle ts = TextStyle::Normal;
    
    if (style.font_weight == ComputedStyle::FontWeight::Bold || style.font_weight_val >= 700) {
        ts = ts | TextStyle::Bold;
    }
    
    if (style.font_style == ComputedStyle::FontStyle::Italic) {
        ts = ts | TextStyle::Italic;
    }
    
    if (style.text_decoration == ComputedStyle::TextDecoration::Underline) {
        ts = ts | TextStyle::Underline;
    } else if (style.text_decoration == ComputedStyle::TextDecoration::LineThrough) {
        ts = ts | TextStyle::Strikethrough;
    }
    
    // Handle small-caps or specifically smaller text
    if (style.font_size_val > 0 && style.font_size_val < 14.0f) {
        ts = ts | TextStyle::Small;
    }

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
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring LayoutEngine::processWhitespace(const std::wstring& text, ComputedStyle::WhiteSpace ws) {
    if (ws == ComputedStyle::WhiteSpace::Pre || ws == ComputedStyle::WhiteSpace::PreWrap) return text;
    
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
    
    // Trim leading and trailing spaces for normal wrap
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
    } else if (transform == ComputedStyle::TextTransform::Capitalize) {
        bool at_word_start = true;
        for (wchar_t& c : result) {
            if (iswspace(c)) {
                at_word_start = true;
            } else if (at_word_start) {
                c = towupper(c);
                at_word_start = false;
            }
        }
    }
    return result;
}

} // namespace epub