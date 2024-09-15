#include "../source/editor/logging/arx_logger.hpp"

#include "../source/editor/arx_editor.hpp"

namespace arx {
    std::vector<LogEntry> Logger::logEntries;
    std::shared_ptr<Editor> Logger::editor = nullptr;

    void Logger::log(LogType level, const std::string& message) {
        std::string timestamp = getCurrentTimestamp();
        logEntries.push_back({level, message, timestamp});

        std::string fullMessage = "[" + timestamp + "] " + typeToString(level) + ": " + message;
        std::cout << fullMessage << std::endl;

        if (editor) {
            editor->addLogMessage(fullMessage);
        }
    }

    void Logger::shutdown() {
        writeLogFile();
        logEntries.clear();
        editor.reset();
    }

    std::string Logger::typeToString(LogType level) {
        switch (level) {
            case LogType::DEBUG: return "DEBUG";
            case LogType::INFO: return "INFO";
            case LogType::WARNING: return "WARNING";
            case LogType::ERROR: return "ERROR";
            case LogType::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    std::string Logger::getCurrentTimestamp() {
        time_t now = time(0);
        tm* timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(timestamp);
    }

    void Logger::writeLogFile() {
        std::ofstream logFile("log.txt", std::ios::out);
        if (logFile.is_open()) {
            for (const auto& entry : logEntries) {
                logFile << "[" << entry.timestamp << "] " << typeToString(entry.type) << ": " << entry.message << std::endl;
            }
            logFile.close();
        } else {
            std::cerr << "Error opening log file for writing." << std::endl;
        }
    }
}