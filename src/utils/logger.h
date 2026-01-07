#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>

enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
};

class Logger {
public:
    static Logger& instance();
    
    void init(const std::string& filepath, LogLevel min_level = LOG_LEVEL_INFO);
    void setLevel(LogLevel level);
    
    void log(LogLevel level, const char* file, int line, const std::string& message);
    
    template<typename T>
    void logDebug(const char* file, int line, const T& arg) {
        log(LOG_LEVEL_DEBUG, file, line, toString(arg));
    }
    
    template<typename T, typename... Args>
    void logDebug(const char* file, int line, const T& first, const Args&... args) {
        log(LOG_LEVEL_DEBUG, file, line, concat(first, args...));
    }
    
    template<typename T>
    void logInfo(const char* file, int line, const T& arg) {
        log(LOG_LEVEL_INFO, file, line, toString(arg));
    }
    
    template<typename T, typename... Args>
    void logInfo(const char* file, int line, const T& first, const Args&... args) {
        log(LOG_LEVEL_INFO, file, line, concat(first, args...));
    }
    
    template<typename T>
    void logWarning(const char* file, int line, const T& arg) {
        log(LOG_LEVEL_WARNING, file, line, toString(arg));
    }
    
    template<typename T, typename... Args>
    void logWarning(const char* file, int line, const T& first, const Args&... args) {
        log(LOG_LEVEL_WARNING, file, line, concat(first, args...));
    }
    
    template<typename T>
    void logError(const char* file, int line, const T& arg) {
        log(LOG_LEVEL_ERROR, file, line, toString(arg));
    }
    
    template<typename T, typename... Args>
    void logError(const char* file, int line, const T& first, const Args&... args) {
        log(LOG_LEVEL_ERROR, file, line, concat(first, args...));
    }

private:
    Logger() {}
    
    template<typename T>
    std::string toString(const T& arg) {
        std::ostringstream oss;
        oss << arg;
        return oss.str();
    }
    
    template<typename T>
    std::string concat(const T& arg) {
        return toString(arg);
    }
    
    template<typename T, typename... Args>
    std::string concat(const T& first, const Args&... args) {
        return toString(first) + " " + concat(args...);
    }
    
    std::string levelToString(LogLevel level);
    std::string getCurrentTime();
    
    std::ofstream file_;
    LogLevel min_level_;
    std::mutex mutex_;
};

#define LOG_DEBUG(...) Logger::instance().logDebug(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Logger::instance().logInfo(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Logger::instance().logWarning(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().logError(__FILE__, __LINE__, __VA_ARGS__)