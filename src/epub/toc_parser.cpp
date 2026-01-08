#include "toc_parser.h"
#include "../utils/logger.h"
#include <algorithm>

namespace epub {

TOCParser::TOCParser() {}

bool TOCParser::parseNCX(const std::string& ncx_content) {
    LOG_DEBUG("Parsing NCX table of contents");
    
    items_.clear();
    
    size_t nav_map = ncx_content.find("<navMap");
    if (nav_map == std::string::npos) {
        LOG_WARNING("navMap not found in NCX");
        return false;
    }
    
    size_t pos = nav_map;
    parseNavPoint(ncx_content, pos, 0);
    
    LOG_INFO("Parsed", items_.size(), "TOC items from NCX");
    return !items_.empty();
}

bool TOCParser::parseNav(const std::string& nav_html) {
    LOG_DEBUG("Parsing EPUB3 navigation document");
    
    items_.clear();
    
    size_t nav_toc = nav_html.find("epub:type=\"toc\"");
    if (nav_toc == std::string::npos) {
        nav_toc = nav_html.find("type=\"toc\"");
    }
    
    if (nav_toc == std::string::npos) {
        LOG_WARNING("TOC navigation not found");
        return false;
    }
    
    size_t pos = nav_toc;
    parseNavList(nav_html, pos, 0);
    
    LOG_INFO("Parsed", items_.size(), "TOC items from nav");
    return !items_.empty();
}

void TOCParser::parseNavPoint(const std::string& xml, size_t& pos, int level) {
    while (pos < xml.length()) {
        size_t nav_point = xml.find("<navPoint", pos);
        if (nav_point == std::string::npos) break;
        
        size_t nav_end = xml.find("</navPoint>", nav_point);
        if (nav_end == std::string::npos) break;
        
        std::string nav_section = xml.substr(nav_point, nav_end - nav_point);
        
        TOCItem item;
        item.level = level;
        
        // Extract title
        item.title = extractText(nav_section, "text");
        
        // Extract href
        size_t content_pos = nav_section.find("<content");
        if (content_pos != std::string::npos) {
            size_t src_start = nav_section.find("src=\"", content_pos);
            if (src_start != std::string::npos) {
                src_start += 5;
                size_t src_end = nav_section.find("\"", src_start);
                item.href = normalizeHref(nav_section.substr(src_start, src_end - src_start));
            }
        }
        
        if (!item.title.empty() && !item.href.empty()) {
            items_.push_back(item);
            LOG_DEBUG("TOC item:", item.title, "->", item.href);
        }
        
        // Parse nested navPoints
        size_t nested_start = nav_section.find("<navPoint", 1);
        if (nested_start != std::string::npos) {
            nested_start += nav_point;
            parseNavPoint(xml, nested_start, level + 1);
        }
        
        pos = nav_end + 11;
    }
}

void TOCParser::parseNavList(const std::string& html, size_t& pos, int level) {
    size_t ol_start = html.find("<ol", pos);
    if (ol_start == std::string::npos) return;
    
    size_t ol_end = html.find("</ol>", ol_start);
    if (ol_end == std::string::npos) return;
    
    std::string ol_section = html.substr(ol_start, ol_end - ol_start);
    
    size_t li_pos = 0;
    while (li_pos < ol_section.length()) {
        size_t li_start = ol_section.find("<li", li_pos);
        if (li_start == std::string::npos) break;
        
        size_t li_end = ol_section.find("</li>", li_start);
        if (li_end == std::string::npos) break;
        
        std::string li_section = ol_section.substr(li_start, li_end - li_start);
        
        TOCItem item;
        item.level = level;
        
        // Extract href and title from <a> tag
        size_t a_start = li_section.find("<a");
        if (a_start != std::string::npos) {
            size_t href_start = li_section.find("href=\"", a_start);
            if (href_start != std::string::npos) {
                href_start += 6;
                size_t href_end = li_section.find("\"", href_start);
                item.href = normalizeHref(li_section.substr(href_start, href_end - href_start));
            }
            
            size_t a_close = li_section.find(">", a_start);
            size_t a_end = li_section.find("</a>", a_close);
            if (a_close != std::string::npos && a_end != std::string::npos) {
                item.title = li_section.substr(a_close + 1, a_end - a_close - 1);
                
                // Remove nested tags
                size_t tag_start;
                while ((tag_start = item.title.find('<')) != std::string::npos) {
                    size_t tag_end = item.title.find('>', tag_start);
                    if (tag_end != std::string::npos) {
                        item.title.erase(tag_start, tag_end - tag_start + 1);
                    } else {
                        break;
                    }
                }
            }
        }
        
        if (!item.title.empty() && !item.href.empty()) {
            items_.push_back(item);
            LOG_DEBUG("TOC item:", item.title, "->", item.href);
        }
        
        // Parse nested <ol>
        size_t nested_ol = li_section.find("<ol", 1);
        if (nested_ol != std::string::npos) {
            nested_ol += li_start;
            parseNavList(html, nested_ol, level + 1);
        }
        
        li_pos = li_end + 5;
    }
    
    pos = ol_end + 5;
}

std::string TOCParser::extractText(const std::string& xml, const std::string& tag) {
    std::string open_tag = "<" + tag;
    size_t start = xml.find(open_tag);
    if (start == std::string::npos) return "";
    
    start = xml.find(">", start);
    if (start == std::string::npos) return "";
    start++;
    
    std::string close_tag = "</" + tag + ">";
    size_t end = xml.find(close_tag, start);
    if (end == std::string::npos) return "";
    
    return xml.substr(start, end - start);
}

std::string TOCParser::extractAttribute(const std::string& tag_content, 
                                       const std::string& attr) {
    std::string search = attr + "=\"";
    size_t pos = tag_content.find(search);
    if (pos != std::string::npos) {
        pos += search.length();
        size_t end = tag_content.find("\"", pos);
        if (end != std::string::npos) {
            return tag_content.substr(pos, end - pos);
        }
    }
    return "";
}

std::string TOCParser::normalizeHref(const std::string& href) {
    // Remove anchor
    size_t anchor = href.find('#');
    if (anchor != std::string::npos) {
        return href.substr(0, anchor);
    }
    return href;
}

void TOCParser::clear() {
    items_.clear();
}

size_t TOCParser::findChapterIndex(const std::string& href) const {
    std::string normalized = normalizeHref(href);
    
    for (size_t i = 0; i < items_.size(); i++) {
        if (items_[i].href == normalized) {
            return items_[i].spine_index;
        }
    }
    
    return 0;
}

} // namespace epub