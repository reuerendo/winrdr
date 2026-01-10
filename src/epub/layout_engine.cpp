#include "layout_engine.h"
#include "../utils/logger.h"
#include <algorithm>
<<<<<<< Updated upstream

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
=======
#include <sstream>

namespace epub {

LayoutEngine::LayoutEngine() {}

std::vector<RenderLine> LayoutEngine::layout(DocumentNode* document, ImageCache* image_cache,
                                             int viewport_width, int default_font_size) {
    LOG_DEBUG("Starting layout, viewport:", viewport_width);
    
    LayoutContext ctx;
    ctx.viewport_width = viewport_width;
    ctx.default_font_size = default_font_size;
    ctx.image_cache = image_cache;
    ctx.current_y = 0;
    ctx.current_x = 0;
    
    std::vector<RenderLine> output;
>>>>>>> Stashed changes
    
    for (auto& child : document->children) {
        layoutNode(child.get(), ctx, output);
    }
    
    // Flush any remaining inline content
    flushInlineContent(ctx, output);
    
    LOG_INFO("Layout complete, lines:", output.size());
    
    return output;
}

void LayoutEngine::layoutNode(DOMNode* node, LayoutContext& ctx, 
                              std::vector<RenderLine>& output) {
    if (!node) return;
    
<<<<<<< Updated upstream
    // Skip nodes with display:none
    if (node->computed_style.display == DisplayType::None) {
        return;
    }
=======
    NodeType type = node->getType();
>>>>>>> Stashed changes
    
    if (type == NodeType::Element) {
        layoutElement(static_cast<ElementNode*>(node), ctx, output);
    } else if (type == NodeType::Text) {
        layoutText(static_cast<TextNode*>(node), ctx, output);
    }
}

<<<<<<< Updated upstream
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
=======
void LayoutEngine::layoutElement(ElementNode* element, LayoutContext& ctx,
                                 std::vector<RenderLine>& output) {
    const ComputedStyle& style = element->computed_style;
    
    // Skip display:none
    if (style.display == DisplayType::None) {
        return;
    }
    
    // Block-level element
    if (style.display == DisplayType::Block || 
        style.display == DisplayType::ListItem) {
        
        // Flush inline content before block
        flushInlineContent(ctx, output);
        
        // Add top margin
        addBlockMargins(style, ctx, output, true);
        addBlockPadding(style, ctx, output, true);
        
        // Process children
        for (auto& child : element->children) {
            layoutNode(child.get(), ctx, output);
        }
        
        // Flush inline content after block
        flushInlineContent(ctx, output);
        
        // Add bottom padding and margin
        addBlockPadding(style, ctx, output, false);
        addBlockMargins(style, ctx, output, false);
    }
    // Inline element
    else if (style.display == DisplayType::Inline) {
        // Save previous style
        ComputedStyle saved_style = ctx.current_text_style;
        
        // Merge with current style
        ctx.current_text_style.bold = ctx.current_text_style.bold || style.bold;
        ctx.current_text_style.italic = ctx.current_text_style.italic || style.italic;
        ctx.current_text_style.underline = ctx.current_text_style.underline || style.underline;
        ctx.current_text_style.strikethrough = ctx.current_text_style.strikethrough || style.strikethrough;
        ctx.current_text_style.monospace = ctx.current_text_style.monospace || style.monospace;
        
        if (style.font_size_multiplier != 1.0f) {
            ctx.current_text_style.font_size_multiplier *= style.font_size_multiplier;
        }
        
        if (style.vertical_align != ComputedStyle::VerticalAlign::Baseline) {
            ctx.current_text_style.vertical_align = style.vertical_align;
        }
        
        // Process children
        for (auto& child : element->children) {
            layoutNode(child.get(), ctx, output);
        }
        
        // Restore style
        ctx.current_text_style = saved_style;
>>>>>>> Stashed changes
    }
    // Image
    else if (element->getTagName() == "img") {
        flushInlineContent(ctx, output);
        
        std::string src = element->getAttribute("src");
<<<<<<< Updated upstream
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
    
    if (!wide_text.empty()) {
        // If style changed, flush previous text
        if (!current_inline_text_.empty()) {
            // Check if we need to start new element due to style change
            // For now, just append - proper implementation would track style changes
        }
        
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
=======
        if (!src.empty() && ctx.image_cache) {
            const ImageData* img = ctx.image_cache->getImage(src);
            if (img) {
                RenderLine line;
                line.type = RenderLine::Type::Image;
                line.start_y = ctx.current_y;
                line.image_id = src;
                line.image_width = img->width;
                line.image_height = img->height;
                
                // Scale to fit viewport
                if (line.image_width > ctx.viewport_width) {
                    float scale = (float)ctx.viewport_width / line.image_width;
                    line.image_width = ctx.viewport_width;
                    line.image_height = (int)(line.image_height * scale);
                }
                
                line.height = line.image_height;
                line.split_before = RenderLine::SPLIT_AUTO;
                line.split_after = RenderLine::SPLIT_AUTO;
                
                output.push_back(line);
                ctx.current_y += line.height;
>>>>>>> Stashed changes
            }
        } else {
            result += c;
            prev_was_space = false;
        }
    }
<<<<<<< Updated upstream
    
    // Trim leading/trailing space
    if (!result.empty() && result[0] == L' ') {
        result = result.substr(1);
    }
    if (!result.empty() && result.back() == L' ') {
        result.pop_back();
    }
    
    return result;
=======
}

void LayoutEngine::layoutText(TextNode* text, LayoutContext& ctx,
                              std::vector<RenderLine>& output) {
    std::string utf8_text = text->getText();
    
    // Convert UTF-8 to wstring
    std::wstring wtext;
    if (!utf8_text.empty()) {
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, nullptr, 0);
        if (size > 0) {
            wtext.resize(size - 1);
            MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, &wtext[0], size);
        }
    }
    
