#include <Maplex/Cipher/Cipher.h>
#include <Maplex/Randomization/SeededShuffle.h>
#include <Maplex/Utils/Logger.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace Maplex::Cipher
{
namespace
{
std::optional<std::size_t> FindLongestTrigger(
    const std::vector<Triggers::Trigger>& triggers,
    std::string_view text,
    std::size_t position)
{
    std::optional<std::size_t> longestIndex;
    for (std::size_t index = 0; index < triggers.size(); ++index)
    {
        const std::string_view value = triggers[index].Value;
        if (value.empty() || position + value.size() > text.size())
        {
            continue;
        }

        bool matches = true;
        for (std::size_t offset = 0; offset < value.size(); ++offset)
        {
            const auto textCharacter = static_cast<unsigned char>(text[position + offset]);
            const auto valueCharacter = static_cast<unsigned char>(value[offset]);
            if (std::tolower(textCharacter) != std::tolower(valueCharacter))
            {
                matches = false;
                break;
            }
        }

        if (matches && (!longestIndex || value.size() > triggers[*longestIndex].Value.size()))
        {
            longestIndex = index;
        }
    }

    return longestIndex;
}

std::size_t LongestTriggerLength(const std::vector<Triggers::Trigger>& triggers)
{
    std::size_t longestLength = 0;
    for (const Triggers::Trigger& trigger : triggers)
    {
        longestLength = std::max(longestLength, trigger.Value.size());
    }

    return longestLength;
}

std::optional<std::string> FindMappingId(
    const std::vector<Triggers::Trigger>& triggers,
    std::size_t triggerIndex,
    std::optional<std::uint64_t> seed = std::nullopt)
{
    std::vector<std::optional<std::string>> assignments;
    assignments.reserve(triggers.size());
    for (const Triggers::Trigger& trigger : triggers)
    {
        assignments.push_back(trigger.MappingId);
    }

    if (seed)
    {
        std::vector<std::size_t> mappedSlots;
        std::vector<std::string> mappingIds;
        for (std::size_t index = 0; index < assignments.size(); ++index)
        {
            if (assignments[index])
            {
                mappedSlots.push_back(index);
                mappingIds.push_back(*assignments[index]);
            }
        }

        const std::vector<std::size_t> shuffleOrder =
            Randomization::CreateShuffleOrder(mappingIds.size(), *seed);
        for (std::size_t index = 0; index < mappedSlots.size(); ++index)
        {
            assignments[mappedSlots[index]] = std::move(mappingIds[shuffleOrder[index]]);
        }
    }

    for (std::size_t offset = 0; offset < triggers.size(); ++offset)
    {
        const std::size_t candidateIndex = (triggerIndex + offset) % triggers.size();
        if (assignments[candidateIndex])
        {
            return assignments[candidateIndex];
        }
    }

    return std::nullopt;
}

std::string InitialMappingId(const Config::Configuration& configuration)
{
    const std::optional<std::string> mappingId = FindMappingId(configuration.OrderedTriggers, 0);
    if (!mappingId)
    {
        throw std::invalid_argument("At least one trigger must point to a Mapping to start the cipher.");
    }

    return *mappingId;
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

void ApplyTranspositions(std::string& symbols, const std::vector<Config::PositionalTransposition>& transpositions, bool encrypt)
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

constexpr unsigned char kPassthroughTag = 0x80;
constexpr std::string_view kMappingSeedPurpose = "mapping-seed";
constexpr std::string_view kSubMappingSeedPurpose = "sub-mapping-seed";

bool IsPunctuation(unsigned char character)
{
    return (character >= '!' && character <= '/') ||
        (character >= ':' && character <= '@') ||
        (character >= '[' && character <= '`') ||
        (character >= '{' && character <= '~');
}

void RecordPunctuation(std::string_view text, std::vector<unsigned char>& pendingPunctuation)
{
    for (const unsigned char character : text)
    {
        if (IsPunctuation(character))
        {
            pendingPunctuation.push_back(character);
        }
    }
}

std::optional<std::uint64_t> ApplyPunctuationEvents(
    std::optional<std::uint64_t> seed,
    const std::vector<unsigned char>& pendingPunctuation,
    std::string_view seedPurpose)
{
    if (!seed)
    {
        return std::nullopt;
    }

    for (const unsigned char punctuation : pendingPunctuation)
    {
        seed = Randomization::DerivePunctuationSeed(*seed, punctuation, seedPurpose);
    }

    return seed;
}

Mappings::Symbol TransformSymbol(
    const Config::Configuration& configuration,
    std::string_view mappingId,
    Mappings::Symbol symbol,
    std::optional<std::uint64_t> subMappingSeed,
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
            return static_cast<Mappings::Symbol>(
                static_cast<unsigned char>(symbol) | kPassthroughTag);
        }

        if (!subMappingSeed)
        {
            return subMapping->front();
        }

        Mappings::SubMapping choices = *subMapping;
        const std::vector<std::size_t> shuffleOrder = Randomization::CreateShuffleOrder(
            choices.size(),
            Randomization::DeriveSubMappingSeed(*subMappingSeed, mappingId, symbol));
        return choices[shuffleOrder.front()];
    }

    const auto raw = static_cast<unsigned char>(symbol);
    if (raw & kPassthroughTag)
    {
        return static_cast<Mappings::Symbol>(raw & static_cast<unsigned char>(~kPassthroughTag));
    }

    return mapping->second.FindPlaintextSymbol(symbol).value_or(symbol);
}

void ApplyTriggerSelection(
    const Config::Configuration& configuration,
    const Triggers::TriggerSelection& selection,
    std::string& activeMappingId,
    std::optional<std::uint64_t>& activeSubMappingSeed,
    std::vector<unsigned char>& pendingPunctuation)
{
    const Triggers::Trigger& seedTrigger = configuration.OrderedTriggers[selection.SeedTriggerIndex];
    const std::optional<std::string> mappingId = FindMappingId(
        configuration.OrderedTriggers,
        selection.DominantTriggerIndex,
        ApplyPunctuationEvents(seedTrigger.MappingSeed, pendingPunctuation, kMappingSeedPurpose));

    if (mappingId)
    {
        activeMappingId = *mappingId;
    }

    activeSubMappingSeed = ApplyPunctuationEvents(
        seedTrigger.SubMappingSeed,
        pendingPunctuation,
        kSubMappingSeedPurpose);
    pendingPunctuation.clear();
}

void LogIgnoredShorterTriggers(
    const std::vector<Triggers::Trigger>& triggers,
    std::string_view text,
    std::size_t position,
    std::size_t selectedIndex)
{
    const Triggers::Trigger& selected = triggers[selectedIndex];
    for (const Triggers::Trigger& candidate : triggers)
    {
        if (candidate.Value.size() >= selected.Value.size() ||
            position + candidate.Value.size() > text.size())
        {
            continue;
        }

        bool matches = true;
        for (std::size_t offset = 0; offset < candidate.Value.size(); ++offset)
        {
            const auto textCharacter = static_cast<unsigned char>(text[position + offset]);
            const auto valueCharacter = static_cast<unsigned char>(candidate.Value[offset]);
            if (std::tolower(textCharacter) != std::tolower(valueCharacter))
            {
                matches = false;
                break;
            }
        }

        if (matches)
        {
            Utils::Log(
                Utils::Level::Info,
                Utils::Category::Trigger,
                "Ignored trigger '" + candidate.Value + "' because longer trigger '" + selected.Value +
                    "' matched at the same position.");
        }
    }
}
}

