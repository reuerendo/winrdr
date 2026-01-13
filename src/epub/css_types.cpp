#include "css_types.h"

namespace epub {

void CSSComputedStyle::inheritFrom(const CSSComputedStyle& parent) {
    // Inherit properties based on crengine's logic
    
    // White space is inherited
    if (white_space == CSSWhiteSpace::Inherit) {
        white_space = parent.white_space;
    }
    
    // Text align is inherited
    if (text_align == CSSTextAlign::Inherit) {
        text_align = parent.text_align;
    }
    
    if (text_align_last == CSSTextAlign::Inherit) {
        text_align_last = parent.text_align_last;
    }
    
    // Text decoration is inherited (non-standard but common)
    if (text_decoration == CSSTextDecoration::Inherit) {
        text_decoration = parent.text_decoration;
    }
    
    // Text transform is inherited
    if (text_transform == CSSTextTransform::Inherit) {
        text_transform = parent.text_transform;
    }
    
    // Font properties are inherited
    if (font_family == CSSFontFamily::Inherit) {
        font_family = parent.font_family;
    }
    
    if (font_name.empty()) {
        font_name = parent.font_name;
    }
    
    if (font_style == CSSFontStyle::Inherit) {
        font_style = parent.font_style;
    }
    
    if (font_weight == CSSFontWeight::Inherit) {
        font_weight = parent.font_weight;
    }
    
    if (font_size.isInherited()) {
        font_size = parent.font_size;
    }
    
    // Line height is inherited
    if (line_height.isInherited()) {
        line_height = parent.line_height;
    }
    
    // Letter spacing is inherited
    if (letter_spacing.isInherited()) {
        letter_spacing = parent.letter_spacing;
    }
    
    // Color is inherited
    if (color.isInherited()) {
        color = parent.color;
    }
    
    // Vertical align for sub/super can be inherited
    if (vertical_align == CSSVerticalAlign::Inherit) {
        vertical_align = parent.vertical_align;
    }
    
    // Text indent is inherited
    if (text_indent.isInherited()) {
        text_indent = parent.text_indent;
    }
    
    // Page breaks can be inherited in some cases
    if (page_break_before == CSSPageBreak::Inherit) {
        page_break_before = parent.page_break_before;
    }
    
    if (page_break_after == CSSPageBreak::Inherit) {
        page_break_after = parent.page_break_after;
    }
    
    if (page_break_inside == CSSPageBreak::Inherit) {
        page_break_inside = parent.page_break_inside;
    }
}

} // namespace epub