    if (!wtext.empty()) {
        processInlineText(wtext, text->computed_style, ctx, output);
    }
}

void LayoutEngine::processInlineText(const std::wstring& text, const ComputedStyle& style,
                                     LayoutContext& ctx, std::vector<RenderLine>& output) {
    // Merge with current text style
    ComputedStyle merged = ctx.current_text_style;
    merged.text_align = style.text_align;
    
    // Word wrapping
    std::wstring current_word;
    int font_height = calculateFontHeight(merged, ctx.default_font_size);
    
    for (size_t i = 0; i < text.length(); i++) {
        wchar_t ch = text[i];
        
        // Whitespace
        if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\r') {
            if (!current_word.empty()) {
                wrapTextLine(current_word, merged, ctx, output);
                current_word.clear();
            }
            
            // Space character
            if (ch == L' ') {
                int space_width = calculateTextWidth(L" ", merged, ctx.default_font_size);
                if (ctx.current_x + space_width <= ctx.viewport_width) {
                    ctx.current_x += space_width;
                }
            }
            // Line break
            else if (ch == L'\n') {
                flushInlineContent(ctx, output);
            }
        }
        // Regular character
        else {
            current_word += ch;
        }
    }
    
    // Flush remaining word
    if (!current_word.empty()) {
        wrapTextLine(current_word, merged, ctx, output);
    }
}

void LayoutEngine::wrapTextLine(const std::wstring& text, const ComputedStyle& style,
                                LayoutContext& ctx, std::vector<RenderLine>& output) {
    int word_width = calculateTextWidth(text, style, ctx.default_font_size);
    int font_height = calculateFontHeight(style, ctx.default_font_size);
    
    // Word fits on current line
    if (ctx.current_x + word_width <= ctx.viewport_width) {
        RenderLine line;
        line.type = RenderLine::Type::Text;
        line.text = text;
        line.style = style;
        line.start_y = ctx.current_y;
        line.height = 0; // Will be set when flushing
        
        ctx.pending_inline_content.push_back(line);
        ctx.current_x += word_width;
        ctx.line_height = std::max(ctx.line_height, font_height);
    }
    // Word doesn't fit - wrap to new line
    else {
        flushInlineContent(ctx, output);
        
        // Add word to new line
        RenderLine line;
        line.type = RenderLine::Type::Text;
        line.text = text;
        line.style = style;
        line.start_y = ctx.current_y;
        line.height = 0;
        
        ctx.pending_inline_content.push_back(line);
        ctx.current_x = word_width;
        ctx.line_height = font_height;
    }
}

void LayoutEngine::flushInlineContent(LayoutContext& ctx, std::vector<RenderLine>& output) {
    if (ctx.pending_inline_content.empty()) {
        return;
    }
    
    // Set height for all pending content
    for (auto& line : ctx.pending_inline_content) {
        line.height = ctx.line_height;
    }
    
    // Add to output
    output.insert(output.end(), ctx.pending_inline_content.begin(), 
                 ctx.pending_inline_content.end());
    
    ctx.current_y += ctx.line_height;
    ctx.current_x = 0;
    ctx.line_height = 0;
    ctx.pending_inline_content.clear();
}

