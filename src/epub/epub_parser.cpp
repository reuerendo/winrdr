#include "epub_parser.h"
#include "../utils/logger.h"
#include <sstream>
#include <algorithm>

namespace epub {

EpubParser::EpubParser() {}
EpubParser::~EpubParser() { close(); }

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
    
    LOG_INFO("EPUB opened successfully");
    LOG_INFO("Title:", metadata_.title);
    LOG_INFO("Author:", metadata_.author);
    LOG_INFO("Chapters:", spine_.size());
    
    return true;
}

void EpubParser::close() {
    zip_.close();
    spine_.clear();
}

bool EpubParser::parseContainer() {
    LOG_DEBUG("Parsing container.xml");
    
    std::string container = zip_.extractTextFile("META-INF/container.xml");
    if (container.empty()) {
        LOG_ERROR("container.xml not found or empty");
        return false;
    }
    
    // Простой поиск full-path в container.xml
    size_t pos = container.find("full-path=\"");
    if (pos == std::string::npos) {
        LOG_ERROR("full-path attribute not found in container.xml");
        return false;
    }
    
    pos += 11; // длина "full-path=\""
    size_t end = container.find("\"", pos);
    if (end == std::string::npos) {
        LOG_ERROR("Malformed full-path attribute");
        return false;
    }
    
    opf_path_ = container.substr(pos, end - pos);
    LOG_INFO("OPF path found:", opf_path_);
    
    // Извлекаем директорию
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
    
    // Парсим метаданные
    metadata_.title = findTagContent(opf, "dc:title");
    metadata_.author = findTagContent(opf, "dc:creator");
    metadata_.language = findTagContent(opf, "dc:language");
    
    LOG_DEBUG("Parsed metadata - Title:", metadata_.title);
    LOG_DEBUG("Parsed metadata - Author:", metadata_.author);
    LOG_DEBUG("Parsed metadata - Language:", metadata_.language);
    
    // Парсим манифест
    std::unordered_map<std::string, SpineItem> manifest;
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
            
            // id
            size_t id_pos = item_tag.find("id=\"");
            if (id_pos != std::string::npos) {
                id_pos += 4;
                size_t id_end = item_tag.find("\"", id_pos);
                item.id = item_tag.substr(id_pos, id_end - id_pos);
            }
            
            // href
            size_t href_pos = item_tag.find("href=\"");
            if (href_pos != std::string::npos) {
                href_pos += 6;
                size_t href_end = item_tag.find("\"", href_pos);
                item.href = item_tag.substr(href_pos, href_end - href_pos);
            }
            
            // media-type
            size_t type_pos = item_tag.find("media-type=\"");
            if (type_pos != std::string::npos) {
                type_pos += 12;
                size_t type_end = item_tag.find("\"", type_pos);
                item.media_type = item_tag.substr(type_pos, type_end - type_pos);
            }
            
            manifest[item.id] = item;
            pos = end;
        }
    }
    
    // Парсим spine
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
            
            if (manifest.find(idref) != manifest.end()) {
                spine_.push_back(manifest[idref]);
            }
            
            pos = idref_end;
        }
    }
    
    LOG_INFO("Parsed manifest items:", manifest.size());
    LOG_INFO("Parsed spine items:", spine_.size());
    
    if (spine_.empty()) {
        LOG_ERROR("Spine is empty");
    }
    
    return !spine_.empty();
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
            
            // Проверяем script/style теги
            if (i + 7 < html.length() && html.substr(i, 7) == "<script") in_script = true;
            if (i + 6 < html.length() && html.substr(i, 6) == "<style") in_style = true;
            if (i + 9 < html.length() && html.substr(i, 9) == "</script>") in_script = false;
            if (i + 8 < html.length() && html.substr(i, 8) == "</style>") in_style = false;
            
            // Добавляем пробелы для некоторых тегов
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
    
    // Декодируем HTML entities
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

std::string EpubParser::getChapterText(size_t index) {
    if (index >= spine_.size()) return "";
    
    std::string path = spine_[index].href;
    
    // Нормализуем путь - убираем ../ в начале
    while (path.find("../") == 0) {
        path = path.substr(3);
    }
    
    // Добавляем content_dir только если путь относительный
    if (!content_dir_.empty() && path.find(content_dir_) != 0) {
        path = content_dir_ + path;
    }
    
    LOG_DEBUG("Loading chapter from path:", path);
    
    std::string html = zip_.extractTextFile(path);
    
    if (html.empty()) {
        LOG_WARNING("Failed to extract chapter, trying without content_dir");
        // Пробуем без content_dir
        path = spine_[index].href;
        while (path.find("../") == 0) {
            path = path.substr(3);
        }
        html = zip_.extractTextFile(path);
    }
    
    return extractTextFromHTML(html);
}

} // namespace epub