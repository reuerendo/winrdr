#include "page_renderer.h"
#include "../utils/logger.h"
#include <gdiplus.h>

namespace {
    bool g_gdiplus_initialized = false;
    ULONG_PTR g_gdiplusToken = 0;
}

PageRenderer::PageRenderer()
    : container_(nullptr)
    , image_cache_(nullptr)
    , current_page_(0)
    , viewport_width_(800)
    , viewport_height_(600)
    , margin_(40)
    , total_height_(0)
    , memory_hdc_(nullptr)
    , memory_bitmap_(nullptr)
{
    if (!g_gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::Status status = Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
        if (status == Gdiplus::Ok) {
            g_gdiplus_initialized = true;
            LOG_INFO("GDI+ initialized successfully");
        } else {
            LOG_ERROR("Failed to initialize GDI+, status:", status);
        }
    }
    
    master_css_ = R"(
        body {
            font-family: Arial, sans-serif;
            font-size: 16px;
            line-height: 1.5;
            margin: 0;
            padding: 0;
        }
        
        p {
            margin: 1em 0;
            text-align: justify;
        }
        
        h1, h2, h3, h4, h5, h6 {
            margin: 1.5em 0 0.5em 0;
            font-weight: bold;
        }
        
        h1 { font-size: 2em; }
        h2 { font-size: 1.5em; }
        h3 { font-size: 1.17em; }
        h4 { font-size: 1em; }
        h5 { font-size: 0.83em; }
        h6 { font-size: 0.67em; }
        
        img {
            max-width: 100%;
            height: auto;
            display: block;
            margin: 1em auto;
        }
        
        blockquote {
            margin: 1em 40px;
            font-style: italic;
        }
        
        ul, ol {
            margin: 1em 0;
            padding-left: 40px;
        }
        
        li {
            margin: 0.5em 0;
        }
        
        pre {
            background-color: #f0f0f0;
            padding: 10px;
            overflow-x: auto;
            margin: 1em 0;
        }
        
        code {
            font-family: 'Courier New', monospace;
        }
        
        a {
            color: #0000EE;
            text-decoration: underline;
        }
        
        strong, b {
            font-weight: bold;
        }
        
        em, i {
            font-style: italic;
        }
        
        u {
            text-decoration: underline;
        }
        
        s, strike, del {
            text-decoration: line-through;
        }
        
        sub {
            vertical-align: sub;
            font-size: smaller;
        }
        
        sup {
            vertical-align: super;
            font-size: smaller;
        }
        
        hr {
            border: 0;
            border-top: 1px solid #ccc;
            margin: 2em 0;
        }
    )";
}

PageRenderer::~PageRenderer() {
    document_.reset();
    
    if (container_) {
        delete container_;
        container_ = nullptr;
    }
    
    if (memory_bitmap_) {
        DeleteObject(memory_bitmap_);
        memory_bitmap_ = nullptr;
    }
    
    if (memory_hdc_) {
        DeleteDC(memory_hdc_);
        memory_hdc_ = nullptr;
    }
}

void PageRenderer::createMemoryDC() {
    if (memory_bitmap_) {
        DeleteObject(memory_bitmap_);
        memory_bitmap_ = nullptr;
    }
    
    if (memory_hdc_) {
        DeleteDC(memory_hdc_);
        memory_hdc_ = nullptr;
    }
    
    HDC screen_hdc = GetDC(NULL);
    if (!screen_hdc) {
        LOG_ERROR("Failed to get screen DC");
        return;
    }
    
    memory_hdc_ = CreateCompatibleDC(screen_hdc);
    if (!memory_hdc_) {
        LOG_ERROR("Failed to create compatible DC");
        ReleaseDC(NULL, screen_hdc);
        return;
    }
    
    int content_width = viewport_width_ - 2 * margin_;
    int content_height = viewport_height_ - 2 * margin_;
    
    if (content_width <= 0 || content_height <= 0) {
        content_width = 600;
        content_height = 800;
    }
    
    memory_bitmap_ = CreateCompatibleBitmap(screen_hdc, content_width, content_height);
    if (!memory_bitmap_) {
        LOG_ERROR("Failed to create compatible bitmap");
        DeleteDC(memory_hdc_);
        memory_hdc_ = nullptr;
        ReleaseDC(NULL, screen_hdc);
        return;
    }
    
    SelectObject(memory_hdc_, memory_bitmap_);
    
    ReleaseDC(NULL, screen_hdc);
    
    LOG_DEBUG("Memory DC created successfully");
}

