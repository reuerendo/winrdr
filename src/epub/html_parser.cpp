#include "html_parser.h"
#include "zip_handler.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace epub {

HTMLParser::HTMLParser() : image_cache_(nullptr) {}
HTMLParser::~HTMLParser() {}

void HTMLParser::setImageCache(ImageCache* cache) {
    image_cache_ = cache;
}

bool HTMLParser::isInlineElement(const std::string& tag) {
    return tag == "span" || tag == "b" || tag == "strong" || 
           tag == "i" || tag == "em" || tag == "u" || tag == "s" ||
           tag == "strike" || tag == "del" || tag == "ins" ||
           tag == "small" || tag == "sub" || tag == "sup" ||
           tag == "code" || tag == "kbd" || tag == "a" ||
           tag == "abbr" || tag == "cite" || tag == "q" || tag == "ruby";
}

FormattedContent HTMLParser::parse(const std::string& html, ZipHandler* zip, 
                                   const std::string& base_path) {
    FormattedContent content;
    ParseContext ctx;
    
    // Parse inline CSS
    size_t style_start = html.find("<style");
    while (style_start != std::string::npos) {
        size_t style_end = html.find("</style>", style_start);
        if (style_end != std::string::npos) {
            size_t content_start = html.find('>', style_start) + 1;
            std::string css = html.substr(content_start, style_end - content_start);
            css_parser_.parseStylesheet(css);
        }
        style_start = html.find("<style", style_end);
    }
    
    size_t pos = 0;
    parseNode(html, pos, content, ctx, zip, base_path);
    
    // Flush any accumulated text
    if (!ctx.accumulated_text.empty()) {
        TextElement elem;
        elem.type = ctx.current_element;
        elem.content = ctx.accumulated_text;
        elem.style = ctx.accumulated_style;
        elem.align = ctx.current_align;
        elem.list_level = ctx.list_level;
        content.push_back(elem);
    }
    
    return content;
}

void HTMLParser::parseNode(const std::string& html, size_t& pos, 
                           FormattedContent& content, ParseContext& ctx,
                           ZipHandler* zip, const std::string& base_path) {
    while (pos < html.length()) {
        if (html[pos] == '<') {
            // Check for comment
            if (pos + 3 < html.length() && html.substr(pos, 4) == "<!--") {
                size_t comment_end = html.find("-->", pos);
                if (comment_end != std::string::npos) {
                    pos = comment_end + 3;
                    continue;
                }
            }
            
            // Check for closing tag
            if (pos + 1 < html.length() && html[pos + 1] == '/') {
                size_t tag_end = html.find('>', pos);
                if (tag_end != std::string::npos) {
                    std::string tag_name = extractTagName(html.substr(pos + 2, tag_end - pos - 2));
                    handleCloseTag(tag_name, ctx, content);
                    pos = tag_end + 1;
                    continue;
                }
            }
            
            // Opening tag
            size_t tag_end = html.find('>', pos);
            if (tag_end != std::string::npos) {
                bool self_closing = (html[tag_end - 1] == '/');
                std::string tag_content = html.substr(pos + 1, tag_end - pos - 1);
                std::string tag_name = extractTagName(tag_content);
                
                handleOpenTag(tag_name, tag_content, ctx, content, zip, base_path);
                
                if (self_closing || tag_name == "br" || tag_name == "img") {
                    handleCloseTag(tag_name, ctx, content);
                }
                
                pos = tag_end + 1;
                continue;
            }
        }
        
        // Text content
        size_t next_tag = html.find('<', pos);
        if (next_tag == std::string::npos) {
            next_tag = html.length();
        }
        
        std::string text = html.substr(pos, next_tag - pos);
        if (!text.empty()) {
            addText(text, ctx, content);
        }
        
        pos = next_tag;
    }
}

bool HTMLParser::isBlockElement(const std::string& tag) {
    return tag == "p" || tag == "div" || tag == "h1" || tag == "h2" || 
           tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" ||
           tag == "blockquote" || tag == "li" || tag == "pre" ||
           tag == "article" || tag == "section" || tag == "aside" ||
           tag == "header" || tag == "footer" || tag == "main" ||
           tag == "figure" || tag == "figcaption";
}

void HTMLParser::flushAccumulatedText(ParseContext& ctx, FormattedContent& content) {
    if (!ctx.accumulated_text.empty()) {
        TextElement elem;
        elem.type = ctx.current_element;
        elem.content = ctx.accumulated_text;
        elem.style = ctx.accumulated_style;
        elem.align = ctx.current_align;
        elem.list_level = ctx.list_level;
        content.push_back(elem);
        
        ctx.accumulated_text.clear();
        ctx.accumulated_style = ctx.current_style;
    }
}