Cipher::Cipher(Config::Configuration configuration)
    : configuration_(std::move(configuration))
{
    const std::vector<std::string> errors = configuration_.Validate();
    if (!errors.empty())
    {
        throw std::invalid_argument(errors.front());
    }
}

std::string Cipher::Encrypt(std::string_view plaintext) const
{
    Triggers::TriggerProgression progression(configuration_.OrderedTriggers);
    std::string activeMappingId = InitialMappingId(configuration_);
    std::optional<std::uint64_t> activeSubMappingSeed;
    std::vector<unsigned char> pendingPunctuation;
    std::string ciphertext;
    ciphertext.reserve(plaintext.size());

    for (std::size_t position = 0; position < plaintext.size();)
    {
        const std::optional<std::size_t> triggerIndex =
            FindLongestTrigger(configuration_.OrderedTriggers, plaintext, position);
        if (!triggerIndex)
        {
            ciphertext.push_back(TransformSymbol(
                configuration_, activeMappingId, plaintext[position], activeSubMappingSeed, true));
            RecordPunctuation(plaintext.substr(position, 1), pendingPunctuation);
            ++position;
            continue;
        }

        LogIgnoredShorterTriggers(configuration_.OrderedTriggers, plaintext, position, *triggerIndex);
        const Triggers::Trigger& trigger = configuration_.OrderedTriggers[*triggerIndex];
        for (std::size_t offset = 0; offset < trigger.Value.size(); ++offset)
        {
            ciphertext.push_back(
                TransformSymbol(
                    configuration_, activeMappingId, plaintext[position + offset], activeSubMappingSeed, true));
        }

        const std::optional<Triggers::TriggerSelection> selection = progression.ProcessOccurrence(trigger.Id);
        if (selection)
        {
            ApplyTriggerSelection(
                configuration_, *selection, activeMappingId, activeSubMappingSeed, pendingPunctuation);
        }

        RecordPunctuation(trigger.Value, pendingPunctuation);
        position += trigger.Value.size();
    }

    ApplyTranspositions(ciphertext, configuration_.Transpositions, true);
    return ciphertext;
}

