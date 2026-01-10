#pragma once

#include "dom_node.h"
#include "image_cache.h"
#include <vector>
#include <memory>

namespace epub {

// Rendering line - slice of content with fixed Y position
struct RenderLine {
    int start_y;
    int height;
    
    enum class Type {
        Text,
        Image,
        BlockMargin,
        BlockPadding
    };
    Type type;
    
    // For text lines
    std::wstring text;
    ComputedStyle style;
    
    // For image lines
    std::string image_id;
    int image_width;
    int image_height;
    
    // Split flags (from CREngine approach)
    enum SplitFlag {
        SPLIT_AUTO = 0,
        SPLIT_AVOID = 1,
        SPLIT_ALWAYS = 2
    };
    
    SplitFlag split_before;
    SplitFlag split_after;
    
    RenderLine() 
        : start_y(0), height(0), type(Type::Text)
        , image_width(0), image_height(0)
        , split_before(SPLIT_AUTO), split_after(SPLIT_AUTO) 
    {}
};

// Rendered page information
struct PageInfo {
    int start_y;
    int height;
    std::vector<const RenderLine*> lines;
};

class LayoutEngine {
public:
    LayoutEngine();
    
    // Layout document tree into render lines
    std::vector<RenderLine> layout(DocumentNode* document, ImageCache* image_cache,
                                   int viewport_width, int default_font_size);
    
    // Split lines into pages
    std::vector<PageInfo> splitIntoPages(const std::vector<RenderLine>& lines,
                                        int page_height);

private:
    struct LayoutContext {
        int viewport_width;
        int current_y;
        int current_x;
        int line_start_x;
        int line_height;
        int default_font_size;
        ImageCache* image_cache;
        
        std::vector<RenderLine> pending_inline_content;
        ComputedStyle current_text_style;
        
        LayoutContext() 
            : viewport_width(0), current_y(0), current_x(0)
            , line_start_x(0), line_height(0), default_font_size(16)
            , image_cache(nullptr) 
        {}
    };
    
    void layoutNode(DOMNode* node, LayoutContext& ctx, 
                   std::vector<RenderLine>& output);
    void layoutElement(ElementNode* element, LayoutContext& ctx,
                      std::vector<RenderLine>& output);
    void layoutText(TextNode* text, LayoutContext& ctx,
                   std::vector<RenderLine>& output);
    
    void flushInlineContent(LayoutContext& ctx, std::vector<RenderLine>& output);
    void addBlockMargins(const ComputedStyle& style, LayoutContext& ctx,
                        std::vector<RenderLine>& output, bool top);
    void addBlockPadding(const ComputedStyle& style, LayoutContext& ctx,
                        std::vector<RenderLine>& output, bool top);
    
<<<<<<< Updated upstream
    std::wstring utf8ToWide(const std::string& str);
    std::wstring processWhitespace(const std::wstring& text, ComputedStyle::WhiteSpace ws);
=======
    void processInlineText(const std::wstring& text, const ComputedStyle& style,
                          LayoutContext& ctx, std::vector<RenderLine>& output);
    void wrapTextLine(const std::wstring& text, const ComputedStyle& style,
                     LayoutContext& ctx, std::vector<RenderLine>& output);
>>>>>>> Stashed changes
    
    int calculateTextWidth(const std::wstring& text, const ComputedStyle& style,
                          int font_size);
    int calculateFontHeight(const ComputedStyle& style, int base_font_size);
    
<<<<<<< Updated upstream
    // Current inline formatting context
    std::wstring current_inline_text_;
    TextStyle current_inline_style_;
    TextAlign current_inline_align_;
    ElementType current_block_type_;
    int current_list_level_;
    
    // Track if we're in inline context
    bool in_inline_context_;
=======
    RenderLine::SplitFlag determineSplitBefore(const ComputedStyle& style);
    RenderLine::SplitFlag determineSplitAfter(const ComputedStyle& style);
>>>>>>> Stashed changes
};

} // namespace epub