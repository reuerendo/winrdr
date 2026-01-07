#include "logger.h"
#include <iostream>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& filepath, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.close();
    }
    
    file_.open(filepath, std::ios::out | std::ios::app);
    min_level_ = min_level;
    
    if (file_.is_open()) {
        log(LOG_LEVEL_INFO, __FILE__, __LINE__, "Logger initialized: " + filepath);
    }
    
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < min_level_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filename = file;
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
    }
    
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "] "
        << "[" << levelToString(level) << "] "
        << "[" << filename << ":" << line << "] "
        << message;
    
    std::string log_line = oss.str();
    
    if (file_.is_open()) {
        file_ << log_line << std::endl;
        file_.flush();
    }
    
    if (level >= LOG_LEVEL_WARNING) {
        std::cerr << log_line << std::endl;
    } else {
        std::cout << log_line << std::endl;
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO ";
        case LOG_LEVEL_WARNING: return "WARN ";
        case LOG_LEVEL_ERROR:   return "ERROR";
        default:                return "?????";
    }
}

std::string Logger::getCurrentTime() {
    time_t now = time(NULL);
    struct tm* tm_ptr = localtime(&now);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_ptr);
    return std::string(buffer);
}