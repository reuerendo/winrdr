#include "epub_parser.h"
#include "../utils/logger.h"
#include <sstream>
#include <algorithm>

namespace epub {

EpubParser::EpubParser() {
    html_parser_.setImageCache(&image_cache_);
}

EpubParser::~EpubParser() { 
    close(); 
}

bool EpubParser::open(const std::string& filepath) {
    LOG_INFO("Opening EPUB file:", filepath);
    
    if (!zip_.open(filepath)) {
        LOG_ERROR("Failed to open ZIP archive");
        return false;
    }
    
    if (!parseContainer()) {
        LOG_ERROR("Failed to parse container.xml");
        return false;
    }
    
    if (!parseOPF()) {
        LOG_ERROR("Failed to parse OPF file");
        return false;
    }
    
    if (!parseTOC()) {
        LOG_WARNING("TOC parsing failed, generating fallback TOC");
        generateFallbackTOC();
    }
    
    LOG_INFO("EPUB opened successfully");
    LOG_INFO("Title:", metadata_.title);
    LOG_INFO("Author:", metadata_.author);
    LOG_INFO("Chapters:", spine_.size());
    LOG_INFO("TOC entries:", toc_parser_.getItems().size());
    
    return true;
}

void EpubParser::close() {
    zip_.close();
    spine_.clear();
    manifest_.clear();
    image_cache_.clear();
    toc_parser_.clear();
}

bool EpubParser::parseContainer() {
    LOG_DEBUG("Parsing container.xml");
    
    std::string container = zip_.extractTextFile("META-INF/container.xml");
    if (container.empty()) {
        LOG_ERROR("container.xml not found or empty");
        return false;
    }
    
    size_t pos = container.find("full-path=\"");
    if (pos == std::string::npos) {
        LOG_ERROR("full-path attribute not found in container.xml");
        return false;
    }
    
    pos += 11;
    size_t end = container.find("\"", pos);
    if (end == std::string::npos) {
        LOG_ERROR("Malformed full-path attribute");
        return false;
    }
    
    opf_path_ = container.substr(pos, end - pos);
    LOG_INFO("OPF path found:", opf_path_);
    
    size_t last_slash = opf_path_.find_last_of("/");
    if (last_slash != std::string::npos) {
        content_dir_ = opf_path_.substr(0, last_slash + 1);
        LOG_DEBUG("Content directory:", content_dir_);
    }
    
    return true;
}

bool EpubParser::parseOPF() {
    LOG_DEBUG("Parsing OPF file:", opf_path_);
    
    std::string opf = zip_.extractTextFile(opf_path_);
    if (opf.empty()) {
        LOG_ERROR("OPF file not found or empty");
        return false;
    }
    
    metadata_.title = findTagContent(opf, "dc:title");
    metadata_.author = findTagContent(opf, "dc:creator");
    metadata_.language = findTagContent(opf, "dc:language");
    
    LOG_DEBUG("Parsed metadata - Title:", metadata_.title);
    LOG_DEBUG("Parsed metadata - Author:", metadata_.author);
    LOG_DEBUG("Parsed metadata - Language:", metadata_.language);
    
    manifest_.clear();
    size_t manifest_start = opf.find("<manifest");
    size_t manifest_end = opf.find("</manifest>", manifest_start);
    
    if (manifest_start != std::string::npos && manifest_end != std::string::npos) {
        std::string manifest_section = opf.substr(manifest_start, manifest_end - manifest_start);
        
        size_t pos = 0;
        while ((pos = manifest_section.find("<item", pos)) != std::string::npos) {
            size_t end = manifest_section.find(">", pos);
            if (end == std::string::npos) break;
            
            std::string item_tag = manifest_section.substr(pos, end - pos);
            
            SpineItem item;
            
            size_t id_pos = item_tag.find("id=\"");
            if (id_pos != std::string::npos) {
                id_pos += 4;
                size_t id_end = item_tag.find("\"", id_pos);
                item.id = item_tag.substr(id_pos, id_end - id_pos);
            }
            
            size_t href_pos = item_tag.find("href=\"");
            if (href_pos != std::string::npos) {
                href_pos += 6;
                size_t href_end = item_tag.find("\"", href_pos);
                item.href = item_tag.substr(href_pos, href_end - href_pos);
            }
            
            size_t type_pos = item_tag.find("media-type=\"");
            if (type_pos != std::string::npos) {
                type_pos += 12;
                size_t type_end = item_tag.find("\"", type_pos);
                item.media_type = item_tag.substr(type_pos, type_end - type_pos);
            }
            
            if (!item.id.empty()) {
                manifest_[item.id] = item;
            }
            
            pos = end;
        }
    }
    
    size_t spine_start = opf.find("<spine");
    size_t spine_end = opf.find("</spine>", spine_start);
    
    if (spine_start != std::string::npos && spine_end != std::string::npos) {
        std::string spine_section = opf.substr(spine_start, spine_end - spine_start);
        
        size_t pos = 0;
        while ((pos = spine_section.find("<itemref", pos)) != std::string::npos) {
            size_t idref_pos = spine_section.find("idref=\"", pos);
            if (idref_pos == std::string::npos) break;
            
            idref_pos += 7;
            size_t idref_end = spine_section.find("\"", idref_pos);
            std::string idref = spine_section.substr(idref_pos, idref_end - idref_pos);
            
            if (manifest_.find(idref) != manifest_.end()) {
                spine_.push_back(manifest_[idref]);
            }
            
            pos = idref_end;
        }
    }
    
    LOG_INFO("Parsed manifest items:", manifest_.size());
    LOG_INFO("Parsed spine items:", spine_.size());
    
    return !spine_.empty();
}

std::string EpubParser::findNCXPath() {
    for (const auto& pair : manifest_) {
        const SpineItem& item = pair.second;
        if (item.media_type == "application/x-dtbncx+xml") {
            LOG_DEBUG("Found NCX in manifest:", item.href);
            return item.href;
        }
    }
    
    return "";
}

bool EpubParser::parseTOC() {
    LOG_DEBUG("Attempting to parse table of contents");
    
    std::string nav_path = content_dir_ + "nav.xhtml";
    std::string nav_content = zip_.extractTextFile(nav_path);
    
    if (nav_content.empty()) {
        nav_path = content_dir_ + "nav.html";
        nav_content = zip_.extractTextFile(nav_path);
    }
    
    if (!nav_content.empty()) {
        if (toc_parser_.parseNav(nav_content)) {
            LOG_INFO("TOC parsed from nav document");
            mapTOCToSpine();
            return true;
        }
    }
    
    std::string ncx_href = findNCXPath();
    std::vector<std::string> ncx_paths;
    
    if (!ncx_href.empty()) {
        ncx_paths.push_back(content_dir_ + ncx_href);
        ncx_paths.push_back(ncx_href);
    }
    
    ncx_paths.push_back(content_dir_ + "toc.ncx");
    ncx_paths.push_back("toc.ncx");
    
    for (const std::string& ncx_path : ncx_paths) {
        LOG_DEBUG("Trying NCX path:", ncx_path);
        std::string ncx_content = zip_.extractTextFile(ncx_path);
        
        if (!ncx_content.empty()) {
            if (toc_parser_.parseNCX(ncx_content)) {
                LOG_INFO("TOC parsed from NCX:", ncx_path);
                mapTOCToSpine();
                return true;
            }
        }
    }
    
    LOG_WARNING("No TOC found in standard locations");
    return false;
}

void EpubParser::generateFallbackTOC() {
    LOG_INFO("Generating fallback TOC from spine");
    
    toc_parser_.clear();
    
    for (size_t i = 0; i < spine_.size(); i++) {
        std::string path = spine_[i].href;
        
        while (path.find("../") == 0) {
            path = path.substr(3);
        }
        
        if (!content_dir_.empty() && path.find(content_dir_) != 0) {
            path = content_dir_ + path;
        }
        
        std::string html = zip_.extractTextFile(path);
        if (html.empty()) {
            path = spine_[i].href;
            while (path.find("../") == 0) {
                path = path.substr(3);
            }
            html = zip_.extractTextFile(path);
        }
        
        std::string title = tryExtractChapterTitle(html);
        if (title.empty()) {
            title = "Chapter " + std::to_string(i + 1);
        }
        
        TOCItem item;
        item.title = title;
        item.href = spine_[i].href;
        item.level = 0;
        item.spine_index = i;
        
        std::vector<TOCItem>& items = const_cast<std::vector<TOCItem>&>(toc_parser_.getItems());
        items.push_back(item);
    }
    
    LOG_INFO("Generated", spine_.size(), "fallback TOC items");
}

std::string EpubParser::tryExtractChapterTitle(const std::string& html) {
    const std::string title_tags[] = {"<title>", "<h1>", "<h2>", "<h3>"};
    
    for (const std::string& tag : title_tags) {
        size_t start = html.find(tag);
        if (start != std::string::npos) {
            start += tag.length();
            
            std::string end_tag = "</" + tag.substr(1);
            size_t end = html.find(end_tag, start);
            
            if (end != std::string::npos) {
                std::string title = html.substr(start, end - start);
                
                std::string clean;
                bool in_tag = false;
                for (char c : title) {
                    if (c == '<') in_tag = true;
                    else if (c == '>') in_tag = false;
                    else if (!in_tag) clean += c;
                }
                
                size_t first = clean.find_first_not_of(" \t\n\r");
                size_t last = clean.find_last_not_of(" \t\n\r");
                if (first != std::string::npos && last != std::string::npos) {
                    clean = clean.substr(first, last - first + 1);
                }
                
                if (!clean.empty() && clean.length() < 100) {
                    return clean;
                }
            }
        }
    }
    
    return "";
}

void EpubParser::mapTOCToSpine() {
    std::vector<TOCItem>& items = const_cast<std::vector<TOCItem>&>(toc_parser_.getItems());
    
    for (TOCItem& item : items) {
        item.spine_index = findChapterByHref(item.href);
    }
}

FormattedContent EpubParser::getChapterContent(size_t index) {
    if (index >= spine_.size()) return FormattedContent();
    
    std::string path = spine_[index].href;
    
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    if (!content_dir_.empty() && path.find(content_dir_) != 0) {
        path = content_dir_ + path;
    }
    
    LOG_DEBUG("Loading chapter content from path:", path);
    
    std::string html = zip_.extractTextFile(path);
    
    if (html.empty()) {
        LOG_WARNING("Failed to extract chapter, trying without content_dir");
        path = spine_[index].href;
        while (path.find("../") == 0) {
            path = path.substr(3);
        }
        html = zip_.extractTextFile(path);
    }
    
    return html_parser_.parse(html, &zip_, content_dir_);
}

std::string EpubParser::getChapterText(size_t index) {
    if (index >= spine_.size()) return "";
    
    std::string path = spine_[index].href;
    
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    if (!content_dir_.empty() && path.find(content_dir_) != 0) {
        path = content_dir_ + path;
    }
    
    LOG_DEBUG("Loading chapter from path:", path);
    
    std::string html = zip_.extractTextFile(path);
    
    if (html.empty()) {
        LOG_WARNING("Failed to extract chapter, trying without content_dir");
        path = spine_[index].href;
        while (path.find("../") == 0) {
            path = path.substr(3);
        }
        html = zip_.extractTextFile(path);
    }
    
    return extractTextFromHTML(html);
}

std::string EpubParser::findTagContent(const std::string& xml, const std::string& tag) {
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

std::string EpubParser::extractTextFromHTML(const std::string& html) {
    std::string text;
    bool in_tag = false;
    bool in_script = false;
    bool in_style = false;
    
    for (size_t i = 0; i < html.length(); i++) {
        if (html[i] == '<') {
            in_tag = true;
            
            if (i + 7 < html.length() && html.substr(i, 7) == "<script") in_script = true;
            if (i + 6 < html.length() && html.substr(i, 6) == "<style") in_style = true;
            if (i + 9 < html.length() && html.substr(i, 9) == "</script>") in_script = false;
            if (i + 8 < html.length() && html.substr(i, 8) == "</style>") in_style = false;
            
            if (i + 3 < html.length()) {
                std::string tag = html.substr(i, 3);
                if (tag == "<p>" || tag == "<br" || tag == "<di") {
                    text += "\n";
                }
            }
        } else if (html[i] == '>') {
            in_tag = false;
        } else if (!in_tag && !in_script && !in_style) {
            text += html[i];
        }
    }
    
    std::string result;
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '&') {
            if (text.substr(i, 6) == "&nbsp;") { result += ' '; i += 5; }
            else if (text.substr(i, 4) == "&lt;") { result += '<'; i += 3; }
            else if (text.substr(i, 4) == "&gt;") { result += '>'; i += 3; }
            else if (text.substr(i, 5) == "&amp;") { result += '&'; i += 4; }
            else if (text.substr(i, 6) == "&quot;") { result += '"'; i += 5; }
            else result += text[i];
        } else {
            result += text[i];
        }
    }
    
    return result;
}

size_t EpubParser::findChapterByHref(const std::string& href) const {
    for (size_t i = 0; i < spine_.size(); i++) {
        if (spine_[i].href == href) {
            return i;
        }
        
        size_t anchor = href.find('#');
        if (anchor != std::string::npos) {
            std::string href_no_anchor = href.substr(0, anchor);
            if (spine_[i].href == href_no_anchor) {
                return i;
            }
        }
    }
    
    return 0;
}

} // namespace epub