void HTMLParser::handleOpenTag(const std::string& tag, const std::string& attributes,
                               ParseContext& ctx, FormattedContent& content,
                               ZipHandler* zip, const std::string& base_path) {
    std::string tag_lower = tag;
    std::transform(tag_lower.begin(), tag_lower.end(), tag_lower.begin(), ::tolower);
    
    // Skip tags that should not render content
    if (tag_lower == "script" || tag_lower == "style" || tag_lower == "title" || 
        tag_lower == "head" || tag_lower == "meta" || tag_lower == "link") {
        context_stack_.push(ctx);
        ctx.skip_content = true;
        return;
    }
    
    // For block elements, flush accumulated text first
    if (isBlockElement(tag_lower)) {
        flushAccumulatedText(ctx, content);
    }
    
    context_stack_.push(ctx);
    
    // Block elements
    if (tag_lower == "p") {
        ctx.current_element = ElementType::Paragraph;
        ctx.in_paragraph = true;
    }
    else if (tag_lower == "article" || tag_lower == "section" || tag_lower == "aside" ||
             tag_lower == "header" || tag_lower == "footer" || tag_lower == "main" ||
             tag_lower == "figure") {
        ctx.current_element = ElementType::Paragraph;
    }
    else if (tag_lower == "figcaption") {
        ctx.current_element = ElementType::Text;
        ctx.current_style = ctx.current_style | TextStyle::Italic | TextStyle::Small;
    }
    
    // Headings
    else if (tag_lower == "h1") ctx.current_element = ElementType::Heading1;
    else if (tag_lower == "h2") ctx.current_element = ElementType::Heading2;
    else if (tag_lower == "h3") ctx.current_element = ElementType::Heading3;
    else if (tag_lower == "h4") ctx.current_element = ElementType::Heading4;
    else if (tag_lower == "h5") ctx.current_element = ElementType::Heading5;
    else if (tag_lower == "h6") ctx.current_element = ElementType::Heading6;
    
    // Quotes
    else if (tag_lower == "blockquote") {
        ctx.current_element = ElementType::Quote;
    }
    else if (tag_lower == "q") {
        ctx.accumulated_text += L"\"";
    }
    
    // Lists
    else if (tag_lower == "li") {
        ctx.current_element = ElementType::ListItem;
    }
    else if (tag_lower == "ul" || tag_lower == "ol") {
        ctx.list_level++;
    }
    
    // Code and preformatted
    else if (tag_lower == "code" || tag_lower == "kbd") {
        ctx.current_style = ctx.current_style | TextStyle::Monospace;
    }
    else if (tag_lower == "pre") {
        ctx.current_element = ElementType::CodeBlock;
        ctx.current_style = ctx.current_style | TextStyle::Monospace;
    }
    
    // Inline text styling - bold
    else if (tag_lower == "b" || tag_lower == "strong") {
        ctx.current_style = ctx.current_style | TextStyle::Bold;
    }
    
    // Inline text styling - italic
    else if (tag_lower == "i" || tag_lower == "em" || tag_lower == "cite") {
        ctx.current_style = ctx.current_style | TextStyle::Italic;
    }
    
    // Inline text styling - underline
    else if (tag_lower == "u" || tag_lower == "ins") {
        ctx.current_style = ctx.current_style | TextStyle::Underline;
    }
    
    // Inline text styling - strikethrough
    else if (tag_lower == "s" || tag_lower == "strike" || tag_lower == "del") {
        ctx.current_style = ctx.current_style | TextStyle::Strikethrough;
    }
    
    // Inline text styling - small
    else if (tag_lower == "small") {
        ctx.current_style = ctx.current_style | TextStyle::Small;
    }
    
    // Inline text styling - sub/sup
    else if (tag_lower == "sub") {
        ctx.current_style = ctx.current_style | TextStyle::Subscript | TextStyle::Small;
    }
    else if (tag_lower == "sup") {
        ctx.current_style = ctx.current_style | TextStyle::Superscript | TextStyle::Small;
    }
    
    // Links
    else if (tag_lower == "a") {
        ctx.current_style = ctx.current_style | TextStyle::Underline;
        std::string href = extractAttribute(attributes, "href");
    }
    
    // Horizontal rule
    else if (tag_lower == "hr") {
        flushAccumulatedText(ctx, content);
        TextElement elem;
        elem.type = ElementType::HorizontalRule;
        content.push_back(elem);
    }
    
    // Line break
    else if (tag_lower == "br") {
        flushAccumulatedText(ctx, content);
        TextElement elem;
        elem.type = ElementType::LineBreak;
        content.push_back(elem);
    }
    
    // Image
    else if (tag_lower == "img") {
        flushAccumulatedText(ctx, content);
        std::string src = extractAttribute(attributes, "src");
        if (!src.empty() && zip && image_cache_) {
            std::string img_path = normalizePath(base_path, src);
            
            std::vector<char> img_data;
            if (zip->extractFile(img_path, img_data)) {
                if (image_cache_->loadImage(img_path, img_data)) {
                    TextElement elem;
                    elem.type = ElementType::Image;
                    elem.image_id = img_path;
                    content.push_back(elem);
                }
            }
        }
    }
    
    // CSS class/style
    std::string class_name = extractAttribute(attributes, "class");
    if (!class_name.empty()) {
        CSSStyle style = css_parser_.getStyle(class_name);
        if (style.has_style) {
            ctx.current_style = ctx.current_style | style.text_style;
        }
        if (style.has_align) {
            ctx.current_align = style.align;
        }
    }
    
    std::string inline_style = extractAttribute(attributes, "style");
    if (!inline_style.empty()) {
        CSSStyle style;
        css_parser_.parseInlineStyle(inline_style, style);
        if (style.has_style) {
            ctx.current_style = ctx.current_style | style.text_style;
        }
        if (style.has_align) {
            ctx.current_align = style.align;
        }
    }
}

