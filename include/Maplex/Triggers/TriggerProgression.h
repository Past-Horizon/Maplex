#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Maplex::Triggers
{
struct Trigger
{
    std::string Id;
    std::string Value;
    std::optional<std::string> MappingId;
    std::optional<std::uint64_t> MappingSeed;
    std::optional<std::uint64_t> SubMappingSeed;
};

struct TriggerSelection
{
    std::size_t DominantTriggerIndex;
    std::optional<std::string> MappingId;
};

class TriggerProgression
{
public:
    explicit TriggerProgression(std::vector<Trigger> triggers);

    std::vector<std::string> Validate() const;
    std::optional<TriggerSelection> ProcessOccurrence(std::string_view triggerId);
    std::vector<TriggerSelection> ProcessText(std::string_view plaintext);

private:
    std::optional<std::size_t> FindTriggerIndex(std::string_view triggerId) const noexcept;
    std::optional<std::size_t> FindLongestTriggerAt(std::string_view plaintext, std::size_t position) const;
    std::optional<std::string> FindMappingId(std::size_t triggerIndex) const;

    std::vector<Trigger> triggers_;
    std::optional<std::size_t> lastObservedTriggerIndex_;
    std::optional<std::size_t> dominantTriggerIndex_;
};
}