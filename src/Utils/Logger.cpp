#include <Maplex/Utils/Logger.h>

#include <iostream>
#include <mutex>

namespace Maplex::Utils
{
namespace
{
std::mutex logMutex;

std::string_view GetLevelName(Level level)
{
    switch (level)
    {
    case Level::Info:
        return "INFO";
    case Level::Debug:
        return "DEBUG";
    case Level::Trace:
        return "TRACE";
    }

    return "UNKNOWN";
}

std::string_view GetCategoryName(Category category)
{
    switch (category)
    {
    case Category::Mapping:
        return "Mapping";
    case Category::Trigger:
        return "Trigger";
    }

    return "Unknown";
}
}

void Log(Level level, Category category, std::string_view message)
{
    const std::lock_guard<std::mutex> lock(logMutex);
    std::clog << '[' << GetLevelName(level) << "] [" << GetCategoryName(category) << "] "
              << message << '\n';
}
}