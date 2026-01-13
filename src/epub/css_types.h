#pragma once

#include "formatted_text.h"
#include <string>
#include <unordered_map>
#include <cstdint>

typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace epub {

// CSS value types based on crengine's css_value_type_t
enum class CSSValueType {
    Inherited,      // css_val_inherited
    Unspecified,    // css_val_unspecified
    Percent,        // css_val_percent
    EM,            // css_val_em
    REM,           // css_val_rem
    EX,            // css_val_ex
    PX,            // css_val_px (screen_px in crengine)
    PT,            // css_val_pt
    CM,            // css_val_cm
    MM,            // css_val_mm
    IN,            // css_val_in
    PC,            // css_val_pc
    Color          // css_val_color
};

// Forward declare toPixels for use in inline methods
int cssLengthToPixels(CSSValueType type, int value, int base_font_size, int parent_value);

// Generic CSS values
enum class CSSGenericValue {
    Auto,           // css_generic_auto
    None,           // css_generic_none
    Normal,         // css_generic_normal
    CurrentColor,   // css_generic_currentcolor
    Transparent     // CSS_COLOR_TRANSPARENT
};

// CSS length value (based on crengine's css_length_t)
struct CSSLength {
    CSSValueType type = CSSValueType::Unspecified;
    int value = 0; // stored as fixed point (value * 256)
    
    CSSLength() = default;
    CSSLength(CSSValueType t, int v) : type(t), value(v) {}
    
    bool isInherited() const { 
        return type == CSSValueType::Inherited; 
    }
    
    bool isUnspecified() const {
        return type == CSSValueType::Unspecified;
    }
    
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
    
    // Forward declare - implementation in css_types.cpp
    int toPixels(int base_font_size = 16, int parent_value = 0) const;
};

// Display types (based on crengine's css_display_t)
enum class CSSDisplay {
    Inherit,        // css_d_inherit
    None,          // css_d_none
    Block,         // css_d_block
    Inline,        // css_d_inline
    InlineBlock,   // css_d_inline_block
    ListItem,      // css_d_list_item_block
    RunIn,         // css_d_run_in
    InlineTable,   // css_d_inline_table
    Table,         // css_d_table
    TableRow,      // css_d_table_row
    TableCell      // css_d_table_cell
};

// White space handling (based on crengine's css_white_space_t)
enum class CSSWhiteSpace {
    Inherit,       // css_ws_inherit
    Normal,        // css_ws_normal
    Nowrap,        // css_ws_nowrap
    Pre,          // css_ws_pre
    PreLine,       // css_ws_pre_line
    PreWrap        // css_ws_pre_wrap
};

// Text align (based on crengine's css_text_align_t)
enum class CSSTextAlign {
    Inherit,       // css_ta_inherit
    Left,         // css_ta_left
    Right,        // css_ta_right
    Center,       // css_ta_center
    Justify,      // css_ta_justify
    Start,        // css_ta_start
    End           // css_ta_end
};

// Text decoration (based on crengine's css_text_decoration_t)
enum class CSSTextDecoration {
    Inherit,       // css_td_inherit
    None,         // css_td_none
    Underline,    // css_td_underline
    Overline,     // css_td_overline
    LineThrough,  // css_td_line_through
    Blink         // css_td_blink
};

// Text transform (based on crengine's css_text_transform_t)
enum class CSSTextTransform {
    Inherit,       // css_tt_inherit
    None,         // css_tt_none
    Uppercase,    // css_tt_uppercase
    Lowercase,    // css_tt_lowercase
    Capitalize,   // css_tt_capitalize
    FullWidth     // css_tt_full_width
};

// Vertical align (based on crengine's css_vertical_align_t)
enum class CSSVerticalAlign {
    Inherit,       // css_va_inherit
    Baseline,     // css_va_baseline
    Sub,          // css_va_sub
    Super,        // css_va_super
    Top,          // css_va_top
    TextTop,      // css_va_text_top
    Middle,       // css_va_middle
    Bottom,       // css_va_bottom
    TextBottom    // css_va_text_bottom
};

// Font style (based on crengine's css_font_style_t)
enum class CSSFontStyle {
    Inherit,       // css_fs_inherit
    Normal,       // css_fs_normal
    Italic,       // css_fs_italic
    Oblique       // css_fs_oblique
};

// Font weight (based on crengine's css_font_weight_t)
enum class CSSFontWeight {
    Inherit,       // css_fw_inherit
    Normal,       // css_fw_normal (400)
    Bold,         // css_fw_bold (700)
    Bolder,       // css_fw_bolder
    Lighter,      // css_fw_lighter
    W100,         // css_fw_100
    W200,         // css_fw_200
    W300,         // css_fw_300
    W400,         // css_fw_400
    W500,         // css_fw_500
    W600,         // css_fw_600
    W700,         // css_fw_700
    W800,         // css_fw_800
    W900          // css_fw_900
};

// Font family (based on crengine's css_font_family_t)
enum class CSSFontFamily {
    Inherit,       // css_ff_inherit
    Serif,        // css_ff_serif
    SansSerif,    // css_ff_sans_serif
    Cursive,      // css_ff_cursive
    Fantasy,      // css_ff_fantasy
    Monospace     // css_ff_monospace
};

// Page break (based on crengine's css_page_break_t)
enum class CSSPageBreak {
    Inherit,       // css_pb_inherit
    Auto,         // css_pb_auto
    Avoid,        // css_pb_avoid
    Always,       // css_pb_always
    Left,         // css_pb_left
    Right,        // css_pb_right
    Page          // css_pb_page
};

// Importance levels for CSS cascade
static constexpr uint32_t IMPORTANT_DECL_HIGHER = 0x80000000U;
static constexpr uint32_t IMPORTANT_DECL_SET = 0x40000000U;
static constexpr uint32_t IMPORTANT_DECL_REMOVE = 0x3FFFFFFFU;

// Property bit flags for cascade tracking
enum CSSPropertyBit : uint64_t {
    imp_bit_display = 1ULL << 0,
    imp_bit_white_space = 1ULL << 1,
    imp_bit_text_align = 1ULL << 2,
    imp_bit_text_align_last = 1ULL << 3,
    imp_bit_text_decoration = 1ULL << 4,
    imp_bit_text_transform = 1ULL << 5,
    imp_bit_vertical_align = 1ULL << 6,
    imp_bit_font_family = 1ULL << 7,
    imp_bit_font_style = 1ULL << 8,
    imp_bit_font_weight = 1ULL << 9,
    imp_bit_font_size = 1ULL << 10,
    imp_bit_line_height = 1ULL << 11,
    imp_bit_letter_spacing = 1ULL << 12,
    imp_bit_color = 1ULL << 13,
    imp_bit_background_color = 1ULL << 14,
    imp_bit_margin_top = 1ULL << 15,
    imp_bit_margin_right = 1ULL << 16,
    imp_bit_margin_bottom = 1ULL << 17,
    imp_bit_margin_left = 1ULL << 18,
    imp_bit_padding_top = 1ULL << 19,
    imp_bit_padding_right = 1ULL << 20,
    imp_bit_padding_bottom = 1ULL << 21,
    imp_bit_padding_left = 1ULL << 22,
    imp_bit_text_indent = 1ULL << 23,
    imp_bit_page_break_before = 1ULL << 24,
    imp_bit_page_break_after = 1ULL << 25,
    imp_bit_page_break_inside = 1ULL << 26,
    imp_bit_width = 1ULL << 27,
    imp_bit_height = 1ULL << 28
};

// Computed style record (based on crengine's css_style_rec_t)
struct CSSComputedStyle {
    // Display and layout
    CSSDisplay display = CSSDisplay::Inline;
    CSSWhiteSpace white_space = CSSWhiteSpace::Normal;
    CSSTextAlign text_align = CSSTextAlign::Left;
    CSSTextAlign text_align_last = CSSTextAlign::Inherit;
    
    // Text decoration and transformation
    CSSTextDecoration text_decoration = CSSTextDecoration::None;
    CSSTextTransform text_transform = CSSTextTransform::None;
    CSSVerticalAlign vertical_align = CSSVerticalAlign::Baseline;
    
    // Font properties
    CSSFontFamily font_family = CSSFontFamily::SansSerif;
    std::string font_name;
    CSSFontStyle font_style = CSSFontStyle::Normal;
    CSSFontWeight font_weight = CSSFontWeight::Normal;
    CSSLength font_size = CSSLength::rem(1.0f);
    
    // Line and letter spacing
    CSSLength line_height = CSSLength::unspecified(CSSGenericValue::Normal);
    CSSLength letter_spacing = CSSLength::unspecified(CSSGenericValue::Normal);
    
    // Colors
    CSSLength color = CSSLength::color(0x000000);
    CSSLength background_color = CSSLength::color(0xFFFFFF);
    
    // Box model - margins
    CSSLength margin[4] = {
        CSSLength::px(0), // top
        CSSLength::px(0), // right
        CSSLength::px(0), // bottom
        CSSLength::px(0)  // left
    };
    
    // Box model - padding
    CSSLength padding[4] = {
        CSSLength::px(0), // top
        CSSLength::px(0), // right
        CSSLength::px(0), // bottom
        CSSLength::px(0)  // left
    };
    
    // Text indent
    CSSLength text_indent = CSSLength::px(0);
    
    // Dimensions
    CSSLength width = CSSLength::unspecified(CSSGenericValue::Auto);
    CSSLength height = CSSLength::unspecified(CSSGenericValue::Auto);
    
    // Page breaks
    CSSPageBreak page_break_before = CSSPageBreak::Auto;
    CSSPageBreak page_break_after = CSSPageBreak::Auto;
    CSSPageBreak page_break_inside = CSSPageBreak::Auto;
    
    // Cascade tracking
    uint64_t importance_bits = 0;
    uint8_t importance_levels[64] = {0};
    
    // Apply property with cascade rules (based on crengine's Apply method)
    template<typename T>
    void apply(const T& value, T* target, CSSPropertyBit bit, uint8_t importance) {
        const int bit_index = __builtin_ctzll(static_cast<uint64_t>(bit));
        const uint8_t current_importance = importance_levels[bit_index];
        
        if (!(importance_bits & bit) || importance >= current_importance) {
            *target = value;
            importance_bits |= bit;
            importance_levels[bit_index] = importance;
        }
    }
    
    // Inherit from parent style
    void inheritFrom(const CSSComputedStyle& parent);
};

// Property value with specificity
struct PropertyValue {
    std::string value;
    int specificity;
    bool is_important;
};

// CSS rule data
struct RuleData {
    std::string selector;
    std::unordered_map<std::string, std::string> properties;
    int specificity;
    bool is_important;
};

} // namespace epub