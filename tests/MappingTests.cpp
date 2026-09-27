#include <Maplex/Mappings/Mapping.h>
#include <Maplex/Triggers/TriggerProgression.h>

#include <gtest/gtest.h>

namespace
{
TEST(MappingTests, ValidatesConfiguredAlphabetAndSubMappings)
{
    const Maplex::Mappings::Mapping mapping({
        {'a', {'x', 'y'}},
        {'b', {'z'}}});

    EXPECT_TRUE(mapping.Validate("ab").empty());
}

TEST(MappingTests, TranslatesSymbolsInBothDirections)
{
    const Maplex::Mappings::Mapping mapping({
        {'a', {'x', 'y'}},
        {'b', {'z'}}});

    EXPECT_EQ(mapping.FindCiphertextSymbol('a', 1), 'y');
    EXPECT_EQ(mapping.FindPlaintextSymbol('z'), 'b');
}

TEST(TriggerTests, UsesTheLongestTriggerAtOnePosition)
{
    Maplex::Triggers::TriggerProgression progression({
        {"short", "a", "mapping-a"},
        {"long", "ab", "mapping-b"}});

    const auto selections = progression.ProcessText("AB");

    ASSERT_EQ(selections.size(), 1);
    EXPECT_EQ(selections[0].DominantTriggerIndex, 1);
    ASSERT_TRUE(selections[0].MappingId.has_value());
    EXPECT_EQ(*selections[0].MappingId, "mapping-b");
}

TEST(TriggerTests, AdvancesOnARepeatedTrigger)
{
    Maplex::Triggers::TriggerProgression progression({
        {"a", "hello", "mapping-a"},
        {"b", "love", "mapping-b"},
        {"c", "forest", "mapping-c"}});

    const auto first = progression.ProcessOccurrence("a");
    const auto duplicate = progression.ProcessOccurrence("a");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(duplicate.has_value());
    EXPECT_EQ(first->DominantTriggerIndex, 0);
    EXPECT_EQ(duplicate->DominantTriggerIndex, 1);
    ASSERT_TRUE(duplicate->MappingId.has_value());
    EXPECT_EQ(*duplicate->MappingId, "mapping-b");
}

TEST(TriggerTests, FinalDuplicateWrapsMappingButKeepsLastTriggerSeeds)
{
    Maplex::Triggers::TriggerProgression progression({
        {"a", "hello", "mapping-a"},
        {"b", "love", "mapping-b"},
        {"c", "forest", "mapping-c"}});

    progression.ProcessOccurrence("a");
    progression.ProcessOccurrence("a");
    progression.ProcessOccurrence("a");
    const auto wrappedDuplicate = progression.ProcessOccurrence("a");

    ASSERT_TRUE(wrappedDuplicate.has_value());
    EXPECT_EQ(wrappedDuplicate->DominantTriggerIndex, 0);
    EXPECT_EQ(wrappedDuplicate->SeedTriggerIndex, 2);
    ASSERT_TRUE(wrappedDuplicate->MappingId.has_value());
    EXPECT_EQ(*wrappedDuplicate->MappingId, "mapping-a");
}

TEST(TriggerTests, FindsTheNextMappingAndWrapsAround)
{
    Maplex::Triggers::TriggerProgression progression({
        {"a", "hello", "mapping-a"},
        {"b", "love", std::nullopt},
        {"c", "forest", "mapping-c"}});

    const auto forward = progression.ProcessOccurrence("b");
    ASSERT_TRUE(forward.has_value());
    ASSERT_TRUE(forward->MappingId.has_value());
    EXPECT_EQ(*forward->MappingId, "mapping-c");

    Maplex::Triggers::TriggerProgression wrappingProgression({
        {"a", "hello", "mapping-a"},
        {"b", "love", std::nullopt},
        {"c", "forest", std::nullopt}});

    const auto wrapped = wrappingProgression.ProcessOccurrence("c");
    ASSERT_TRUE(wrapped.has_value());
    ASSERT_TRUE(wrapped->MappingId.has_value());
    EXPECT_EQ(*wrapped->MappingId, "mapping-a");
}
}