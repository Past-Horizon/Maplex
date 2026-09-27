#pragma once

#include <Maplex/Config/Configuration.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Maplex::Cipher::Transformations
{
class SymbolPermutation
{
public:
    explicit SymbolPermutation(const Config::Configuration& configuration);

    void Reshuffle(std::uint64_t seed);
    Mappings::Symbol Forward(Mappings::Symbol symbol) const noexcept;
    Mappings::Symbol Reverse(Mappings::Symbol symbol) const noexcept;

private:
    std::vector<Mappings::Symbol> symbols_;
    std::unordered_map<Mappings::Symbol, Mappings::Symbol> forward_;
    std::unordered_map<Mappings::Symbol, Mappings::Symbol> inverse_;
};

Mappings::Symbol TransformSymbol(
    const Config::Configuration& configuration,
    std::string_view mappingId,
    Mappings::Symbol symbol,
    std::optional<std::uint64_t> subMappingSeed,
    const SymbolPermutation& permutation,
    bool encrypt);

void ApplyTranspositions(
    std::string& symbols,
    const std::vector<Config::PositionalTransposition>& transpositions,
    bool encrypt);
}