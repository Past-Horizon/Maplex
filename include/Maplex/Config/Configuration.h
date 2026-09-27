#pragma once

#include <Maplex/Mappings/Mapping.h>
#include <Maplex/Triggers/TriggerProgression.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace Maplex::Config
{
struct Configuration
{
    std::string Alphabet;
    std::unordered_map<std::string, Mappings::Mapping> MappingSets;
    std::vector<Triggers::Trigger> OrderedTriggers;

    std::vector<std::string> Validate() const;
};
}