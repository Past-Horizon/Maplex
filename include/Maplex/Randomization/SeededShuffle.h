#pragma once

#include <Maplex/Mappings/Mapping.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Maplex::Randomization
{
std::vector<std::size_t> CreateShuffleOrder(std::size_t count, std::uint64_t seed);
std::uint64_t DeriveSubMappingSeed(std::uint64_t seed, std::string_view mappingId, Mappings::Symbol symbol);
std::uint64_t DerivePunctuationSeed(
	std::uint64_t seed,
	unsigned char punctuation,
	std::string_view seedPurpose);
std::uint64_t DeriveTriggerShuffleSeed(
	std::uint64_t seed,
	std::string_view triggerId,
	std::uint64_t progressionIndex,
	std::uint64_t occurrence);
}