void LayoutEngine::addBlockMargins(const ComputedStyle& style, LayoutContext& ctx,
                                   std::vector<RenderLine>& output, bool top) {
    float margin = top ? style.margin_top : style.margin_bottom;
    
    if (margin > 0) {
        int margin_px = (int)(margin * ctx.default_font_size);
        
        RenderLine line;
        line.type = RenderLine::Type::BlockMargin;
        line.start_y = ctx.current_y;
        line.height = margin_px;
        line.split_before = top ? determineSplitBefore(style) : RenderLine::SPLIT_AUTO;
        line.split_after = top ? RenderLine::SPLIT_AUTO : determineSplitAfter(style);
        
        output.push_back(line);
        ctx.current_y += margin_px;
    }
}

void LayoutEngine::addBlockPadding(const ComputedStyle& style, LayoutContext& ctx,
                                   std::vector<RenderLine>& output, bool top) {
    float padding = top ? style.padding_top : style.padding_bottom;
    
    if (padding > 0) {
        int padding_px = (int)(padding * ctx.default_font_size);
        
        RenderLine line;
        line.type = RenderLine::Type::BlockPadding;
        line.start_y = ctx.current_y;
        line.height = padding_px;
        line.split_before = RenderLine::SPLIT_AVOID;
        line.split_after = RenderLine::SPLIT_AVOID;
        
        output.push_back(line);
        ctx.current_y += padding_px;
    }
}

int LayoutEngine::calculateTextWidth(const std::wstring& text, const ComputedStyle& style,
                                     int font_size) {
    // Approximate: average character width based on font size
    float char_width = font_size * 0.6f;
    
    if (style.bold) {
        char_width *= 1.1f;
    }
    if (style.monospace) {
        char_width = font_size * 0.6f; // Fixed width
    }
    
    return (int)(text.length() * char_width);
}

int LayoutEngine::calculateFontHeight(const ComputedStyle& style, int base_font_size) {
    float size = base_font_size * style.font_size_multiplier;
    
    // Line height
    size *= style.line_height;
    
    return (int)size;
}

RenderLine::SplitFlag LayoutEngine::determineSplitBefore(const ComputedStyle& style) {
    // Based on CSS break-before/page-break-before
    // For now, simple logic
    return RenderLine::SPLIT_AUTO;
}

RenderLine::SplitFlag LayoutEngine::determineSplitAfter(const ComputedStyle& style) {
    return RenderLine::SPLIT_AUTO;
}

// Page splitting algorithm (simplified from CREngine)
std::vector<PageInfo> LayoutEngine::splitIntoPages(const std::vector<RenderLine>& lines,
                                                   int page_height) {
    LOG_DEBUG("Splitting into pages, height:", page_height);
    
    std::vector<PageInfo> pages;
    
    if (lines.empty() || page_height <= 0) {
        return pages;
    }
    
    size_t line_index = 0;
    
    while (line_index < lines.size()) {
        PageInfo page;
        page.start_y = lines[line_index].start_y;
        page.height = 0;
        
        size_t page_start_line = line_index;
        size_t last_auto_split = line_index;
        
        // Add lines to page
        while (line_index < lines.size()) {
            const RenderLine& line = lines[line_index];
            
            int line_end = line.start_y + line.height;
            int page_end = page.start_y + page_height;
            
            // Line fits completely
            if (line_end <= page_end) {
                page.height = line_end - page.start_y;
                
                // Track last position where we can split
                if (line.split_after == RenderLine::SPLIT_AUTO ||
                    line.split_after == RenderLine::SPLIT_ALWAYS) {
                    last_auto_split = line_index;
                }
                
                line_index++;
                
                // Force page break
                if (line.split_after == RenderLine::SPLIT_ALWAYS) {
                    break;
                }
            }
            // Line doesn't fit
            else {
                // Check if we should avoid splitting
                if (line.split_before == RenderLine::SPLIT_AVOID &&
                    last_auto_split > page_start_line) {
                    // Split at last auto position
                    line_index = last_auto_split + 1;
                    break;
                }
                
                // If line itself is too tall, split it
                if (line.height > page_height) {
                    // Keep part of the line
                    page.height = page_height;
                    // Continue with same line on next page
                    break;
                }
                
                // Otherwise, move line to next page
                break;
            }
        }
        
        // Add lines to page
        for (size_t i = page_start_line; i < line_index && i < lines.size(); i++) {
            page.lines.push_back(&lines[i]);
        }
        
        if (!page.lines.empty()) {
            pages.push_back(page);
        }
    }
    
    LOG_INFO("Split into pages:", pages.size());
    
    return pages;
>>>>>>> Stashed changes
}

} // namespace epub