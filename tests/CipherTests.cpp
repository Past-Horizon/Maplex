#include <Maplex/Cipher/Cipher.h>

#include <gtest/gtest.h>

#include <string>
#include <cstdint>
#include <optional>
#include <utility>

namespace
{
Maplex::Mappings::Mapping MakeRotatedMapping(const std::string& alphabet, std::size_t shift)
{
    Maplex::Mappings::Mapping::SubMappings subMappings;
    for (std::size_t index = 0; index < alphabet.size(); ++index)
    {
        const char plaintextSymbol = alphabet[index];
        const char ciphertextSymbol = alphabet[(index + shift) % alphabet.size()];
        subMappings.emplace(plaintextSymbol, Maplex::Mappings::SubMapping{ciphertextSymbol});
    }

    return Maplex::Mappings::Mapping(std::move(subMappings));
}

Maplex::Config::Configuration MakeConfiguration()
{
    Maplex::Config::Configuration configuration;
    configuration.Alphabet = "abcdefghijklmnopqrstuvwxyz";
    configuration.MappingSets.emplace("mapping-a", MakeRotatedMapping(configuration.Alphabet, 1));
    configuration.MappingSets.emplace("mapping-b", MakeRotatedMapping(configuration.Alphabet, 2));
    configuration.OrderedTriggers = {
        {"trigger-a", "hello", "mapping-a"},
        {"trigger-b", "love", "mapping-b"}};
    return configuration;
}

Maplex::Config::Configuration MakeSeededConfiguration(
    std::uint64_t mappingSeed,
    std::uint64_t subMappingSeed)
{
    Maplex::Config::Configuration configuration;
    configuration.Alphabet = "abc";
    configuration.MappingSets.emplace("mapping-a", Maplex::Mappings::Mapping({
        {'a', {'1', '2', '3', '4'}},
        {'b', {'5', '6', '7', '8'}},
        {'c', {'9', '!', '#', '$'}}}));
    configuration.MappingSets.emplace("mapping-b", Maplex::Mappings::Mapping({
        {'a', {'A', 'B', 'C', 'D'}},
        {'b', {'E', 'F', 'G', 'H'}},
        {'c', {'I', 'J', 'K', 'L'}}}));
    configuration.OrderedTriggers = {
        {"trigger-a", "a", "mapping-a", mappingSeed, subMappingSeed},
        {"trigger-b", "b", "mapping-b", mappingSeed + 1, subMappingSeed + 1}};
    return configuration;
}

Maplex::Config::Configuration MakeGridConfiguration()
{
    Maplex::Config::Configuration configuration;
    configuration.Alphabet = "abcdefghi";

    Maplex::Mappings::Mapping::SubMappings subMappings;
    for (std::size_t index = 0; index < configuration.Alphabet.size(); ++index)
    {
        subMappings.emplace(
            configuration.Alphabet[index],
            Maplex::Mappings::SubMapping{static_cast<char>('1' + index)});
    }

    configuration.MappingSets.emplace("mapping-a", Maplex::Mappings::Mapping(std::move(subMappings)));
    configuration.OrderedTriggers = {{"trigger-a", "a", "mapping-a"}};
    return configuration;
}

TEST(CipherTests, EncryptsAndDecryptsTextAcrossTriggerMappingChanges)
{
    const Maplex::Cipher::Cipher cipher(MakeConfiguration());
    const std::string plaintext = "hello i love you";

    const std::string ciphertext = cipher.Encrypt(plaintext);

    EXPECT_NE(ciphertext, plaintext);
    EXPECT_EQ(cipher.Decrypt(ciphertext), plaintext);
}

TEST(CipherTests, TriggerSeedsProduceRepeatableRoundTrips)
{
    const std::string plaintext = "acabacabc";
    const Maplex::Cipher::Cipher firstCipher(MakeSeededConfiguration(100, 200));
    const Maplex::Cipher::Cipher repeatedCipher(MakeSeededConfiguration(100, 200));

    const std::string ciphertext = firstCipher.Encrypt(plaintext);

    EXPECT_EQ(repeatedCipher.Encrypt(plaintext), ciphertext);
    EXPECT_EQ(firstCipher.Decrypt(ciphertext), plaintext);
}

TEST(CipherTests, PunctuationChangesSeedsForTheNextTrigger)
{
    const Maplex::Cipher::Cipher cipher(MakeSeededConfiguration(100, 200));
    const std::string withoutPunctuation = "ac bc";
    const std::string withPunctuation = "ac!bc";

    const std::string baselineCiphertext = cipher.Encrypt(withoutPunctuation);
    const std::string punctuatedCiphertext = cipher.Encrypt(withPunctuation);

    ASSERT_EQ(baselineCiphertext.size(), punctuatedCiphertext.size());
    EXPECT_NE(baselineCiphertext.back(), punctuatedCiphertext.back());
    EXPECT_EQ(cipher.Encrypt(withPunctuation), punctuatedCiphertext);
    EXPECT_EQ(cipher.Decrypt(punctuatedCiphertext), withPunctuation);
}

TEST(CipherTests, DiagonalTranspositionUsesDocumentedOrder)
{
    Maplex::Config::Configuration configuration = MakeGridConfiguration();
    configuration.Transpositions = {{Maplex::Config::TranspositionType::Diagonal, 3, 3}};
    const Maplex::Cipher::Cipher cipher(std::move(configuration));

    EXPECT_EQ(cipher.Encrypt("bcdefghia"), "253864971");
    EXPECT_EQ(cipher.Decrypt("253864971"), "bcdefghia");
}

TEST(CipherTests, ReversedDiagonalTranspositionUsesDocumentedOrder)
{
    Maplex::Config::Configuration configuration = MakeGridConfiguration();
    configuration.Transpositions = {{Maplex::Config::TranspositionType::ReversedDiagonal, 3, 3}};
    const Maplex::Cipher::Cipher cipher(std::move(configuration));

    EXPECT_EQ(cipher.Encrypt("bcdefghia"), "235468791");
    EXPECT_EQ(cipher.Decrypt("235468791"), "bcdefghia");
}

TEST(CipherTests, TranspositionsComposeAndRoundTripShortFinalBlock)
{
    Maplex::Config::Configuration configuration = MakeGridConfiguration();
    configuration.Transpositions = {
        {Maplex::Config::TranspositionType::Diagonal, 3, 3},
        {Maplex::Config::TranspositionType::ReversedDiagonal, 3, 3}};
    const Maplex::Cipher::Cipher cipher(std::move(configuration));
    const std::string plaintext = "bcdefghiabc";

    const std::string ciphertext = cipher.Encrypt(plaintext);

    EXPECT_NE(ciphertext, plaintext);
    EXPECT_EQ(cipher.Decrypt(ciphertext), plaintext);
}

TEST(CipherTests, ChangingTriggerSeedsCanChangeCiphertext)
{
    const std::string plaintext = "acabacabc";
    const Maplex::Cipher::Cipher firstCipher(MakeSeededConfiguration(100, 200));
    const Maplex::Cipher::Cipher secondCipher(MakeSeededConfiguration(101, 201));

    EXPECT_NE(firstCipher.Encrypt(plaintext), secondCipher.Encrypt(plaintext));
}
}