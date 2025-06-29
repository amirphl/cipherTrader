#include "Logger.hpp"

ct::logger::Logger& ct::logger::Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void ct::logger::Logger::init(const std::string& loggerName,
                              LogLevel level,
                              bool enableConsole,
                              bool enableStderr,
                              bool enableFile,
                              const std::string& filename,
                              size_t maxFileSize,
                              size_t maxFiles)
{
    try
    {
        // Create a vector of sinks
        std::vector< spdlog::sink_ptr > sinks;

        // Add console sink if enabled
        if (enableConsole)
        {
            auto console_sink = std::make_shared< spdlog::sinks::stdout_color_sink_mt >();
            console_sink->set_level(toSpdLogLevel(level));
            sinks.push_back(console_sink);
        }

        // Add stderr sink if enabled
        if (enableStderr)
        {
            auto stderr_sink = std::make_shared< spdlog::sinks::stderr_color_sink_mt >();
            stderr_sink->set_level(toSpdLogLevel(level));
            sinks.push_back(stderr_sink);
        }

        // Add file sink if enabled
        if (enableFile)
        {
            // Create directory if it doesn't exist
            std::filesystem::path filePath(filename);
            std::filesystem::create_directories(filePath.parent_path());

            auto file_sink = std::make_shared< spdlog::sinks::rotating_file_sink_mt >(filename, maxFileSize, maxFiles);
            file_sink->set_level(toSpdLogLevel(level));
            sinks.push_back(file_sink);
        }

        // Create and register logger
        logger_ = std::make_shared< spdlog::logger >(loggerName, sinks.begin(), sinks.end());
        logger_->set_level(toSpdLogLevel(level));

        // Set pattern: [timestamp] [level] [thread_id] [logger_name] message
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%n] %v");

        // Register as default logger
        spdlog::register_logger(logger_);

        // Store current level
        currentLevel_ = level;

        // Log initialization
        logger_->info("Logger initialized");
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
    }
}

void ct::logger::Logger::setLevel(LogLevel level)
{
    if (logger_)
    {
        logger_->set_level(toSpdLogLevel(level));
        currentLevel_ = level;
    }
}

ct::logger::LogLevel ct::logger::Logger::getLevel() const
{
    return currentLevel_;
}

spdlog::level::level_enum ct::logger::Logger::toSpdLogLevel(LogLevel level) const
{
    switch (level)
    {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warn:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Critical:
            return spdlog::level::critical;
        case LogLevel::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

ct::logger::LogLevel ct::logger::Logger::fromSpdLogLevel(spdlog::level::level_enum level) const
{
    switch (level)
    {
        case spdlog::level::trace:
            return LogLevel::Trace;
        case spdlog::level::debug:
            return LogLevel::Debug;
        case spdlog::level::info:
            return LogLevel::Info;
        case spdlog::level::warn:
            return LogLevel::Warn;
        case spdlog::level::err:
            return LogLevel::Error;
        case spdlog::level::critical:
            return LogLevel::Critical;
        case spdlog::level::off:
            return LogLevel::Off;
        default:
            return LogLevel::Info;
    }
}


ct::logger::LogsState& ct::logger::LogsState::getInstance()
{
    static ct::logger::LogsState instance;
    return instance;
}

void ct::logger::LogsState::addError(const std::string& message)
{
    errors_.push_back(message);
}

void ct::logger::LogsState::addInfo(const std::string& message)
{
    info_.push_back(message);
}

const std::vector< std::string >& ct::logger::LogsState::getErrors() const
{
    return errors_;
}

const std::vector< std::string >& ct::logger::LogsState::getInfo() const
{
    return info_;
}

void ct::logger::LogsState::clear()
{
    errors_.clear();
    info_.clear();
}
