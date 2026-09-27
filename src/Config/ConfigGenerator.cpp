#include <Maplex/Config/ConfigGenerator.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Maplex::Config
{
namespace
{
constexpr std::size_t kGeneratedChoicesPerSymbol = 3;

class SecureRandom
{
public:
    explicit SecureRandom(Winux::Contracts::ICrypto& crypto)
        : crypto_(crypto)
    {
    }

    std::uint64_t Below(std::uint64_t bound)
    {
        if (bound == 0)
        {
            throw std::invalid_argument("Random selection bound must be greater than zero.");
        }

        const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
        for (;;)
        {
            const std::uint64_t value = Next();
            if (value >= threshold)
            {
                return value % bound;
            }
        }
    }

    template <typename T>
    void Shuffle(std::vector<T>& values)
    {
        for (std::size_t count = values.size(); count > 1; --count)
        {
            const std::size_t index = static_cast<std::size_t>(Below(count));
            std::swap(values[count - 1], values[index]);
        }
    }

    std::uint64_t UniqueSeed(std::unordered_set<std::uint64_t>& used)
    {
        std::uint64_t seed;
        do
        {
            seed = Next();
        }
        while (!used.insert(seed).second);
        return seed;
    }

private:
    std::uint64_t Next()
    {
        std::uint64_t value = 0;
        const auto result = crypto_.fill_random(
            std::as_writable_bytes(std::span<std::uint64_t>(&value, 1)));
        if (result.failed())
        {
            throw std::runtime_error("Could not obtain secure random bytes from Winux: " + result.message());
        }
        return value;
    }

private:
    Winux::Contracts::ICrypto& crypto_;
};

void ValidateOptions(const ConfigGeneratorOptions& options)
{
    if (options.Alphabet.empty())
    {
        throw std::invalid_argument("The generated configuration alphabet cannot be empty.");
    }
    if (options.MappingCount == 0)
    {
        throw std::invalid_argument("The generated configuration must contain at least one Mapping.");
    }
    if (options.MaxTranspositions == 0)
    {
        throw std::invalid_argument("Generated configurations must contain at least one transposition.");
    }
    if (options.MaxTranspositions > 8)
    {
        throw std::invalid_argument("Maximum generated transpositions cannot exceed eight.");
    }

    std::unordered_set<Mappings::Symbol> alphabetSymbols;
    for (const Mappings::Symbol symbol : options.Alphabet)
    {
        if (!alphabetSymbols.insert(symbol).second)
        {
            throw std::invalid_argument("The generated configuration alphabet contains duplicate symbols.");
        }
    }

    std::unordered_set<Mappings::Symbol> ciphertextSymbols;
    for (const Mappings::Symbol symbol : options.CiphertextSymbols)
    {
        if (static_cast<unsigned char>(symbol) & 0x80)
        {
            throw std::invalid_argument("Generated ciphertext symbols cannot use the reserved high bit.");
        }
        if (!ciphertextSymbols.insert(symbol).second)
        {
            throw std::invalid_argument("Generated ciphertext symbols must be unique.");
        }
    }

    if (options.Alphabet.size() > std::numeric_limits<std::size_t>::max() / kGeneratedChoicesPerSymbol)
    {
        throw std::invalid_argument("Requested alphabet is too large for three ciphertext symbols per letter.");
    }

    const std::size_t requiredSymbolsPerMapping = options.Alphabet.size() * kGeneratedChoicesPerSymbol;
    if (ciphertextSymbols.size() < requiredSymbolsPerMapping)
    {
        throw std::invalid_argument(
            "Ciphertext symbol pool is too small to provide three unique symbols for every letter in a Mapping.");
    }

    std::vector<TriggerDefinition> triggerDefinitions = options.Triggers;
    if (triggerDefinitions.empty())
    {
        triggerDefinitions.reserve(options.Alphabet.size());
        for (const Mappings::Symbol symbol : options.Alphabet)
        {
            const std::string value(1, symbol);
            triggerDefinitions.push_back({"trigger-" + value, value});
        }
    }

    std::vector<Triggers::Trigger> triggers;
    triggers.reserve(triggerDefinitions.size());
    for (const TriggerDefinition& definition : triggerDefinitions)
    {
        triggers.push_back({definition.Id, definition.Value, std::nullopt});
    }
    const std::vector<std::string> triggerErrors = Triggers::TriggerProgression(std::move(triggers)).Validate();
    if (!triggerErrors.empty())
    {
        throw std::invalid_argument(triggerErrors.front());
    }
}
}

Configuration GenerateConfiguration(
    const ConfigGeneratorOptions& options,
    Winux::Contracts::ICrypto& crypto)
{
    ValidateOptions(options);

    SecureRandom random(crypto);
    Configuration configuration;
    configuration.Alphabet = options.Alphabet;

    std::unordered_set<std::uint64_t> usedSeeds;
    configuration.SymbolShuffleSeed = random.UniqueSeed(usedSeeds);

    for (std::size_t mappingIndex = 0; mappingIndex < options.MappingCount; ++mappingIndex)
    {
        std::vector<Mappings::Symbol> availableSymbols(
            options.CiphertextSymbols.begin(),
            options.CiphertextSymbols.end());
        random.Shuffle(availableSymbols);

        Mappings::Mapping::SubMappings subMappings;
        for (std::size_t alphabetIndex = 0; alphabetIndex < options.Alphabet.size(); ++alphabetIndex)
        {
            const std::size_t firstSymbol = alphabetIndex * kGeneratedChoicesPerSymbol;
            Mappings::SubMapping choices(
                availableSymbols.begin() + firstSymbol,
                availableSymbols.begin() + firstSymbol + kGeneratedChoicesPerSymbol);
            random.Shuffle(choices);
            subMappings.emplace(options.Alphabet[alphabetIndex], std::move(choices));
        }

        configuration.MappingSets.emplace(
            "mapping-" + std::to_string(mappingIndex + 1),
            Mappings::Mapping(std::move(subMappings)));
    }

    std::vector<std::string> mappingIds;
    mappingIds.reserve(options.MappingCount);
    for (std::size_t index = 0; index < options.MappingCount; ++index)
    {
        mappingIds.push_back("mapping-" + std::to_string(index + 1));
    }

    std::vector<TriggerDefinition> triggerDefinitions = options.Triggers;
    if (triggerDefinitions.empty())
    {
        triggerDefinitions.reserve(options.Alphabet.size());
        for (const Mappings::Symbol symbol : options.Alphabet)
        {
            const std::string value(1, symbol);
            triggerDefinitions.push_back({"trigger-" + value, value});
        }
    }

    configuration.OrderedTriggers.reserve(triggerDefinitions.size());
    for (const TriggerDefinition& definition : triggerDefinitions)
    {
        const std::string& mappingId = mappingIds[static_cast<std::size_t>(random.Below(mappingIds.size()))];
        configuration.OrderedTriggers.push_back({
            definition.Id,
            definition.Value,
            mappingId,
            random.UniqueSeed(usedSeeds),
            random.UniqueSeed(usedSeeds)});
    }

    const std::size_t transpositionCount =
        1 + static_cast<std::size_t>(random.Below(options.MaxTranspositions));
    configuration.Transpositions.reserve(transpositionCount);
    for (std::size_t index = 0; index < transpositionCount; ++index)
    {
        configuration.Transpositions.push_back({
            random.Below(2) == 0 ? TranspositionType::Diagonal : TranspositionType::ReversedDiagonal,
            static_cast<std::size_t>(random.Below(4)) + 2,
            static_cast<std::size_t>(random.Below(4)) + 2});
    }

    const std::vector<std::string> errors = configuration.Validate();
    if (!errors.empty())
    {
        throw std::logic_error("Generated invalid configuration: " + errors.front());
    }

    return configuration;
}
}