void PageRenderer::setContent(const std::string& html, const std::string& css) {
    LOG_DEBUG("Setting content, HTML length:", html.length(), "CSS length:", css.length());
    
    try {
        document_.reset();
        pages_.clear();
        current_page_ = 0;
        total_height_ = 0;
        
        if (html.empty()) {
            LOG_WARNING("Empty HTML content");
            return;
        }
        
        if (!memory_hdc_) {
            createMemoryDC();
        }
        
        if (!memory_hdc_) {
            LOG_ERROR("Failed to create memory DC");
            return;
        }
        
        if (!container_) {
            container_ = new LitehtmlContainer(memory_hdc_, image_cache_);
            LOG_DEBUG("Created new LitehtmlContainer");
        } else {
            container_->setHDC(memory_hdc_);
            LOG_DEBUG("Reused existing LitehtmlContainer");
        }
        
        int content_width = viewport_width_ - 2 * margin_;
        int content_height = viewport_height_ - 2 * margin_;
        
        if (content_width <= 0 || content_height <= 0) {
            LOG_ERROR("Invalid content dimensions:", content_width, "x", content_height);
            return;
        }
        
        container_->setViewportSize(content_width, content_height);
        
        std::string combined_css = master_css_;
        if (!css.empty()) {
            combined_css += "\n" + css;
        }
        
        LOG_DEBUG("Creating litehtml document...");
        
        document_ = litehtml::document::createFromString(
            html.c_str(), 
            container_, 
            combined_css.c_str()
        );
        
        if (!document_) {
            LOG_ERROR("Failed to create litehtml document");
            return;
        }
        
        LOG_INFO("Document created successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in setContent:", e.what());
        document_.reset();
    } catch (...) {
        LOG_ERROR("Unknown exception in setContent");
        document_.reset();
    }
}

void PageRenderer::setImageCache(epub::ImageCache* cache) {
    image_cache_ = cache;
    
    if (container_) {
        delete container_;
        container_ = nullptr;
        LOG_DEBUG("Container deleted, will be recreated on next setContent");
    }
}

void PageRenderer::setViewport(int width, int height, int margin) {
    viewport_width_ = width;
    viewport_height_ = height;
    margin_ = margin;
    
    pages_.clear();
    current_page_ = 0;
    
    if (memory_hdc_) {
        createMemoryDC();
    }
    
    if (container_) {
        int content_width = width - 2 * margin;
        int content_height = height - 2 * margin;
        
        if (content_width > 0 && content_height > 0) {
            container_->setViewportSize(content_width, content_height);
            
            if (memory_hdc_) {
                container_->setHDC(memory_hdc_);
            }
        }
    }
    
    LOG_DEBUG("Viewport set:", width, "x", height, "margin:", margin);
}

