#pragma once

#include <Maplex/Mappings/Mapping.h>
#include <Maplex/Triggers/TriggerProgression.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Maplex::Config
{
enum class TranspositionType
{
    Diagonal,
    ReversedDiagonal
};

struct PositionalTransposition
{
    TranspositionType Type;
    std::size_t Width;
    std::size_t Height;
};

struct Configuration
{
    std::string Alphabet;
    std::unordered_map<std::string, Mappings::Mapping> MappingSets;
    std::vector<Triggers::Trigger> OrderedTriggers;
    std::vector<PositionalTransposition> Transpositions;
    std::optional<std::uint64_t> SymbolShuffleSeed;

    std::vector<std::string> Validate() const;
};
}