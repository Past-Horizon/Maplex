#pragma once

#include <Maplex/Config/Configuration.h>
#include <Winux/Winux.h>

#include <cstddef>
#include <string>
#include <vector>

namespace Maplex::Config
{
struct TriggerDefinition
{
    std::string Id;
    std::string Value;
};

struct ConfigGeneratorOptions
{
    std::string Alphabet;
    std::vector<TriggerDefinition> Triggers;
    std::size_t MappingCount = 4;
    std::size_t MaxTranspositions = 2;
    std::string CiphertextSymbols =
        "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
};

Configuration GenerateConfiguration(
    const ConfigGeneratorOptions& options,
    Winux::Contracts::ICrypto& crypto);
}