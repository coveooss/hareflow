#pragma once

#include <functional>
#include <shared_mutex>
#include <string>

#include <fmt/core.h>

#include "hareflow/export.h"

namespace hareflow {

enum class LoggingLevel { Debug, Info, Warn, Error };

using LoggingHandler = std::function<void(LoggingLevel, std::string)>;

class Logger
{
public:
    HAREFLOW_EXPORT static void set_level(LoggingLevel level);

    HAREFLOW_EXPORT static void disable();
    HAREFLOW_EXPORT static void to_stdout();
    HAREFLOW_EXPORT static void to_stderr();
    HAREFLOW_EXPORT static void to_custom_handler(LoggingHandler handler);

    template<typename... Args> static void debug(std::string_view format, Args&&... args)
    {
        s_logger.log(LoggingLevel::Debug, format, std::forward<Args>(args)...);
    }

    template<typename... Args> static void info(std::string_view format, Args&&... args)
    {
        s_logger.log(LoggingLevel::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args> static void warn(std::string_view format, Args&&... args)
    {
        s_logger.log(LoggingLevel::Warn, format, std::forward<Args>(args)...);
    }

    template<typename... Args> static void error(std::string_view format, Args&&... args)
    {
        s_logger.log(LoggingLevel::Error, format, std::forward<Args>(args)...);
    }

private:
    class InternalLogger
    {
    public:
        void set_level(LoggingLevel level)
        {
            std::unique_lock lock(m_mutex);
            m_level = level;
        }

        void set_handler(LoggingHandler handler)
        {
            std::unique_lock lock(m_mutex);
            m_handler = std::move(handler);
        }

        template<typename... Args> void log(LoggingLevel level, std::string_view format, Args&&... args)
        {
            std::shared_lock lock(m_mutex);
            if (m_handler && level >= m_level) {
                m_handler(level, fmt::format(format, std::forward<Args>(args)...));
            }
        }

    private:
        std::shared_mutex m_mutex   = {};
        LoggingLevel      m_level   = LoggingLevel::Warn;
        LoggingHandler    m_handler = {};
    };

    static InternalLogger s_logger;
};

}  // namespace hareflow