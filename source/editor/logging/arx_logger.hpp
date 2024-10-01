#pragma once

#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>


namespace arx {

    class Editor;

    #define ARX_LOG_DEBUG(text, ...)    { arx::Logger::logf(arx::LogType::DEBUG,    std::string(__FUNCTION__) + ": " + std::string(text), ##__VA_ARGS__); }
    #define ARX_LOG_INFO(text, ...)     { arx::Logger::logf(arx::LogType::INFO,     std::string(__FUNCTION__) + ": " + std::string(text), ##__VA_ARGS__); }
    #define ARX_LOG_WARNING(text, ...)  { arx::Logger::logf(arx::LogType::WARNING,  std::string(__FUNCTION__) + ": " + std::string(text), ##__VA_ARGS__); }
    #define ARX_LOG_ERROR(text, ...)    { arx::Logger::logf(arx::LogType::ERROR,    std::string(__FUNCTION__) + ": " + std::string(text), ##__VA_ARGS__); }
    #define ARX_LOG_CRITICAL(text, ...) { arx::Logger::logf(arx::LogType::CRITICAL, std::string(__FUNCTION__) + ": " + std::string(text), ##__VA_ARGS__); }

    enum class LogType { 
        DEBUG, 
        INFO, 
        WARNING, 
        ERROR, 
        CRITICAL 
    };

    struct LogEntry {
        LogType type;
        std::string message;
        std::string timestamp;
    };

    class Logger {
    public:
        static void log(LogType level, const std::string& message);
        static void shutdown();

        template<typename... Args>
        static void logf(LogType level, const std::string& format, Args... args) {
            std::ostringstream oss;
            formatString(oss, format, args...);
            log(level, oss.str());
        }

        static void setEditor(std::shared_ptr<Editor> editor) { Logger::editor = editor; }

    private:
        Logger() = delete;
        ~Logger() = delete;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        template<typename T, typename... Args>
        static void formatString(std::ostringstream& oss, const std::string& format, T value, Args... args) {
            size_t pos = format.find("{}");
            if (pos != std::string::npos) {
                oss << format.substr(0, pos) << value;
                formatString(oss, format.substr(pos + 2), args...);
            } else {
                oss << format;
            }
        }

        static void formatString(std::ostringstream& oss, const std::string& format) {
            oss << format;
        }

        static std::shared_ptr<Editor> editor;
        static std::vector<LogEntry> logEntries;
        static std::string typeToString(LogType level);
        static std::string getCurrentTimestamp();
        static void writeLogFile();
    };
}