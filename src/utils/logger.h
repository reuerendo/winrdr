#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>

namespace logger {

enum class Level {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance();
    
    void init(const std::string& filepath, Level min_level = Level::INFO);
    void setLevel(Level level);
    
    void log(Level level, const char* file, int line, const std::string& message);
    
    template<typename... Args>
    void debug(const char* file, int line, Args&&... args) {
        log(Level::DEBUG, file, line, format(std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void info(const char* file, int line, Args&&... args) {
        log(Level::INFO, file, line, format(std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void warning(const char* file, int line, Args&&... args) {
        log(Level::WARNING, file, line, format(std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void error(const char* file, int line, Args&&... args) {
        log(Level::ERROR, file, line, format(std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    
    template<typename T>
    std::string format(T&& arg) {
        std::ostringstream oss;
        oss << arg;
        return oss.str();
    }
    
    template<typename T, typename... Args>
    std::string format(T&& first, Args&&... args) {
        std::ostringstream oss;
        oss << first;
        ((oss << " " << args), ...);
        return oss.str();
    }
    
    std::string levelToString(Level level);
    std::string getCurrentTime();
    
    std::ofstream file_;
    Level min_level_ = Level::INFO;
    std::mutex mutex_;
};

} // namespace logger

// Макросы для удобства
#define LOG_DEBUG(...) logger::Logger::instance().debug(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) logger::Logger::instance().info(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) logger::Logger::instance().warning(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger::Logger::instance().error(__FILE__, __LINE__, __VA_ARGS__)
