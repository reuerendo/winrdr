#include "page_renderer.h"
#include "../utils/logger.h"

PageRenderer::PageRenderer()
    : image_cache_(nullptr)
    , current_page_(0)
    , viewport_width_(800)
    , viewport_height_(600)
    , margin_(40)
    , total_height_(0)
{
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
}

void PageRenderer::setContent(const std::string& html, const std::string& css) {
    LOG_DEBUG("Setting content, HTML length:", html.length(), "CSS length:", css.length());
    
    document_.reset();
    pages_.clear();
    current_page_ = 0;
    total_height_ = 0;
    
    if (html.empty()) {
        LOG_WARNING("Empty HTML content");
        return;
    }
    
    HDC hdc = GetDC(NULL);
    
    container_ = std::make_unique<LitehtmlContainer>(hdc, image_cache_);
    
    litehtml::context context;
    
    std::string combined_css = master_css_;
    if (!css.empty()) {
        combined_css += "\n" + css;
    }
    
    context.load_master_stylesheet(combined_css.c_str());
    
    document_ = litehtml::document::createFromString(html.c_str(), container_.get(), &context);
    
    ReleaseDC(NULL, hdc);
    
    if (!document_) {
        LOG_ERROR("Failed to create litehtml document");
        return;
    }
    
    LOG_INFO("Document created successfully");
}

void PageRenderer::setImageCache(epub::ImageCache* cache) {
    image_cache_ = cache;
}

void PageRenderer::setViewport(int width, int height, int margin) {
    viewport_width_ = width;
    viewport_height_ = height;
    margin_ = margin;
    
    pages_.clear();
    current_page_ = 0;
    
    LOG_DEBUG("Viewport set:", width, "x", height, "margin:", margin);
}

void PageRenderer::calculatePages(HDC hdc) {
    if (!document_) {
        LOG_WARNING("No document to calculate pages");
        return;
    }
    
    pages_.clear();
    
    int content_width = viewport_width_ - 2 * margin_;
    int content_height = viewport_height_ - 2 * margin_;
    
    container_->setHDC(hdc);
    
    document_->render(content_width);
    
    total_height_ = document_->height();
    
    LOG_DEBUG("Document rendered, total height:", total_height_);
    
    if (total_height_ == 0) {
        LOG_WARNING("Document height is zero");
        return;
    }
    
    int num_pages = (total_height_ + content_height - 1) / content_height;
    
    for (int i = 0; i < num_pages; i++) {
        PageInfo page;
        page.scroll_offset = i * content_height;
        pages_.push_back(page);
    }
    
    LOG_INFO("Pages calculated:", pages_.size());
}

bool PageRenderer::nextPage() {
    if (pages_.empty()) {
        HDC hdc = GetDC(NULL);
        calculatePages(hdc);
        ReleaseDC(NULL, hdc);
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
        HDC hdc = GetDC(NULL);
        calculatePages(hdc);
        ReleaseDC(NULL, hdc);
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
    
    if (pages_.empty()) {
        calculatePages(hdc);
    }
    
    if (pages_.empty() || current_page_ >= pages_.size()) {
        LOG_WARNING("Nothing to render");
        return;
    }
    
    container_->setHDC(hdc);
    
    const PageInfo& page = pages_[current_page_];
    
    RECT clip_rect;
    clip_rect.left = margin_;
    clip_rect.top = margin_;
    clip_rect.right = viewport_width_ - margin_;
    clip_rect.bottom = viewport_height_ - margin_;
    
    HRGN clip_region = CreateRectRgn(clip_rect.left, clip_rect.top, clip_rect.right, clip_rect.bottom);
    SelectClipRgn(hdc, clip_region);
    
    litehtml::position clip_pos;
    clip_pos.x = margin_;
    clip_pos.y = margin_;
    clip_pos.width = viewport_width_ - 2 * margin_;
    clip_pos.height = viewport_height_ - 2 * margin_;
    
    litehtml::position draw_pos;
    draw_pos.x = margin_;
    draw_pos.y = margin_ - page.scroll_offset;
    draw_pos.width = viewport_width_ - 2 * margin_;
    draw_pos.height = total_height_;
    
    document_->draw((litehtml::uint_ptr)hdc, margin_, margin_ - page.scroll_offset, &clip_pos);
    
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip_region);
}