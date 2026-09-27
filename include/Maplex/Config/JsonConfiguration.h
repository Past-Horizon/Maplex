#pragma once

#include <Maplex/Config/Configuration.h>

#include <string>

namespace Maplex::Config
{
Configuration LoadJsonConfiguration(const std::string& path);
}