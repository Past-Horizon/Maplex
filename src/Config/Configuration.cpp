#include <Maplex/Config/Configuration.h>

#include <limits>

namespace Maplex::Config
{
std::vector<std::string> Configuration::Validate() const
{
    std::vector<std::string> errors;
    if (MappingSets.empty())
    {
        errors.emplace_back("At least one Mapping must be configured.");
    }

    for (const auto& [mappingId, mapping] : MappingSets)
    {
        if (mappingId.empty())
        {
            errors.emplace_back("Mapping IDs cannot be empty.");
        }

        for (const std::string& mappingError : mapping.Validate(Alphabet))
        {
            errors.emplace_back("Mapping '" + mappingId + "': " + mappingError);
        }
    }

    for (const std::string& triggerError : Triggers::TriggerProgression(OrderedTriggers).Validate())
    {
        errors.emplace_back(triggerError);
    }

    for (const Triggers::Trigger& trigger : OrderedTriggers)
    {
        if (trigger.MappingId && MappingSets.find(*trigger.MappingId) == MappingSets.end())
        {
            errors.emplace_back("Trigger '" + trigger.Id + "' refers to unknown Mapping '" +
                                *trigger.MappingId + "'.");
        }
    }

    for (const PositionalTransposition& transposition : Transpositions)
    {
        if (transposition.Width == 0 || transposition.Height == 0)
        {
            errors.emplace_back("Transposition block width and height must be greater than zero.");
            continue;
        }

        if (transposition.Width > std::numeric_limits<std::size_t>::max() / transposition.Height ||
            transposition.Width > std::numeric_limits<std::size_t>::max() - transposition.Height)
        {
            errors.emplace_back("Transposition block dimensions are too large.");
        }
    }

    return errors;
}
}