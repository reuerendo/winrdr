#pragma once

#include "dom_node.h"
#include "formatted_text.h"
#include "image_cache.h"
#include <string>

namespace epub {

class LayoutEngine {
public:
    LayoutEngine();
    
    FormattedContent layout(DocumentNode* document, ImageCache* image_cache);

private:
    void layoutNode(DOMNode* node, int list_level);
    void layoutElement(ElementNode* element, int list_level);
    void layoutText(TextNode* text);
    
    void flushInlineContent();
    void addLineBreak();
    
    TextStyle computeTextStyle(const ComputedStyle& style);
    TextAlign computeTextAlign(const ComputedStyle& style);
    
    std::wstring utf8ToWide(const std::string& str);
    std::wstring processWhitespace(const std::wstring& text, ComputedStyle::WhiteSpace ws);
    
    FormattedContent output_;
    ImageCache* image_cache_;
    
    // Current inline formatting context
    std::wstring current_inline_text_;
    TextStyle current_inline_style_;
    TextAlign current_inline_align_;
    ElementType current_block_type_;
    int current_list_level_;
    
    // Track if we're in inline context
    bool in_inline_context_;
};

} // namespace epub