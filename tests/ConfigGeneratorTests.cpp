#include <Maplex/Cipher/Cipher.h>
#include <Maplex/Config/ConfigGenerator.h>
#include <Maplex/Config/JsonConfiguration.h>
#include <Winux/Winux.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{
Maplex::Config::ConfigGeneratorOptions MakeOptions()
{
    Maplex::Config::ConfigGeneratorOptions options;
    options.Alphabet = "abcdef";
    options.Triggers = {
        {"trigger-a", "abc"},
        {"trigger-b", "def"},
        {"trigger-c", "cab"}};
    options.MaxTranspositions = 2;
    return options;
}

TEST(ConfigGeneratorTests, GeneratesThreeUniqueCipherSymbolsForEveryLetterInEachMapping)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    const Maplex::Config::Configuration configuration =
        Maplex::Config::GenerateConfiguration(MakeOptions(), platform->crypto());

    EXPECT_TRUE(configuration.Validate().empty());
    ASSERT_TRUE(configuration.SymbolShuffleSeed.has_value());
    EXPECT_EQ(configuration.MappingSets.size(), 4);

    for (const auto& [mappingId, mapping] : configuration.MappingSets)
    {
        static_cast<void>(mappingId);
        std::unordered_set<Maplex::Mappings::Symbol> usedSymbols;
        for (const char plaintextSymbol : configuration.Alphabet)
        {
            const Maplex::Mappings::SubMapping* subMapping = mapping.FindSubMapping(plaintextSymbol);
            ASSERT_NE(subMapping, nullptr);
            ASSERT_EQ(subMapping->size(), 3);
            for (const char ciphertextSymbol : *subMapping)
            {
                EXPECT_TRUE(usedSymbols.insert(ciphertextSymbol).second)
                    << "Ciphertext symbol was reused: " << ciphertextSymbol;
            }
        }
    }

    std::unordered_set<std::uint64_t> usedSeeds{*configuration.SymbolShuffleSeed};
    for (const Maplex::Triggers::Trigger& trigger : configuration.OrderedTriggers)
    {
        ASSERT_TRUE(trigger.MappingId.has_value());
        ASSERT_TRUE(trigger.MappingSeed.has_value());
        ASSERT_TRUE(trigger.SubMappingSeed.has_value());
        EXPECT_TRUE(usedSeeds.insert(*trigger.MappingSeed).second);
        EXPECT_TRUE(usedSeeds.insert(*trigger.SubMappingSeed).second);
    }
}

TEST(ConfigGeneratorTests, SavedGeneratedConfigurationCanDecryptTheSameMessage)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    const Maplex::Config::Configuration generated =
        Maplex::Config::GenerateConfiguration(MakeOptions(), platform->crypto());
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "maplex-generated-config-test.json";
    Maplex::Config::SaveJsonConfiguration(generated, path.string());

    const Maplex::Config::Configuration loaded = Maplex::Config::LoadJsonConfiguration(path.string());
    std::filesystem::remove(path);

    const Maplex::Cipher::Cipher cipher(loaded);
    const std::string plaintext = "abc def cab abcdef";
    const std::string ciphertext = cipher.Encrypt(plaintext);

    EXPECT_EQ(cipher.Decrypt(ciphertext), plaintext);
}

TEST(ConfigGeneratorTests, RejectsInsufficientCiphertextSymbolPool)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    Maplex::Config::ConfigGeneratorOptions options = MakeOptions();
    options.CiphertextSymbols = "123";

    EXPECT_THROW(
        Maplex::Config::GenerateConfiguration(options, platform->crypto()),
        std::invalid_argument);
}
}

TEST(ConfigGeneratorTests, SupportsOneMapping)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    Maplex::Config::ConfigGeneratorOptions options = MakeOptions();
    options.MappingCount = 1;

    const Maplex::Config::Configuration configuration =
        Maplex::Config::GenerateConfiguration(options, platform->crypto());

    EXPECT_EQ(configuration.MappingSets.size(), 1);
    EXPECT_TRUE(configuration.Validate().empty());
}

TEST(ConfigGeneratorTests, RejectsZeroMappings)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    Maplex::Config::ConfigGeneratorOptions options = MakeOptions();
    options.MappingCount = 0;

    EXPECT_THROW(
        Maplex::Config::GenerateConfiguration(options, platform->crypto()),
        std::invalid_argument);
}

TEST(ConfigGeneratorTests, DefaultsToOneTriggerPerAlphabetLetter)
{
    std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
    Maplex::Config::ConfigGeneratorOptions options = MakeOptions();
    options.Triggers.clear();

    const Maplex::Config::Configuration configuration =
        Maplex::Config::GenerateConfiguration(options, platform->crypto());

    ASSERT_EQ(configuration.OrderedTriggers.size(), options.Alphabet.size());
    for (std::size_t index = 0; index < options.Alphabet.size(); ++index)
    {
        const std::string value(1, options.Alphabet[index]);
        EXPECT_EQ(configuration.OrderedTriggers[index].Id, "trigger-" + value);
        EXPECT_EQ(configuration.OrderedTriggers[index].Value, value);
    }

    const Maplex::Cipher::Cipher cipher(configuration);
    EXPECT_EQ(cipher.Decrypt(cipher.Encrypt(options.Alphabet)), options.Alphabet);
}