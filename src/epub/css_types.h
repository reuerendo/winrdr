#pragma once

#include "formatted_text.h"
#include <string>
#include <unordered_map>
#include <cstdint>

typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace epub {

// CSS value types for length values
enum class CSSValueType {
    Inherited,
    Unspecified,
    Percent,
    EM,
    REM,
    PX,
    PT,
    CM,
    MM,
    IN,
    Color
};

// Generic CSS values (auto, none, normal, etc.)
enum class CSSGenericValue {
    Auto,
    None,
    Normal,
    CurrentColor,
    Transparent
};

struct CSSLength {
    CSSValueType type = CSSValueType::Unspecified;
    int value = 0; // stored as fixed point (value * 256)
    
    CSSLength() = default;
    CSSLength(CSSValueType t, int v) : type(t), value(v) {}
    
    bool isInherited() const { return type == CSSValueType::Unspecified && value == 0; }
    
    static CSSLength inherited() {
        return CSSLength(CSSValueType::Inherited, 0);
    }
    
    static CSSLength unspecified(CSSGenericValue generic) {
        return CSSLength(CSSValueType::Unspecified, static_cast<int>(generic));
    }
    
    static CSSLength px(int pixels) {
        return CSSLength(CSSValueType::PX, pixels * 256);
    }
    
    static CSSLength percent(int pct) {
        return CSSLength(CSSValueType::Percent, pct * 256);
    }
    
    static CSSLength em(float ems) {
        return CSSLength(CSSValueType::EM, static_cast<int>(ems * 256.0f));
    }
    
    static CSSLength rem(float rems) {
        return CSSLength(CSSValueType::REM, static_cast<int>(rems * 256.0f));
    }
    
    static CSSLength color(uint32_t rgb) {
        return CSSLength(CSSValueType::Color, static_cast<int>(rgb));
    }
};

enum class CSSDisplay {
    Inherit,
    None,
    Block,
    Inline,
    InlineBlock,
    ListItem
};

enum class CSSVerticalAlign {
    Inherit,
    Baseline,
    Sub,
    Super,
    Top,
    Middle,
    Bottom
};

enum class CSSTextTransform {
    Inherit,
    None,
    Uppercase,
    Lowercase,
    Capitalize
};

enum class CSSWhiteSpace {
    Inherit,
    Normal,
    Pre,
    PreWrap,
    PreLine,
    Nowrap
};

enum class CSSPageBreak {
    Inherit,
    Auto,
    Avoid,
    Always
};

// Importance levels for cascade
enum class CSSImportance : uint8_t {
    Normal = 0,
    Important = 1,
    HigherImportance = 2 // for user-agent !important
};

// Bit flags for tracking which properties were set
enum CSSPropertyBit : uint64_t {
    imp_bit_display = 1ULL << 0,
    imp_bit_white_space = 1ULL << 1,
    imp_bit_text_align = 1ULL << 2,
    imp_bit_text_decoration = 1ULL << 3,
    imp_bit_text_transform = 1ULL << 4,
    imp_bit_vertical_align = 1ULL << 5,
    imp_bit_font_family = 1ULL << 6,
    imp_bit_font_style = 1ULL << 7,
    imp_bit_font_weight = 1ULL << 8,
    imp_bit_font_size = 1ULL << 9,
    imp_bit_line_height = 1ULL << 10,
    imp_bit_letter_spacing = 1ULL << 11,
    imp_bit_color = 1ULL << 12,
    imp_bit_background_color = 1ULL << 13,
    imp_bit_margin_top = 1ULL << 14,
    imp_bit_margin_right = 1ULL << 15,
    imp_bit_margin_bottom = 1ULL << 16,
    imp_bit_margin_left = 1ULL << 17,
    imp_bit_padding_top = 1ULL << 18,
    imp_bit_padding_right = 1ULL << 19,
    imp_bit_padding_bottom = 1ULL << 20,
    imp_bit_padding_left = 1ULL << 21,
    imp_bit_text_indent = 1ULL << 22,
    imp_bit_page_break_before = 1ULL << 23,
    imp_bit_page_break_after = 1ULL << 24,
    imp_bit_page_break_inside = 1ULL << 25
};

// Style with cascade support
struct CSSComputedStyle {
    // Display properties
    CSSDisplay display = CSSDisplay::Inline;
    CSSWhiteSpace white_space = CSSWhiteSpace::Normal;
    TextAlign text_align = TextAlign::Left;
    CSSVerticalAlign vertical_align = CSSVerticalAlign::Baseline;
    CSSTextTransform text_transform = CSSTextTransform::None;
    
    // Text decoration flags
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool monospace = false;
    bool small_caps = false;
    
    // Font properties
    CSSLength font_size = CSSLength::em(1.0f);
    CSSLength line_height = CSSLength::unspecified(CSSGenericValue::Normal);
    CSSLength letter_spacing = CSSLength::unspecified(CSSGenericValue::Normal);
    
    // Colors
    CSSLength color = CSSLength::color(0x000000);
    CSSLength background_color = CSSLength::color(0xFFFFFF00); // transparent
    
    // Box model
    CSSLength margin_top = CSSLength::px(0);
    CSSLength margin_right = CSSLength::px(0);
    CSSLength margin_bottom = CSSLength::px(0);
    CSSLength margin_left = CSSLength::px(0);
    
    CSSLength padding_top = CSSLength::px(0);
    CSSLength padding_right = CSSLength::px(0);
    CSSLength padding_bottom = CSSLength::px(0);
    CSSLength padding_left = CSSLength::px(0);
    
    CSSLength text_indent = CSSLength::px(0);
    
    // Page breaks
    CSSPageBreak page_break_before = CSSPageBreak::Auto;
    CSSPageBreak page_break_after = CSSPageBreak::Auto;
    CSSPageBreak page_break_inside = CSSPageBreak::Auto;
    
    // Importance tracking for cascade
    uint64_t importance_bits = 0;
    uint8_t importance_levels[32] = {0}; // CSSImportance for each property
    
    // Apply a property value with cascade rules
    template<typename T>
    void apply(const T& value, T* target, CSSPropertyBit bit, CSSImportance importance) {
        const int bit_index = __builtin_ctzll(static_cast<uint64_t>(bit));
        const uint8_t current_importance = importance_levels[bit_index];
        const uint8_t new_importance = static_cast<uint8_t>(importance);
        
        if (!(importance_bits & bit) || new_importance >= current_importance) {
            *target = value;
            importance_bits |= bit;
            importance_levels[bit_index] = new_importance;
        }
    }
    
    // Inherit property from parent
    template<typename T>
    void inherit(const T& parent_value, T* target) {
        *target = parent_value;
    }
};

struct PropertyValue {
    std::string value;
    int specificity;
    CSSImportance importance;
};

struct RuleData {
    std::string selector;
    std::unordered_map<std::string, std::string> properties;
    int specificity;
    CSSImportance importance;
};

} // namespace epub