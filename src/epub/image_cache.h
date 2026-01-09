#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace epub {

struct ImageData {
    std::vector<unsigned char> pixels;
    int width;
    int height;
    int channels;
    
    ImageData() : width(0), height(0), channels(0) {}
};

class ImageCache {
public:
    ImageCache();
    ~ImageCache();
    
    // Load image from memory (JPEG, PNG, GIF, BMP)
    bool loadImage(const std::string& id, const std::vector<char>& data);
    
    // Get cached image
    const ImageData* getImage(const std::string& id) const;
    
    // Clear cache
    void clear();
    
    // Get cache size in bytes
    size_t getCacheSize() const;
    
    // Set max cache size (0 = unlimited)
    void setMaxCacheSize(size_t bytes);

private:
    void evictOldest();
    
    std::unordered_map<std::string, ImageData> cache_;
    std::vector<std::string> access_order_;
    size_t max_cache_size_;
    size_t current_cache_size_;
};

} // namespace epub