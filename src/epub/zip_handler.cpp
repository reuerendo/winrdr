#include "zip_handler.h"
#include "../utils/logger.h"
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_APIS
#include "../../libs/miniz.h"
#include <cstring>

namespace epub {

struct ZipHandler::Impl {
    mz_zip_archive archive;
    bool is_open = false;
};

ZipHandler::ZipHandler() : impl_(std::make_unique<Impl>()) {
    memset(&impl_->archive, 0, sizeof(mz_zip_archive));
}

ZipHandler::~ZipHandler() {
    close();
}

bool ZipHandler::open(const std::string& filepath) {
    close();
    
    LOG_INFO("Opening ZIP file:", filepath);
    impl_->is_open = mz_zip_reader_init_file(&impl_->archive, filepath.c_str(), 0);
    
    if (impl_->is_open) {
        int file_count = mz_zip_reader_get_num_files(&impl_->archive);
        LOG_INFO("ZIP opened successfully. Files count:", file_count);
    } else {
        LOG_ERROR("Failed to open ZIP file:", filepath);
    }
    
    return impl_->is_open;
}

void ZipHandler::close() {
    if (impl_->is_open) {
        mz_zip_reader_end(&impl_->archive);
        impl_->is_open = false;
    }
}

bool ZipHandler::extractFile(const std::string& filename, std::vector<char>& data) {
    if (!impl_->is_open) {
        LOG_ERROR("ZIP not opened, cannot extract:", filename);
        return false;
    }
    
    LOG_DEBUG("Extracting file:", filename);
    
    int file_index = mz_zip_reader_locate_file(&impl_->archive, filename.c_str(), nullptr, 0);
    if (file_index < 0) {
        LOG_WARNING("File not found in archive:", filename);
        return false;
    }
    
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&impl_->archive, file_index, &file_stat)) {
        LOG_ERROR("Failed to get file stat:", filename);
        return false;
    }
    
    LOG_DEBUG("File size:", file_stat.m_uncomp_size, "bytes");
    
    data.resize(file_stat.m_uncomp_size);
    bool result = mz_zip_reader_extract_to_mem(&impl_->archive, file_index, data.data(), data.size(), 0);
    
    if (result) {
        LOG_DEBUG("File extracted successfully:", filename);
    } else {
        LOG_ERROR("Failed to extract file:", filename);
    }
    
    return result;
}

std::string ZipHandler::extractTextFile(const std::string& filename) {
    std::vector<char> data;
    if (!extractFile(filename, data)) return "";
    
    return std::string(data.begin(), data.end());
}

bool ZipHandler::fileExists(const std::string& filename) {
    if (!impl_->is_open) return false;
    return mz_zip_reader_locate_file(&impl_->archive, filename.c_str(), nullptr, 0) >= 0;
}

std::vector<std::string> ZipHandler::listFiles() {
    std::vector<std::string> files;
    if (!impl_->is_open) return files;
    
    int num_files = mz_zip_reader_get_num_files(&impl_->archive);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (mz_zip_reader_file_stat(&impl_->archive, i, &file_stat)) {
            files.push_back(file_stat.m_filename);
        }
    }
    return files;
}

} // namespace epub
