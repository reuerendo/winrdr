#include "css_debug_logger.h"
#include "../utils/logger.h"
#include <lexbor/html/interfaces/element.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace epub {

void CSSDebugLogger::logElement(lxb_dom_node_t* node,
                                const std::vector<std::string>& matched_selectors,
                                const CSSComputedStyle& style,
                                int depth) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return;
    }
    
    CSSDebugInfo info;
    info.tag_name = getTagName(node);
    info.id = getIdName(node);
    info.class_name = getClassName(node);
    info.matched_selectors = matched_selectors;
    info.computed_style = style;
    info.depth = depth;
    
    elements_.push_back(info);
}

void CSSDebugLogger::printReport(const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open CSS debug report file:", output_path);
        return;
    }
    
    writeReport(file);
    
    file.close();
    LOG_INFO("CSS debug report saved to:", output_path);
}

void CSSDebugLogger::printReportToConsole() {
    writeReport(std::cout);
}

void CSSDebugLogger::writeReport(std::ostream& out) {
    out << "╔════════════════════════════════════════════════════════════════╗\n";
    out << "║           CSS DEBUG REPORT - STYLESHEET ANALYSIS              ║\n";
    out << "╚════════════════════════════════════════════════════════════════╝\n\n";
    out << "Total elements analyzed: " << elements_.size() << "\n\n";
    
    for (const auto& elem : elements_) {
        std::string indent(elem.depth * 2, ' ');
        
        out << indent << "┌─ <" << elem.tag_name;
        if (!elem.id.empty()) {
            out << " id=\"" << elem.id << "\"";
        }
        if (!elem.class_name.empty()) {
            out << " class=\"" << elem.class_name << "\"";
        }
        out << ">\n";
        
        if (!elem.matched_selectors.empty()) {
            out << indent << "│  Matched selectors:\n";
            for (const auto& selector : elem.matched_selectors) {
                out << indent << "│    • " << selector << "\n";
            }
        }
        
        std::string style_output = formatStyle(elem.computed_style);
        if (!style_output.empty()) {
            out << indent << "│  Computed styles:\n";
            out << style_output;
        } else {
            out << indent << "│  No computed styles\n";
        }
        
        out << indent << "└─\n\n";
    }
}

void CSSDebugLogger::clear() {
    elements_.clear();
}

