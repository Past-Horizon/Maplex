#include <Maplex/Config/Configuration.h>

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

    return errors;
}
}