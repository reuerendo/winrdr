#include "logger.h"
#include <iostream>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

namespace logger {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& filepath, Level min_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.close();
    }
    
    file_.open(filepath, std::ios::out | std::ios::app);
    min_level_ = min_level;
    
    if (file_.is_open()) {
        log(Level::INFO, __FILE__, __LINE__, "Logger initialized: " + filepath);
    }
    
#ifdef _WIN32
    // Включаем UTF-8 в консоли Windows
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void Logger::setLevel(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::log(Level level, const char* file, int line, const std::string& message) {
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
    
    // Вывод в файл
    if (file_.is_open()) {
        file_ << log_line << std::endl;
        file_.flush();
    }
    
    // Вывод в консоль
    if (level >= Level::WARNING) {
        std::cerr << log_line << std::endl;
    } else {
        std::cout << log_line << std::endl;
    }
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO ";
        case Level::WARNING: return "WARN ";
        case Level::ERROR:   return "ERROR";
        default:             return "?????";
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace logger