std::string CSSDebugLogger::formatStyle(const CSSComputedStyle& style) {
    std::ostringstream oss;
    bool has_styles = false;
    
    if (style.bold || style.italic || style.underline || style.strikethrough || 
        style.monospace || style.small_caps) {
        oss << "       │    Text decorations: ";
        if (style.bold) oss << "bold ";
        if (style.italic) oss << "italic ";
        if (style.underline) oss << "underline ";
        if (style.strikethrough) oss << "strikethrough ";
        if (style.monospace) oss << "monospace ";
        if (style.small_caps) oss << "small-caps ";
        oss << "\n";
        has_styles = true;
    }
    
    if (style.font_size != 1.0f) {
        oss << "       │    font-size: " << std::fixed << std::setprecision(2) 
            << style.font_size << "em\n";
        has_styles = true;
    }
    
    if (style.line_height != 1.2f) {
        oss << "       │    line-height: " << std::fixed << std::setprecision(2) 
            << style.line_height << "\n";
        has_styles = true;
    }
    
    if (style.letter_spacing != 0.0f) {
        oss << "       │    letter-spacing: " << std::fixed << std::setprecision(2) 
            << style.letter_spacing << "\n";
        has_styles = true;
    }
    
    if (style.margin_top != 0 || style.margin_bottom != 0 || 
        style.margin_left != 0 || style.margin_right != 0) {
        oss << "       │    margin: " << style.margin_top << "px " 
            << style.margin_right << "px " << style.margin_bottom << "px " 
            << style.margin_left << "px\n";
        has_styles = true;
    }
    
    if (style.padding_top != 0 || style.padding_bottom != 0 || 
        style.padding_left != 0 || style.padding_right != 0) {
        oss << "       │    padding: " << style.padding_top << "px " 
            << style.padding_right << "px " << style.padding_bottom << "px " 
            << style.padding_left << "px\n";
        has_styles = true;
    }
    
    if (style.text_indent != 0) {
        oss << "       │    text-indent: " << style.text_indent << "px\n";
        has_styles = true;
    }
    
    if (style.display != CSSDisplay::Inline) {
        oss << "       │    display: ";
        switch (style.display) {
            case CSSDisplay::None: oss << "none"; break;
            case CSSDisplay::Block: oss << "block"; break;
            case CSSDisplay::InlineBlock: oss << "inline-block"; break;
            case CSSDisplay::ListItem: oss << "list-item"; break;
            default: oss << "inline"; break;
        }
        oss << "\n";
        has_styles = true;
    }
    
    if (style.vertical_align != CSSVerticalAlign::Baseline) {
        oss << "       │    vertical-align: ";
        switch (style.vertical_align) {
            case CSSVerticalAlign::Sub: oss << "sub"; break;
            case CSSVerticalAlign::Super: oss << "super"; break;
            case CSSVerticalAlign::Top: oss << "top"; break;
            case CSSVerticalAlign::Middle: oss << "middle"; break;
            case CSSVerticalAlign::Bottom: oss << "bottom"; break;
            default: oss << "baseline"; break;
        }
        oss << "\n";
        has_styles = true;
    }
    
    if (style.text_align != TextAlign::Left) {
        oss << "       │    text-align: ";
        switch (style.text_align) {
            case TextAlign::Center: oss << "center"; break;
            case TextAlign::Right: oss << "right"; break;
            case TextAlign::Justify: oss << "justify"; break;
            default: oss << "left"; break;
        }
        oss << "\n";
        has_styles = true;
    }
    
    if (style.text_transform != CSSTextTransform::None) {
        oss << "       │    text-transform: ";
        switch (style.text_transform) {
            case CSSTextTransform::Uppercase: oss << "uppercase"; break;
            case CSSTextTransform::Lowercase: oss << "lowercase"; break;
            case CSSTextTransform::Capitalize: oss << "capitalize"; break;
            default: oss << "none"; break;
        }
        oss << "\n";
        has_styles = true;
    }
    
    if (style.white_space != CSSWhiteSpace::Normal) {
        oss << "       │    white-space: ";
        switch (style.white_space) {
            case CSSWhiteSpace::Pre: oss << "pre"; break;
            case CSSWhiteSpace::PreWrap: oss << "pre-wrap"; break;
            case CSSWhiteSpace::PreLine: oss << "pre-line"; break;
            case CSSWhiteSpace::Nowrap: oss << "nowrap"; break;
            default: oss << "normal"; break;
        }
        oss << "\n";
        has_styles = true;
    }
    
    if (style.page_break_before || style.page_break_after || style.page_break_inside_avoid) {
        oss << "       │    page-break: ";
        if (style.page_break_before) oss << "before ";
        if (style.page_break_after) oss << "after ";
        if (style.page_break_inside_avoid) oss << "inside-avoid ";
        oss << "\n";
        has_styles = true;
    }
    
    if (!has_styles) {
        return "";
    }
    
    return oss.str();
}

std::string CSSDebugLogger::getTagName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    const lxb_char_t* tag_name_raw = lxb_dom_element_qualified_name(element, nullptr);
    
    if (tag_name_raw) {
        return std::string(reinterpret_cast<const char*>(tag_name_raw));
    }
    
    return "";
}

std::string CSSDebugLogger::getClassName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>("class"),
        5,
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

std::string CSSDebugLogger::getIdName(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    
    lxb_dom_element_t* element = lxb_dom_interface_element(node);
    
    size_t attr_len = 0;
    const lxb_char_t* attr_value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>("id"),
        2,
        &attr_len
    );
    
    if (attr_value && attr_len > 0) {
        return std::string(reinterpret_cast<const char*>(attr_value), attr_len);
    }
    
    return "";
}

} // namespace epub