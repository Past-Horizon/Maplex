#pragma once

#include <string_view>

namespace Maplex::Utils
{
enum class Level
{
    Info,
    Debug,
    Trace
};

enum class Category
{
    Mapping,
    Trigger
};

void Log(Level level, Category category, std::string_view message);
}