std::string Cipher::Decrypt(std::string_view ciphertext) const
{
    std::string restoredCiphertext(ciphertext);
    ApplyTranspositions(restoredCiphertext, configuration_.Transpositions, false);

    Triggers::TriggerProgression progression(configuration_.OrderedTriggers);
    std::string activeMappingId = InitialMappingId(configuration_);
    std::optional<std::uint64_t> activeSubMappingSeed;
    std::vector<unsigned char> pendingPunctuation;
    std::string plaintext;
    plaintext.reserve(ciphertext.size());
    const std::size_t lookaheadLength = LongestTriggerLength(configuration_.OrderedTriggers);

    for (std::size_t position = 0; position < restoredCiphertext.size();)
    {
        std::string decodedRemainder;
        const std::size_t availableLength = restoredCiphertext.size() - position;
        const std::size_t windowLength = std::min(availableLength, lookaheadLength);
        decodedRemainder.reserve(windowLength);
        for (std::size_t offset = 0; offset < windowLength; ++offset)
        {
            decodedRemainder.push_back(
                TransformSymbol(
                    configuration_, activeMappingId, restoredCiphertext[position + offset], activeSubMappingSeed, false));
        }

        const std::optional<std::size_t> triggerIndex =
            FindLongestTrigger(configuration_.OrderedTriggers, decodedRemainder, 0);
        if (!triggerIndex)
        {
            const Mappings::Symbol decodedSymbol = TransformSymbol(
                configuration_, activeMappingId, restoredCiphertext[position], activeSubMappingSeed, false);
            plaintext.push_back(decodedSymbol);
            RecordPunctuation(std::string_view(&decodedSymbol, 1), pendingPunctuation);
            ++position;
            continue;
        }

        LogIgnoredShorterTriggers(configuration_.OrderedTriggers, decodedRemainder, 0, *triggerIndex);
        const Triggers::Trigger& trigger = configuration_.OrderedTriggers[*triggerIndex];
        for (std::size_t offset = 0; offset < trigger.Value.size(); ++offset)
        {
            plaintext.push_back(
                TransformSymbol(
                    configuration_, activeMappingId, restoredCiphertext[position + offset], activeSubMappingSeed, false));
        }

        const std::optional<Triggers::TriggerSelection> selection = progression.ProcessOccurrence(trigger.Id);
        if (selection)
        {
            ApplyTriggerSelection(
                configuration_, *selection, activeMappingId, activeSubMappingSeed, pendingPunctuation);
        }

        RecordPunctuation(trigger.Value, pendingPunctuation);
        position += trigger.Value.size();
    }

    return plaintext;
}
}