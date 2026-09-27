#include "Transformations.h"

#include <Maplex/Randomization/SeededShuffle.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Maplex::Cipher::Transformations
{
namespace
{
constexpr unsigned char kPassthroughTag = 0x80;
constexpr Mappings::Symbol kPassthroughSeedMarker = '\0';

unsigned char DerivePassthroughOffset(
    std::string_view mappingId,
    std::optional<std::uint64_t> subMappingSeed)
{
    const std::uint64_t derived = Randomization::DeriveSubMappingSeed(
        subMappingSeed.value_or(0), mappingId, kPassthroughSeedMarker);
    return static_cast<unsigned char>(derived) & 0x7F;
}

std::vector<std::size_t> CreateDiagonalOrder(
    std::size_t blockLength,
    const Config::PositionalTransposition& transposition)
{
    std::vector<std::size_t> order;
    order.reserve(blockLength);

    const std::size_t lastRow = std::min(transposition.Height - 1, (blockLength - 1) / transposition.Width);
    const std::size_t lastColumn = std::min(
        transposition.Width - 1,
        blockLength - 1 - lastRow * transposition.Width);
    std::size_t lastDiagonal = lastRow + lastColumn;
    if (lastRow > 0)
    {
        lastDiagonal = std::max(lastDiagonal, lastRow + transposition.Width - 2);
    }

    for (std::size_t diagonal = 0; diagonal <= lastDiagonal; ++diagonal)
    {
        const std::size_t firstRow = diagonal >= transposition.Width
            ? diagonal - transposition.Width + 1
            : 0;
        const std::size_t lastRow = std::min(diagonal, transposition.Height - 1);

        if (transposition.Type == Config::TranspositionType::Diagonal)
        {
            for (std::size_t row = lastRow + 1; row > firstRow; --row)
            {
                const std::size_t sourceRow = row - 1;
                const std::size_t column = diagonal - sourceRow;
                const std::size_t sourceIndex = sourceRow * transposition.Width + column;
                if (sourceIndex < blockLength)
                {
                    order.push_back(sourceIndex);
                }
            }
        }
        else
        {
            for (std::size_t row = firstRow; row <= lastRow; ++row)
            {
                const std::size_t column = diagonal - row;
                const std::size_t sourceIndex = row * transposition.Width + column;
                if (sourceIndex < blockLength)
                {
                    order.push_back(sourceIndex);
                }
            }
        }
    }

    return order;
}

void ApplyTransposition(
    std::string& symbols,
    const Config::PositionalTransposition& transposition,
    bool encrypt)
{
    const std::size_t blockCapacity = transposition.Width * transposition.Height;
    for (std::size_t blockStart = 0; blockStart < symbols.size();)
    {
        const std::size_t blockLength = std::min(blockCapacity, symbols.size() - blockStart);
        const std::string_view block(symbols.data() + blockStart, blockLength);
        const std::vector<std::size_t> order = CreateDiagonalOrder(blockLength, transposition);
        std::string transformed(blockLength, '\0');

        for (std::size_t outputIndex = 0; outputIndex < order.size(); ++outputIndex)
        {
            if (encrypt)
            {
                transformed[outputIndex] = block[order[outputIndex]];
            }
            else
            {
                transformed[order[outputIndex]] = block[outputIndex];
            }
        }

        symbols.replace(blockStart, blockLength, transformed);
        blockStart += blockLength;
    }
}
}

SymbolPermutation::SymbolPermutation(const Config::Configuration& configuration)
{
    std::unordered_set<Mappings::Symbol> uniqueSymbols;
    for (const auto& [mappingId, mapping] : configuration.MappingSets)
    {
        static_cast<void>(mappingId);
        for (const Mappings::Symbol plaintextSymbol : configuration.Alphabet)
        {
            const Mappings::SubMapping* subMapping = mapping.FindSubMapping(plaintextSymbol);
            if (subMapping != nullptr)
            {
                uniqueSymbols.insert(subMapping->begin(), subMapping->end());
            }
        }
    }

    symbols_.assign(uniqueSymbols.begin(), uniqueSymbols.end());
    std::sort(symbols_.begin(), symbols_.end(), [](Mappings::Symbol left, Mappings::Symbol right)
    {
        return static_cast<unsigned char>(left) < static_cast<unsigned char>(right);
    });

    for (const Mappings::Symbol symbol : symbols_)
    {
        forward_.emplace(symbol, symbol);
        inverse_.emplace(symbol, symbol);
    }
}

void SymbolPermutation::Reshuffle(std::uint64_t seed)
{
    const std::vector<std::size_t> order = Randomization::CreateShuffleOrder(symbols_.size(), seed);
    for (std::size_t index = 0; index < symbols_.size(); ++index)
    {
        const Mappings::Symbol source = symbols_[index];
        const Mappings::Symbol target = symbols_[order[index]];
        forward_[source] = target;
        inverse_[target] = source;
    }
}

Mappings::Symbol SymbolPermutation::Forward(Mappings::Symbol symbol) const noexcept
{
    const auto found = forward_.find(symbol);
    return found == forward_.end() ? symbol : found->second;
}

Mappings::Symbol SymbolPermutation::Reverse(Mappings::Symbol symbol) const noexcept
{
    const auto found = inverse_.find(symbol);
    return found == inverse_.end() ? symbol : found->second;
}

void ApplyTranspositions(
    std::string& symbols,
    const std::vector<Config::PositionalTransposition>& transpositions,
    bool encrypt)
{
    if (encrypt)
    {
        for (const Config::PositionalTransposition& transposition : transpositions)
        {
            ApplyTransposition(symbols, transposition, true);
        }
        return;
    }

    for (auto transposition = transpositions.rbegin(); transposition != transpositions.rend(); ++transposition)
    {
        ApplyTransposition(symbols, *transposition, false);
    }
}

Mappings::Symbol TransformSymbol(
    const Config::Configuration& configuration,
    std::string_view mappingId,
    Mappings::Symbol symbol,
    std::optional<std::uint64_t> subMappingSeed,
    const SymbolPermutation& permutation,
    bool encrypt)
{
    const auto mapping = configuration.MappingSets.find(std::string(mappingId));
    if (mapping == configuration.MappingSets.end())
    {
        return symbol;
    }

    if (encrypt)
    {
        const Mappings::SubMapping* subMapping = mapping->second.FindSubMapping(symbol);
        if (subMapping == nullptr || subMapping->empty())
        {
            const unsigned char offset = DerivePassthroughOffset(mappingId, subMappingSeed);
            const unsigned char keyedSymbol =
                (static_cast<unsigned char>(symbol) & 0x7F) ^ offset;
            return static_cast<Mappings::Symbol>(
                keyedSymbol | kPassthroughTag);
        }

        if (!subMappingSeed)
        {
            return permutation.Forward(subMapping->front());
        }

        const std::vector<std::size_t> shuffleOrder = Randomization::CreateShuffleOrder(
            subMapping->size(),
            Randomization::DeriveSubMappingSeed(*subMappingSeed, mappingId, symbol));
        return permutation.Forward((*subMapping)[shuffleOrder.front()]);
    }

    symbol = permutation.Reverse(symbol);
    const auto raw = static_cast<unsigned char>(symbol);
    if (raw & kPassthroughTag)
    {
        const unsigned char offset = DerivePassthroughOffset(mappingId, subMappingSeed);
        return static_cast<Mappings::Symbol>((raw & 0x7F) ^ offset);
    }

    return mapping->second.FindPlaintextSymbol(symbol).value_or(symbol);
}
}