#pragma once

#include "zip_handler.h"
#include "formatted_text.h"
#include "html_processor.h"
#include "image_cache.h"
#include "toc_parser.h"
#include <string>
#include <vector>
#include <unordered_map>

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
    const std::vector<TOCItem>& getTOC() const { return toc_parser_.getItems(); }
    
    FormattedContent getChapterContent(size_t index);
    std::string getChapterText(size_t index);
    size_t getChapterCount() const { return spine_.size(); }
    
    ImageCache& getImageCache() { return image_cache_; }
    const ImageCache& getImageCache() const { return image_cache_; }
    
    size_t findChapterByHref(const std::string& href) const;

private:
    void mapTOCToSpine();
    bool parseContainer();
    bool parseOPF();
    bool parseTOC();
    void generateFallbackTOC();
    
    std::string extractTextFromHTML(const std::string& html);
    std::string findTagContent(const std::string& xml, const std::string& tag);
    std::string findNCXPath();
    std::string tryExtractChapterTitle(const std::string& html);
    
    ZipHandler zip_;
    std::string opf_path_;
    std::string content_dir_;
    Metadata metadata_;
    std::vector<SpineItem> spine_;
    std::unordered_map<std::string, SpineItem> manifest_;
    
    HTMLProcessor html_processor_;
    ImageCache image_cache_;
    TOCParser toc_parser_;
};

} // namespace epub