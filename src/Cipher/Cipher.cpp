#include <Maplex/Cipher/Cipher.h>
#include <Maplex/Randomization/SeededShuffle.h>
#include <Maplex/Utils/Logger.h>

#include <Maplex/Cipher/Transformations.h>

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

void ApplySymbolShuffleSelection(
    const Config::Configuration& configuration,
    const Triggers::TriggerSelection& selection,
    std::optional<std::uint64_t>& shuffleState,
    std::uint64_t& triggerOccurrence,
    Transformations::SymbolPermutation& permutation)
{
    if (!shuffleState)
    {
        return;
    }

    ++triggerOccurrence;
    const Triggers::Trigger& selectedTrigger =
        configuration.OrderedTriggers[selection.DominantTriggerIndex];
    shuffleState = Randomization::DeriveTriggerShuffleSeed(
        *shuffleState,
        selectedTrigger.Id,
        static_cast<std::uint64_t>(selection.DominantTriggerIndex),
        triggerOccurrence);
    permutation.Reshuffle(*shuffleState);
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
    std::optional<std::uint64_t> symbolShuffleState = configuration_.SymbolShuffleSeed;
    std::uint64_t triggerOccurrence = 0;
    Transformations::SymbolPermutation symbolPermutation(configuration_);
    std::vector<unsigned char> pendingPunctuation;
    std::string ciphertext;
    ciphertext.reserve(plaintext.size());

    for (std::size_t position = 0; position < plaintext.size();)
    {
        const std::optional<std::size_t> triggerIndex =
            FindLongestTrigger(configuration_.OrderedTriggers, plaintext, position);
        if (!triggerIndex)
        {
            ciphertext.push_back(Transformations::TransformSymbol(
                configuration_, activeMappingId, plaintext[position], activeSubMappingSeed, symbolPermutation, true));
            RecordPunctuation(plaintext.substr(position, 1), pendingPunctuation);
            ++position;
            continue;
        }

        LogIgnoredShorterTriggers(configuration_.OrderedTriggers, plaintext, position, *triggerIndex);
        const Triggers::Trigger& trigger = configuration_.OrderedTriggers[*triggerIndex];
        for (std::size_t offset = 0; offset < trigger.Value.size(); ++offset)
        {
            ciphertext.push_back(
                Transformations::TransformSymbol(
                    configuration_, activeMappingId, plaintext[position + offset], activeSubMappingSeed, symbolPermutation, true));
        }

        const std::optional<Triggers::TriggerSelection> selection = progression.ProcessOccurrence(trigger.Id);
        if (selection)
        {
            ApplyTriggerSelection(
                configuration_, *selection, activeMappingId, activeSubMappingSeed, pendingPunctuation);
            ApplySymbolShuffleSelection(
                configuration_, *selection, symbolShuffleState, triggerOccurrence, symbolPermutation);
        }

        RecordPunctuation(trigger.Value, pendingPunctuation);
        position += trigger.Value.size();
    }

    Transformations::ApplyTranspositions(ciphertext, configuration_.Transpositions, true);
    return ciphertext;
}

std::string Cipher::Decrypt(std::string_view ciphertext) const
{
    std::string restoredCiphertext(ciphertext);
    Transformations::ApplyTranspositions(restoredCiphertext, configuration_.Transpositions, false);

    Triggers::TriggerProgression progression(configuration_.OrderedTriggers);
    std::string activeMappingId = InitialMappingId(configuration_);
    std::optional<std::uint64_t> activeSubMappingSeed;
    std::optional<std::uint64_t> symbolShuffleState = configuration_.SymbolShuffleSeed;
    std::uint64_t triggerOccurrence = 0;
    Transformations::SymbolPermutation symbolPermutation(configuration_);
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
                Transformations::TransformSymbol(
                    configuration_, activeMappingId, restoredCiphertext[position + offset], activeSubMappingSeed, symbolPermutation, false));
        }

        const std::optional<std::size_t> triggerIndex =
            FindLongestTrigger(configuration_.OrderedTriggers, decodedRemainder, 0);
        if (!triggerIndex)
        {
            const Mappings::Symbol decodedSymbol = Transformations::TransformSymbol(
                configuration_, activeMappingId, restoredCiphertext[position], activeSubMappingSeed, symbolPermutation, false);
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
                Transformations::TransformSymbol(
                    configuration_, activeMappingId, restoredCiphertext[position + offset], activeSubMappingSeed, symbolPermutation, false));
        }

        const std::optional<Triggers::TriggerSelection> selection = progression.ProcessOccurrence(trigger.Id);
        if (selection)
        {
            ApplyTriggerSelection(
                configuration_, *selection, activeMappingId, activeSubMappingSeed, pendingPunctuation);
            ApplySymbolShuffleSelection(
                configuration_, *selection, symbolShuffleState, triggerOccurrence, symbolPermutation);
        }

        RecordPunctuation(trigger.Value, pendingPunctuation);
        position += trigger.Value.size();
    }

    return plaintext;
}
}