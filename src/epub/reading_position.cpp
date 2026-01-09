#include "reading_position.h"
#include "../utils/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace epub {

ReadingPositionManager& ReadingPositionManager::instance() {
    static ReadingPositionManager mgr;
    return mgr;
}

ReadingPositionManager::ReadingPositionManager() {
    config_file_ = getConfigPath();
    load();
}

std::string ReadingPositionManager::getConfigPath() const {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        char utf8_path[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path, sizeof(utf8_path), NULL, NULL);
        return std::string(utf8_path) + "\\EpubReader\\positions.txt";
    }
    return "positions.txt";
#else
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    return std::string(home) + "/.epub_reader_positions";
#endif
}

std::string ReadingPositionManager::getBookHash(const std::string& path) const {
    // Simple hash based on path
    size_t hash = 0;
    for (char c : path) {
        hash = hash * 31 + static_cast<unsigned char>(c);
    }
    
    std::ostringstream oss;
    oss << hash;
    return oss.str();
}

bool ReadingPositionManager::savePosition(const std::string& book_path, 
                                         size_t chapter, size_t page) {
    std::string hash = getBookHash(book_path);
    
    BookPosition pos;
    pos.book_path = book_path;
    pos.chapter_index = chapter;
    pos.page_index = page;
    
    positions_[hash] = pos;
    
    bool result = save();
    
    if (result) {
        LOG_DEBUG("Position saved:", book_path, "chapter:", chapter, "page:", page);
    } else {
        LOG_ERROR("Failed to save position");
    }
    
    return result;
}

BookPosition ReadingPositionManager::loadPosition(const std::string& book_path) {
    std::string hash = getBookHash(book_path);
    
    auto it = positions_.find(hash);
    if (it != positions_.end()) {
        LOG_DEBUG("Position loaded:", book_path, "chapter:", it->second.chapter_index, 
                 "page:", it->second.page_index);
        return it->second;
    }
    
    return BookPosition();
}

bool ReadingPositionManager::hasPosition(const std::string& book_path) const {
    return positions_.find(getBookHash(book_path)) != positions_.end();
}

void ReadingPositionManager::clearPosition(const std::string& book_path) {
    positions_.erase(getBookHash(book_path));
    save();
}

void ReadingPositionManager::clearAll() {
    positions_.clear();
    save();
}

bool ReadingPositionManager::load() {
    std::ifstream file(config_file_);
    if (!file.is_open()) {
        LOG_DEBUG("No saved positions file found");
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string hash, path;
        size_t chapter, page;
        
        if (std::getline(iss, hash, '|') &&
            std::getline(iss, path, '|') &&
            (iss >> chapter) && iss.ignore() &&
            (iss >> page)) {
            
            BookPosition pos;
            pos.book_path = path;
            pos.chapter_index = chapter;
            pos.page_index = page;
            
            positions_[hash] = pos;
        }
    }
    
    LOG_INFO("Loaded positions for", positions_.size(), "books");
    return true;
}

bool ReadingPositionManager::save() {
    // Ensure directory exists
#ifdef _WIN32
    size_t slash = config_file_.find_last_of("\\/");
    if (slash != std::string::npos) {
        std::string dir = config_file_.substr(0, slash);
        CreateDirectoryA(dir.c_str(), NULL);
    }
#endif
    
    std::ofstream file(config_file_);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open positions file for writing:", config_file_);
        return false;
    }
    
    for (const auto& pair : positions_) {
        const BookPosition& pos = pair.second;
        file << pair.first << "|"
             << pos.book_path << "|"
             << pos.chapter_index << "|"
             << pos.page_index << "\n";
    }
    
    return true;
}

} // namespace epub