#pragma once

#include <memory>
#include <string>
#include <vector>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace CipherLog
{

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

class Logger
{
   public:
    /**
     * @brief Get the singleton instance of the logger
     * @return Reference to the logger instance
     */
    static Logger& getInstance();

    /**
     * @brief Initialize the logger with specified configuration
     * @param loggerName Name of the logger
     * @param level Default log level
     * @param enableConsole Whether to log to console (stdout)
     * @param enableStderr Whether to log to stderr
     * @param enableFile Whether to log to file
     * @param filename Path to log file (if file logging is enabled)
     * @param maxFileSize Maximum size of each log file in bytes
     * @param maxFiles Maximum number of rotated log files to keep
     */
    void init(const std::string& loggerName,
              LogLevel level              = LogLevel::Info,
              bool enableConsole          = true,
              bool enableStderr           = false,
              bool enableFile             = false,
              const std::string& filename = "logs/ciphertrader.log",
              size_t maxFileSize          = 5 * 1024 * 1024,
              size_t maxFiles             = 3);

    /**
     * @brief Set the global log level
     * @param level New log level
     */
    void setLevel(LogLevel level);

    /**
     * @brief Get the current log level
     * @return Current log level
     */
    LogLevel getLevel() const;

    /**
     * @brief Log a trace message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void trace(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->trace(msg, args...);
    }

    /**
     * @brief Log a debug message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void debug(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->debug(msg, args...);
    }

    /**
     * @brief Log an info message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void info(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->info(msg, args...);
    }

    /**
     * @brief Log a warning message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void warn(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->warn(msg, args...);
    }

    /**
     * @brief Log an error message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void error(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->error(msg, args...);
    }

    /**
     * @brief Log a critical message
     * @param msg Message to log
     * @param args Format arguments
     */
    template < typename... Args >
    void critical(const std::string& msg, const Args&... args)
    {
        if (logger_)
            logger_->critical(msg, args...);
    }

   private:
    Logger()                         = default;
    ~Logger()                        = default;
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    std::shared_ptr< spdlog::logger > logger_;
    LogLevel currentLevel_ = LogLevel::Info;

    /**
     * @brief Convert LogLevel enum to spdlog::level::level_enum
     * @param level LogLevel to convert
     * @return Equivalent spdlog level
     */
    spdlog::level::level_enum toSpdLogLevel(LogLevel level) const;

    /**
     * @brief Convert spdlog::level::level_enum to LogLevel
     * @param level spdlog level to convert
     * @return Equivalent LogLevel
     */
    LogLevel fromSpdLogLevel(spdlog::level::level_enum level) const;
};

// Convenience macro for accessing the logger
#define LOG Logger::getInstance()

} // namespace CipherLog