void PageRenderer::calculatePages(HDC hdc) {
    if (!document_) {
        LOG_WARNING("No document to calculate pages");
        return;
    }
    
    try {
        pages_.clear();
        
        int content_width = viewport_width_ - 2 * margin_;
        int content_height = viewport_height_ - 2 * margin_;
        
        if (content_width <= 0 || content_height <= 0) {
            LOG_ERROR("Invalid content dimensions for page calculation");
            return;
        }
        
        if (container_ && memory_hdc_) {
            container_->setHDC(memory_hdc_);
        }
        
        LOG_DEBUG("Rendering document with width:", content_width);
        
        document_->render(content_width);
        
        total_height_ = document_->height();
        
        LOG_DEBUG("Document rendered, total height:", total_height_);
        
        if (total_height_ <= 0) {
            LOG_WARNING("Document height is zero or negative, using default page");
            PageInfo page;
            page.scroll_offset = 0;
            pages_.push_back(page);
            return;
        }
        
        int num_pages = (total_height_ + content_height - 1) / content_height;
        
        if (num_pages <= 0) {
            num_pages = 1;
        }
        
        for (int i = 0; i < num_pages; i++) {
            PageInfo page;
            page.scroll_offset = i * content_height;
            pages_.push_back(page);
        }
        
        LOG_INFO("Pages calculated:", pages_.size());
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in calculatePages:", e.what());
        pages_.clear();
    } catch (...) {
        LOG_ERROR("Unknown exception in calculatePages");
        pages_.clear();
    }
}

bool PageRenderer::nextPage() {
    if (pages_.empty()) {
        if (memory_hdc_) {
            calculatePages(memory_hdc_);
        }
    }
    
    if (current_page_ + 1 < pages_.size()) {
        current_page_++;
        LOG_DEBUG("Next page:", current_page_ + 1, "/", pages_.size());
        return true;
    }
    return false;
}

bool PageRenderer::prevPage() {
    if (current_page_ > 0) {
        current_page_--;
        LOG_DEBUG("Prev page:", current_page_ + 1, "/", pages_.size());
        return true;
    }
    return false;
}

void PageRenderer::goToPage(size_t page) {
    if (pages_.empty()) {
        if (memory_hdc_) {
            calculatePages(memory_hdc_);
        }
    }
    
    if (page < pages_.size()) {
        current_page_ = page;
        LOG_DEBUG("Go to page:", current_page_ + 1, "/", pages_.size());
    }
}

void PageRenderer::render(HDC hdc) {
    if (!document_) {
        LOG_WARNING("No document to render");
        return;
    }
    
    if (!hdc) {
        LOG_ERROR("Invalid HDC for rendering");
        return;
    }
    
    try {
        if (pages_.empty()) {
            if (memory_hdc_) {
                calculatePages(memory_hdc_);
            } else {
                LOG_ERROR("No memory DC available");
                return;
            }
        }
        
        if (pages_.empty() || current_page_ >= pages_.size()) {
            LOG_WARNING("Nothing to render, pages:", pages_.size(), "current:", current_page_);
            return;
        }
        
        if (container_ && memory_hdc_) {
            container_->setHDC(memory_hdc_);
        }
        
        const PageInfo& page = pages_[current_page_];
        
        RECT clip_rect;
        clip_rect.left = margin_;
        clip_rect.top = margin_;
        clip_rect.right = viewport_width_ - margin_;
        clip_rect.bottom = viewport_height_ - margin_;
        
        HRGN clip_region = CreateRectRgn(clip_rect.left, clip_rect.top, clip_rect.right, clip_rect.bottom);
        if (!clip_region) {
            LOG_ERROR("Failed to create clip region");
            return;
        }
        
        int result = SelectClipRgn(hdc, clip_region);
        if (result == ERROR) {
            LOG_ERROR("Failed to select clip region");
            DeleteObject(clip_region);
            return;
        }
        
        litehtml::position clip_pos;
        clip_pos.x = margin_;
        clip_pos.y = margin_;
        clip_pos.width = viewport_width_ - 2 * margin_;
        clip_pos.height = viewport_height_ - 2 * margin_;
        
        document_->draw(
            (litehtml::uint_ptr)hdc, 
            margin_, 
            margin_ - page.scroll_offset, 
            &clip_pos
        );
        
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip_region);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in render:", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception in render");
    }
}