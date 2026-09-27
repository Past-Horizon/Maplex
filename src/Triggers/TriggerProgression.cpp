#include <Maplex/Triggers/TriggerProgression.h>
#include <Maplex/Utils/Logger.h>

#include <cctype>
#include <unordered_set>
#include <utility>

namespace Maplex::Triggers
{
namespace
{
std::string NormalizeValue(std::string_view value)
{
    std::string normalized;
    normalized.reserve(value.size());

    for (const unsigned char character : value)
    {
        normalized.push_back(static_cast<char>(std::tolower(character)));
    }

    return normalized;
}

bool MatchesAt(std::string_view text, std::size_t position, std::string_view value)
{
    if (position + value.size() > text.size())
    {
        return false;
    }

    for (std::size_t offset = 0; offset < value.size(); ++offset)
    {
        const auto textCharacter = static_cast<unsigned char>(text[position + offset]);
        const auto valueCharacter = static_cast<unsigned char>(value[offset]);
        if (std::tolower(textCharacter) != std::tolower(valueCharacter))
        {
            return false;
        }
    }

    return true;
}
}

TriggerProgression::TriggerProgression(std::vector<Trigger> triggers)
    : triggers_(std::move(triggers))
{
}

std::vector<std::string> TriggerProgression::Validate() const
{
    std::vector<std::string> errors;
    std::unordered_set<std::string> triggerIds;
    std::unordered_set<std::string> triggerValues;

    for (const Trigger& trigger : triggers_)
    {
        if (trigger.Id.empty())
        {
            errors.emplace_back("Trigger IDs cannot be empty.");
        }
        else if (!triggerIds.insert(trigger.Id).second)
        {
            errors.emplace_back("Trigger IDs must be unique: " + trigger.Id);
        }

        if (trigger.Value.empty())
        {
            errors.emplace_back("Trigger values cannot be empty.");
        }
        else if (!triggerValues.insert(NormalizeValue(trigger.Value)).second)
        {
            errors.emplace_back("Trigger values must be unique ignoring letter case: " + trigger.Value);
        }

        if (trigger.MappingId && trigger.MappingId->empty())
        {
            errors.emplace_back("A Mapping ID must not be empty when assigned to a trigger.");
        }
    }

    return errors;
}

std::optional<TriggerSelection> TriggerProgression::ProcessOccurrence(std::string_view triggerId)
{
    if (triggers_.empty())
    {
        return std::nullopt;
    }

    const std::optional<std::size_t> triggerIndex = FindTriggerIndex(triggerId);
    if (!triggerIndex)
    {
        return std::nullopt;
    }

    if (lastObservedTriggerIndex_ == triggerIndex)
    {
        const std::size_t previousDominantIndex = *dominantTriggerIndex_;
        dominantTriggerIndex_ = (previousDominantIndex + 1) % triggers_.size();
        const std::size_t seedTriggerIndex = previousDominantIndex == triggers_.size() - 1
            ? previousDominantIndex
            : *dominantTriggerIndex_;
        lastObservedTriggerIndex_ = triggerIndex;
        return TriggerSelection{
            *dominantTriggerIndex_,
            seedTriggerIndex,
            FindMappingId(*dominantTriggerIndex_)};
    }

    dominantTriggerIndex_ = *triggerIndex;
    lastObservedTriggerIndex_ = triggerIndex;
    return TriggerSelection{
        *dominantTriggerIndex_,
        *dominantTriggerIndex_,
        FindMappingId(*dominantTriggerIndex_)};
}

std::vector<TriggerSelection> TriggerProgression::ProcessText(std::string_view plaintext)
{
    std::vector<TriggerSelection> selections;
    for (std::size_t position = 0; position < plaintext.size(); ++position)
    {
        const std::optional<std::size_t> selectedIndex = FindLongestTriggerAt(plaintext, position);
        if (!selectedIndex)
        {
            continue;
        }

        const Trigger& selectedTrigger = triggers_[*selectedIndex];
        for (const Trigger& candidate : triggers_)
        {
            if (candidate.Value.size() >= selectedTrigger.Value.size() ||
                !MatchesAt(plaintext, position, candidate.Value))
            {
                continue;
            }

            Utils::Log(
                Utils::Level::Info,
                Utils::Category::Trigger,
                "Ignored trigger '" + candidate.Value + "' because longer trigger '" + selectedTrigger.Value +
                    "' matched at the same position.");
        }

        const std::optional<TriggerSelection> selection = ProcessOccurrence(triggers_[*selectedIndex].Id);
        if (selection)
        {
            selections.push_back(*selection);
        }
    }

    return selections;
}

std::optional<std::size_t> TriggerProgression::FindTriggerIndex(std::string_view triggerId) const noexcept
{
    for (std::size_t index = 0; index < triggers_.size(); ++index)
    {
        if (triggers_[index].Id == triggerId)
        {
            return index;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> TriggerProgression::FindLongestTriggerAt(
    std::string_view plaintext,
    std::size_t position) const
{
    std::optional<std::size_t> longestIndex;
    for (std::size_t index = 0; index < triggers_.size(); ++index)
    {
        if (!MatchesAt(plaintext, position, triggers_[index].Value))
        {
            continue;
        }

        if (!longestIndex || triggers_[index].Value.size() > triggers_[*longestIndex].Value.size())
        {
            longestIndex = index;
        }
    }

    return longestIndex;
}

std::optional<std::string> TriggerProgression::FindMappingId(std::size_t triggerIndex) const
{
    if (triggers_.empty())
    {
        return std::nullopt;
    }

    for (std::size_t offset = 0; offset < triggers_.size(); ++offset)
    {
        const std::size_t candidateIndex = (triggerIndex + offset) % triggers_.size();
        if (triggers_[candidateIndex].MappingId)
        {
            return triggers_[candidateIndex].MappingId;
        }
    }

    return std::nullopt;
}
}