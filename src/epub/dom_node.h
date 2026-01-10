#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace epub {

enum class NodeType {
    Element,
    Text,
    Document
};

enum class DisplayType {
    None,
    Block,
    Inline,
    InlineBlock,
    ListItem,
    Table,        // Treated as Block in layout
    TableRow,     // Treated as Block in layout
    TableCell     // Treated as Block in layout
};

struct ComputedStyle {
    DisplayType display = DisplayType::Inline;
    
    // Text styling
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool monospace = false;
    float font_size_multiplier = 1.0f;
    
    enum class VerticalAlign { Baseline, Sub, Super } vertical_align = VerticalAlign::Baseline;
    
    enum class TextAlign { Left, Right, Center, Justify } text_align = TextAlign::Left;
    
    // Box model (in pixels or em units)
    float margin_top = 0;
    float margin_bottom = 0;
    float margin_left = 0;
    float margin_right = 0;
    
    float padding_top = 0;
    float padding_bottom = 0;
    float padding_left = 0;
    float padding_right = 0;
    
    struct Color {
        unsigned char r = 0, g = 0, b = 0;
        Color() = default;
        Color(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
    };
    Color text_color = Color(0, 0, 0);
    Color background_color = Color(255, 255, 255);
    bool has_background = false;
    
    enum class ListStyleType { None, Disc, Circle, Square, Decimal } list_style = ListStyleType::Disc;
    
    enum class WhiteSpace { Normal, Pre, Nowrap, PreWrap } white_space = WhiteSpace::Normal;
    
    float line_height = 1.2f;
    
    float letter_spacing = 0.0f;
    
    enum class TextTransform { None, Uppercase, Lowercase, Capitalize } text_transform = TextTransform::None;
    
    enum class FontVariantCaps { 
        Normal, SmallCaps, AllSmallCaps 
    } font_variant_caps = FontVariantCaps::Normal;
    
    enum class Hyphens { None, Manual, Auto } hyphens = Hyphens::Manual;
    
    enum class PageBreak { Auto, Always, Avoid } page_break_before = PageBreak::Auto;
    enum class PageBreak page_break_after = PageBreak::Auto;
    enum class PageBreak page_break_inside = PageBreak::Auto;
    
    std::string font_family;
    
    enum class TextRendering { Auto, OptimizeSpeed, OptimizeLegibility } 
        text_rendering = TextRendering::Auto;
    
    float text_indent = 0.0f;
    
    enum class TextAlignLast { Auto, Left, Right, Center, Justify } 
        text_align_last = TextAlignLast::Auto;
};

class DOMNode {
public:
    virtual ~DOMNode() = default;
    virtual NodeType getType() const = 0;
    
    DOMNode* parent = nullptr;
    std::vector<std::unique_ptr<DOMNode>> children;
    
    ComputedStyle computed_style;
};

class ElementNode : public DOMNode {
public:
    ElementNode(const std::string& tag_name) : tag_name_(tag_name) {}
    
    NodeType getType() const override { return NodeType::Element; }
    
    const std::string& getTagName() const { return tag_name_; }
    const std::unordered_map<std::string, std::string>& getAttributes() const { return attributes_; }
    
    void setAttribute(const std::string& name, const std::string& value) {
        attributes_[name] = value;
    }
    
    std::string getAttribute(const std::string& name) const {
        auto it = attributes_.find(name);
        return it != attributes_.end() ? it->second : "";
    }
    
    bool hasAttribute(const std::string& name) const {
        return attributes_.find(name) != attributes_.end();
    }

private:
    std::string tag_name_;
    std::unordered_map<std::string, std::string> attributes_;
};

class TextNode : public DOMNode {
public:
    TextNode(const std::string& text) : text_(text) {}
    
    NodeType getType() const override { return NodeType::Text; }
    
    const std::string& getText() const { return text_; }
    void setText(const std::string& text) { text_ = text; }

private:
    std::string text_;
};

class DocumentNode : public DOMNode {
public:
    NodeType getType() const override { return NodeType::Document; }
};

} // namespace epub