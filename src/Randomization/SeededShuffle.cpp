#include <Maplex/Randomization/SeededShuffle.h>

#include <numeric>
#include <utility>

namespace Maplex::Randomization
{
namespace
{
class SplitMix64
{
public:
    explicit SplitMix64(std::uint64_t seed)
        : state_(seed)
    {
    }

    std::uint64_t Next()
    {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

private:
    std::uint64_t state_;
};
}

std::vector<std::size_t> CreateShuffleOrder(std::size_t count, std::uint64_t seed)
{
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), 0);

    SplitMix64 random(seed);
    for (std::size_t index = count; index > 1; --index)
    {
        const std::size_t swapIndex = static_cast<std::size_t>(random.Next() % index);
        std::swap(order[index - 1], order[swapIndex]);
    }

    return order;
}

std::uint64_t DeriveSubMappingSeed(
    std::uint64_t seed,
    std::string_view mappingId,
    Mappings::Symbol symbol)
{
    std::uint64_t derivedSeed = seed ^ 0xcbf29ce484222325ULL;
    for (const unsigned char character : mappingId)
    {
        derivedSeed = (derivedSeed ^ character) * 0x100000001b3ULL;
    }

    derivedSeed = (derivedSeed ^ static_cast<unsigned char>(symbol)) * 0x100000001b3ULL;
    return SplitMix64(derivedSeed).Next();
}

std::uint64_t DerivePunctuationSeed(
    std::uint64_t seed,
    unsigned char punctuation,
    std::string_view seedPurpose)
{
    std::uint64_t derivedSeed = seed ^ 0xcbf29ce484222325ULL;
    for (const unsigned char character : seedPurpose)
    {
        derivedSeed = (derivedSeed ^ character) * 0x100000001b3ULL;
    }

    derivedSeed = (derivedSeed ^ punctuation) * 0x100000001b3ULL;
    return SplitMix64(derivedSeed).Next();
}
}