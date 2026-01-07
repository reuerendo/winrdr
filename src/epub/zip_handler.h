#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace epub {

class ZipHandler {
public:
    ZipHandler();
    ~ZipHandler();

    bool open(const std::string& filepath);
    void close();
    
    bool extractFile(const std::string& filename, std::vector<char>& data);
    std::string extractTextFile(const std::string& filename);
    
    bool fileExists(const std::string& filename);
    std::vector<std::string> listFiles();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace epub