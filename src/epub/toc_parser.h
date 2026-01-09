#pragma once

#include <string>
#include <vector>

namespace epub {

struct TOCItem {
    std::string title;
    std::string href;
    int level;
    size_t spine_index;
    
    TOCItem() : level(0), spine_index(0) {}
};

class TOCParser {
public:
    TOCParser();
    
    bool parseNCX(const std::string& ncx_content);
    bool parseNav(const std::string& nav_html);
    
    const std::vector<TOCItem>& getItems() const { return items_; }
    void clear();
    
    size_t findChapterIndex(const std::string& href) const;

private:
    void parseNavPoint(const std::string& xml, size_t& pos, int level);
    void parseNavList(const std::string& html, size_t& pos, int level);
    
    std::string extractText(const std::string& xml, const std::string& tag) const;
    std::string extractAttribute(const std::string& tag_content, const std::string& attr) const;
    std::string normalizeHref(const std::string& href) const;
    
    std::vector<TOCItem> items_;
};

} // namespace epub