void HTMLParser::handleCloseTag(const std::string& tag, ParseContext& ctx,
                               FormattedContent& content) {
    std::string tag_lower = tag;
    std::transform(tag_lower.begin(), tag_lower.end(), tag_lower.begin(), ::tolower);
    
    // Add closing quote for <q> tag
    if (tag_lower == "q" && !ctx.skip_content) {
        ctx.accumulated_text += L"\"";
    }
    
    // For block elements, flush accumulated text
    if (isBlockElement(tag_lower)) {
        flushAccumulatedText(ctx, content);
        
        // Add line break after block elements
        if (!ctx.skip_content) {
            if (!content.empty() && content.back().type != ElementType::LineBreak) {
                TextElement elem;
                elem.type = ElementType::LineBreak;
                content.push_back(elem);
            }
        }
    }
    
    if (tag_lower == "ul" || tag_lower == "ol") {
        ctx.list_level--;
    }
    
    // Save accumulated text before restoring context
    std::wstring current_text = ctx.accumulated_text;
    
    if (!context_stack_.empty()) {
        ctx = context_stack_.top();
        context_stack_.pop();
        
        // Restore accumulated text for inline elements
        if (!isBlockElement(tag_lower)) {
            ctx.accumulated_text = current_text;
        }
    }
}

void HTMLParser::addText(const std::string& text, ParseContext& ctx,
                        FormattedContent& content) {
    if (ctx.skip_content) return;
    
    std::string decoded = decodeHTMLEntities(text);
    
    // Skip whitespace-only text
    bool has_content = false;
    for (char c : decoded) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            has_content = true;
            break;
        }
    }
    
    if (!has_content) return;
    
    std::wstring wide_text = utf8ToWide(decoded);
    
    // If style changed, create new element with accumulated text first
    if (!ctx.accumulated_text.empty() && ctx.accumulated_style != ctx.current_style) {
        flushAccumulatedText(ctx, content);
    }
    
    // Accumulate text with current style
    ctx.accumulated_text += wide_text;
    ctx.accumulated_style = ctx.current_style;
}

std::string HTMLParser::extractTagName(const std::string& tag_content) {
    size_t space = tag_content.find(' ');
    size_t slash = tag_content.find('/');
    size_t end = std::min(space, slash);
    
    if (end == std::string::npos) {
        end = tag_content.length();
    }
    
    return tag_content.substr(0, end);
}

std::string HTMLParser::extractAttribute(const std::string& tag_content, 
                                        const std::string& attr_name) {
    std::string search = attr_name + "=\"";
    size_t pos = tag_content.find(search);
    if (pos == std::string::npos) {
        search = attr_name + "='";
        pos = tag_content.find(search);
    }
    
    if (pos != std::string::npos) {
        pos += search.length();
        size_t end = tag_content.find(search.back(), pos);
        if (end != std::string::npos) {
            return tag_content.substr(pos, end - pos);
        }
    }
    
    return "";
}

std::string HTMLParser::decodeHTMLEntities(const std::string& text) {
    std::string result;
    
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '&') {
            if (text.substr(i, 6) == "&nbsp;") { result += ' '; i += 5; }
            else if (text.substr(i, 4) == "&lt;") { result += '<'; i += 3; }
            else if (text.substr(i, 4) == "&gt;") { result += '>'; i += 3; }
            else if (text.substr(i, 5) == "&amp;") { result += '&'; i += 4; }
            else if (text.substr(i, 6) == "&quot;") { result += '"'; i += 5; }
            else if (text.substr(i, 6) == "&apos;") { result += '\''; i += 5; }
            else result += text[i];
        } else {
            result += text[i];
        }
    }
    
    return result;
}

std::wstring HTMLParser::utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size, 0);
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

std::string HTMLParser::normalizePath(const std::string& base, const std::string& relative) {
    if (relative.empty()) return "";
    
    if (relative[0] == '/') return relative.substr(1);
    
    std::string path = relative;
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    if (!base.empty()) {
        return base + path;
    }
    
    return path;
}

} // namespace epub