#include "image_cache.h"
#include "../utils/logger.h"

// stb_image for image decoding (single-header, available in PocketBook SDK)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#include "../../libs/stb_image.h"

namespace epub {

const size_t DEFAULT_MAX_CACHE_SIZE = 50 * 1024 * 1024; // 50 MB

ImageCache::ImageCache() 
    : max_cache_size_(DEFAULT_MAX_CACHE_SIZE)
    , current_cache_size_(0) 
{
}

ImageCache::~ImageCache() {
    clear();
}

bool ImageCache::loadImage(const std::string& id, const std::vector<char>& data) {
    if (data.empty()) {
        LOG_ERROR("Empty image data for:", id);
        return false;
    }
    
    LOG_DEBUG("Loading image:", id, "size:", data.size(), "bytes");
    
    ImageData img;
    unsigned char* pixels = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size()),
        &img.width,
        &img.height,
        &img.channels,
        0
    );
    
    if (!pixels) {
        LOG_ERROR("Failed to decode image:", id, "reason:", stbi_failure_reason());
        return false;
    }
    
    size_t pixel_count = img.width * img.height * img.channels;
    img.pixels.assign(pixels, pixels + pixel_count);
    stbi_image_free(pixels);
    
    LOG_INFO("Image loaded:", id, img.width, "x", img.height, "channels:", img.channels);
    
    size_t image_size = img.pixels.size();
    
    // Evict old images if needed
    while (max_cache_size_ > 0 && 
           current_cache_size_ + image_size > max_cache_size_ && 
           !cache_.empty()) {
        evictOldest();
    }
    
    current_cache_size_ += image_size;
    cache_[id] = std::move(img);
    access_order_.push_back(id);
    
    LOG_DEBUG("Cache size:", current_cache_size_ / 1024, "KB, images:", cache_.size());
    
    return true;
}

const ImageData* ImageCache::getImage(const std::string& id) const {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return &it->second;
    }
    return nullptr;
}

void ImageCache::clear() {
    cache_.clear();
    access_order_.clear();
    current_cache_size_ = 0;
    LOG_DEBUG("Image cache cleared");
}

size_t ImageCache::getCacheSize() const {
    return current_cache_size_;
}

void ImageCache::setMaxCacheSize(size_t bytes) {
    max_cache_size_ = bytes;
    
    while (max_cache_size_ > 0 && 
           current_cache_size_ > max_cache_size_ && 
           !cache_.empty()) {
        evictOldest();
    }
}

void ImageCache::evictOldest() {
    if (access_order_.empty()) return;
    
    std::string oldest_id = access_order_.front();
    access_order_.erase(access_order_.begin());
    
    auto it = cache_.find(oldest_id);
    if (it != cache_.end()) {
        size_t image_size = it->second.pixels.size();
        current_cache_size_ -= image_size;
        cache_.erase(it);
        LOG_DEBUG("Evicted image:", oldest_id);
    }
}

} // namespace epub