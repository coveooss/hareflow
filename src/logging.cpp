#include "hareflow/logging.h"

#include <cstdio>

#include <memory>
#include <map>

namespace {

using namespace hareflow;
const std::map<LoggingLevel, std::string> LEVEL_TO_LABEL = {{LoggingLevel::Debug, "DEBUG"},
                                                            {LoggingLevel::Info, "INFO"},
                                                            {LoggingLevel::Warn, "WARN"},
                                                            {LoggingLevel::Error, "ERROR"}};

void print_to_file(FILE* file, std::mutex& mutex, LoggingLevel level, std::string message)
{
    std::string_view level_label;
    if (auto it = LEVEL_TO_LABEL.find(level); it != LEVEL_TO_LABEL.end()) {
        level_label = it->second;
    }
    std::unique_lock lock(mutex);
    fmt::print(file, "{}: {}\n", level_label, message);
}

}  // namespace

namespace hareflow {

Logger::InternalLogger Logger::s_logger;

void Logger::set_level(LoggingLevel level)
{
    s_logger.set_level(level);
}

void Logger::disable()
{
    s_logger.set_handler({});
}

void Logger::to_stdout()
{
    s_logger.set_handler([output_mutex = std::make_shared<std::mutex>()](LoggingLevel level, std::string message) {
        print_to_file(stdout, *output_mutex, level, std::move(message));
    });
}

void Logger::to_stderr()
{
    s_logger.set_handler([output_mutex = std::make_shared<std::mutex>()](LoggingLevel level, std::string message) {
        print_to_file(stderr, *output_mutex, level, std::move(message));
    });
}

void Logger::to_custom_handler(LoggingHandler handler)
{
    s_logger.set_handler(std::move(handler));
}

}  // namespace hareflow