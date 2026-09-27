#include <Maplex/Config/JsonConfiguration.h>

#include <Maplex/Mappings/Mapping.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Maplex::Config
{
namespace
{
using Json = nlohmann::json;

Mappings::Symbol ParseSymbol(const Json& value, std::string_view description)
{
    if (!value.is_string())
    {
        throw std::invalid_argument(std::string(description) + " must be a one-character string.");
    }

    const std::string symbol = value.get<std::string>();
    if (symbol.size() != 1)
    {
        throw std::invalid_argument(std::string(description) + " must be a one-character string.");
    }

    return symbol.front();
}

Mappings::Mapping ParseMapping(const Json& jsonMapping)
{
    if (!jsonMapping.is_object())
    {
        throw std::invalid_argument("Each Mapping must be an object of plaintext symbols and Sub-mappings.");
    }

    Mappings::Mapping::SubMappings subMappings;
    for (auto entry = jsonMapping.begin(); entry != jsonMapping.end(); ++entry)
    {
        if (entry.key().size() != 1)
        {
            throw std::invalid_argument("Mapping keys must be one-character plaintext symbols.");
        }

        if (!entry.value().is_array())
        {
            throw std::invalid_argument("Each Sub-mapping must be an array of ciphertext symbols.");
        }

        Mappings::SubMapping symbols;
        for (const Json& value : entry.value())
        {
            symbols.push_back(ParseSymbol(value, "Ciphertext symbol"));
        }

        subMappings.emplace(entry.key().front(), std::move(symbols));
    }

    return Mappings::Mapping(std::move(subMappings));
}

std::optional<std::uint64_t> ParseSeed(const Json& triggerJson, const char* key)
{
    if (!triggerJson.contains(key) || triggerJson.at(key).is_null())
    {
        return std::nullopt;
    }

    const Json& value = triggerJson.at(key);
    if (value.is_number_unsigned())
    {
        return value.get<std::uint64_t>();
    }

    if (value.is_number_integer())
    {
        const std::int64_t signedSeed = value.get<std::int64_t>();
        if (signedSeed >= 0)
        {
            return static_cast<std::uint64_t>(signedSeed);
        }
    }

    throw std::invalid_argument(std::string("Trigger seed '") + key + "' must be a non-negative integer.");
}
}

Configuration LoadJsonConfiguration(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Could not open configuration file: " + path);
    }

    Json document;
    input >> document;
    if (!document.is_object())
    {
        throw std::invalid_argument("Configuration root must be a JSON object.");
    }

    Configuration configuration;
    configuration.Alphabet = document.at("alphabet").get<std::string>();

    const Json& mappingSets = document.at("mappings");
    if (!mappingSets.is_object())
    {
        throw std::invalid_argument("'mappings' must be an object keyed by Mapping ID.");
    }

    for (auto mapping = mappingSets.begin(); mapping != mappingSets.end(); ++mapping)
    {
        configuration.MappingSets.emplace(mapping.key(), ParseMapping(mapping.value()));
    }

    const Json& triggers = document.at("triggers");
    if (!triggers.is_array())
    {
        throw std::invalid_argument("'triggers' must be an ordered array.");
    }

    for (const Json& triggerJson : triggers)
    {
        Triggers::Trigger trigger{
            triggerJson.at("id").get<std::string>(),
            triggerJson.at("value").get<std::string>(),
            std::nullopt,
            ParseSeed(triggerJson, "mappingSeed"),
            ParseSeed(triggerJson, "subMappingSeed")};

        if (triggerJson.contains("mapping") && !triggerJson.at("mapping").is_null())
        {
            trigger.MappingId = triggerJson.at("mapping").get<std::string>();
        }

        configuration.OrderedTriggers.push_back(std::move(trigger));
    }

    if (document.contains("transpositions"))
    {
        const Json& transpositions = document.at("transpositions");
        if (!transpositions.is_array())
        {
            throw std::invalid_argument("'transpositions' must be an ordered array.");
        }

        for (const Json& transpositionJson : transpositions)
        {
            if (!transpositionJson.is_object())
            {
                throw std::invalid_argument("Each transposition must be an object.");
            }

            const std::string type = transpositionJson.at("type").get<std::string>();
            TranspositionType transpositionType;
            if (type == "diagonal")
            {
                transpositionType = TranspositionType::Diagonal;
            }
            else if (type == "reversedDiagonal")
            {
                transpositionType = TranspositionType::ReversedDiagonal;
            }
            else
            {
                throw std::invalid_argument(
                    "Transposition type must be 'diagonal' or 'reversedDiagonal'.");
            }

            configuration.Transpositions.push_back({
                transpositionType,
                transpositionJson.at("width").get<std::size_t>(),
                transpositionJson.at("height").get<std::size_t>()});
        }
    }

    return configuration;
}
}