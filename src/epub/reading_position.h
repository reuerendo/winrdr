#pragma once

#include <string>
#include <unordered_map>

namespace epub {

struct BookPosition {
    size_t chapter_index;
    size_t page_index;
    std::string book_path;
    
    BookPosition() : chapter_index(0), page_index(0) {}
};

class ReadingPositionManager {
public:
    static ReadingPositionManager& instance();
    
    bool savePosition(const std::string& book_path, size_t chapter, size_t page);
    BookPosition loadPosition(const std::string& book_path);
    
    bool hasPosition(const std::string& book_path) const;
    void clearPosition(const std::string& book_path);
    void clearAll();

private:
    ReadingPositionManager();
    
    std::string getConfigPath() const;
    std::string getBookHash(const std::string& path) const;
    
    bool load();
    bool save();
    
    std::unordered_map<std::string, BookPosition> positions_;
    std::string config_file_;
};

} // namespace epub

#define POSITION_MGR epub::ReadingPositionManager::instance()