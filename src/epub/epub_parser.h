#pragma once

#include "zip_handler.h"
#include <string>
#include <vector>

namespace epub {

struct SpineItem {
    std::string id;
    std::string href;
    std::string media_type;
};

struct Metadata {
    std::string title;
    std::string author;
    std::string language;
};

class EpubParser {
public:
    EpubParser();
    ~EpubParser();

    bool open(const std::string& filepath);
    void close();
    
    const Metadata& getMetadata() const { return metadata_; }
    const std::vector<SpineItem>& getSpine() const { return spine_; }
    
    std::string getChapterText(size_t index);
    size_t getChapterCount() const { return spine_.size(); }

private:
    bool parseContainer();
    bool parseOPF();
    std::string extractTextFromHTML(const std::string& html);
    std::string findTagContent(const std::string& xml, const std::string& tag);
    
    ZipHandler zip_;
    std::string opf_path_;
    std::string content_dir_;
    Metadata metadata_;
    std::vector<SpineItem> spine_;
};

} // namespace epub
