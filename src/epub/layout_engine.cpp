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
    
    // Iterate through top-level document children
    for (auto& child : document->children) {
        layoutNode(child.get(), 0);
    }
    
    flushInlineContent();
    
    LOG_INFO("Layout complete, elements:", output_.size());
    return output_;
}

void LayoutEngine::layoutNode(DOMNode* node, int list_level) {
    // Check node type using your project's NodeType enum and getType() method
    if (node->getType() == NodeType::Element) {
        layoutElement(static_cast<ElementNode*>(node), list_level);
    } else if (node->getType() == NodeType::Text) {
        layoutText(static_cast<TextNode*>(node));
    }
}

void LayoutEngine::layoutElement(ElementNode* element, int list_level) {
    std::string tag = element->getTagName();
    const ComputedStyle& style = element->getStyle();
    
    // Determine if this is a block-level element
    bool is_block = (tag == "p" || tag == "div" || tag == "h1" || tag == "h2" || 
                     tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" || 
                     tag == "li" || tag == "blockquote" || tag == "hr" || tag == "img");

    if (is_block) {
        flushInlineContent();
        
        // Update block context from CSS styles
        current_block_type_ = ElementType::Paragraph;
        if (tag[0] == 'h' && tag.length() == 2) {
            int level = tag[1] - '0';
            if (level >= 1 && level <= 6) {
                current_block_type_ = static_cast<ElementType>(static_cast<int>(ElementType::Heading1) + level - 1);
            }
        } else if (tag == "li") {
            current_block_type_ = ElementType::ListItem;
        } else if (tag == "blockquote") {
            current_block_type_ = ElementType::Quote;
        } else if (tag == "hr") {
            current_block_type_ = ElementType::HorizontalRule;
        } else if (tag == "img") {
            current_block_type_ = ElementType::Image;
        }

        current_inline_align_ = computeTextAlign(style);
        current_list_level_ = list_level;
        current_text_indent_ = style.text_indent;
        current_margin_top_ = style.margin_top;
        current_margin_bottom_ = style.margin_bottom;
        current_font_family_ = style.font_family;
        first_element_in_block_ = true;

        // Special handling for images - they are blocks that carry their own data
        if (tag == "img") {
            const auto& attrs = element->getAttributes();
            auto it = attrs.find("src");
            if (it != attrs.end()) {
                TextElement img_elem;
                img_elem.type = ElementType::Image;
                img_elem.image_id = it->second;
                img_elem.align = current_inline_align_;
                img_elem.margin_top = current_margin_top_;
                img_elem.margin_bottom = current_margin_bottom_;
                output_.push_back(img_elem);
            }
            return; // Don't process children for images
        }
        
        if (tag == "hr") {
            TextElement hr_elem;
            hr_elem.type = ElementType::HorizontalRule;
            hr_elem.margin_top = current_margin_top_;
            hr_elem.margin_bottom = current_margin_bottom_;
            output_.push_back(hr_elem);
            return;
        }
    }

    // Process children recursively
    for (auto& child : element->children) {
        // Pass updated list level for list items
        layoutNode(child.get(), list_level + (tag == "li" ? 1 : 0));
    }

    if (is_block) {
        flushInlineContent();
    }
}

void LayoutEngine::layoutText(TextNode* text) {
    // Access text content via getText() as seen in project structure
    std::wstring content = text->getText();
    if (content.empty()) return;

    // Use current element's style if available, otherwise default
    // In your DOM, TextNode might need to get style from parent, but here we use context
    // because LayoutEngine tracks the active block style during recursion
    
    if (!in_inline_context_) {
        in_inline_context_ = true;
    }

    current_inline_text_ += content;
}

void LayoutEngine::flushInlineContent() {
    if (current_inline_text_.empty() && !in_inline_context_) return;

    // Clean up whitespace for the accumulated text
    std::wstring processed = processWhitespace(current_inline_text_, ComputedStyle::WhiteSpace::Normal);
    
    if (!processed.empty()) {
        TextElement elem;
        elem.type = current_block_type_;
        elem.content = processed;
        elem.style = current_inline_style_;
        elem.align = current_inline_align_;
        elem.list_level = current_list_level_;
        elem.text_indent = current_text_indent_;
        
        // Pass explicit style properties for rendering
        elem.margin_top = current_margin_top_;
        elem.margin_bottom = current_margin_bottom_;
        elem.font_family = current_font_family_;

        output_.push_back(elem);
    }

    current_inline_text_.clear();
    in_inline_context_ = false;
    first_element_in_block_ = false;
}

TextStyle LayoutEngine::computeTextStyle(const ComputedStyle& style) {
    TextStyle ts = TextStyle::Normal;
    // Map ComputedStyle boolean flags to TextStyle bitmask
    if (style.bold) ts = ts | TextStyle::Bold;
    if (style.italic) ts = ts | TextStyle::Italic;
    if (style.underline) ts = ts | TextStyle::Underline;
    if (style.strikethrough) ts = ts | TextStyle::Strikethrough;
    
    // Inferred: some projects use font_size to determine Small style
    if (style.font_size < 14.0f && style.font_size > 0) ts = ts | TextStyle::Small;
    
    return ts;
}

TextAlign LayoutEngine::computeTextAlign(const ComputedStyle& style) {
    // Map ComputedStyle::TextAlign enum to FormattedText's TextAlign
    switch (style.text_align) {
        case ComputedStyle::TextAlign::Center:  return TextAlign::Center;
        case ComputedStyle::TextAlign::Right:   return TextAlign::Right;
        case ComputedStyle::TextAlign::Justify: return TextAlign::Justify;
        case ComputedStyle::TextAlign::Left:
        default:                                return TextAlign::Left;
    }
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
    
    // Trim leading and trailing spaces for the whole block
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