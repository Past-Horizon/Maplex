#include <Maplex/Mappings/Mapping.h>

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Maplex::Mappings
{
Mapping::Mapping(SubMappings subMappings)
    : subMappings_(std::move(subMappings))
{
}

const SubMapping* Mapping::FindSubMapping(Symbol plaintextSymbol) const noexcept
{
    const auto subMapping = subMappings_.find(plaintextSymbol);
    if (subMapping == subMappings_.end())
    {
        return nullptr;
    }

    return &subMapping->second;
}

std::optional<Symbol> Mapping::FindCiphertextSymbol(Symbol plaintextSymbol, std::size_t choiceIndex) const noexcept
{
    const SubMapping* subMapping = FindSubMapping(plaintextSymbol);
    if (subMapping == nullptr || choiceIndex >= subMapping->size())
    {
        return std::nullopt;
    }

    return (*subMapping)[choiceIndex];
}

std::optional<Symbol> Mapping::FindPlaintextSymbol(Symbol ciphertextSymbol) const noexcept
{
    std::optional<Symbol> plaintextSymbol;
    for (const auto& [candidatePlaintext, subMapping] : subMappings_)
    {
        for (const Symbol candidateCiphertext : subMapping)
        {
            if (candidateCiphertext != ciphertextSymbol)
            {
                continue;
            }

            if (plaintextSymbol && *plaintextSymbol != candidatePlaintext)
            {
                return std::nullopt;
            }

            plaintextSymbol = candidatePlaintext;
        }
    }

    return plaintextSymbol;
}

std::vector<std::string> Mapping::Validate(const std::string& alphabet) const
{
    std::vector<std::string> errors;
    if (alphabet.empty())
    {
        errors.emplace_back("The configured alphabet cannot be empty.");
        return errors;
    }

    std::unordered_set<Symbol> alphabetSymbols;
    for (const Symbol symbol : alphabet)
    {
        if (!alphabetSymbols.insert(symbol).second)
        {
            errors.emplace_back("The configured alphabet contains a duplicate symbol: " + std::string(1, symbol));
        }
    }

    for (const Symbol plaintextSymbol : alphabetSymbols)
    {
        const auto subMapping = subMappings_.find(plaintextSymbol);
        if (subMapping == subMappings_.end())
        {
            errors.emplace_back("Missing Sub-mapping for plaintext symbol: " + std::string(1, plaintextSymbol));
            continue;
        }

        if (subMapping->second.empty() || subMapping->second.size() > 4)
        {
            errors.emplace_back("Sub-mapping for plaintext symbol " + std::string(1, plaintextSymbol) +
                                " must contain between one and four ciphertext symbols.");
        }
    }

    std::unordered_map<Symbol, Symbol> ciphertextOwners;
    for (const auto& [plaintextSymbol, subMapping] : subMappings_)
    {
        if (alphabetSymbols.find(plaintextSymbol) == alphabetSymbols.end())
        {
            errors.emplace_back("Mapping contains a Sub-mapping outside the configured alphabet: " +
                                std::string(1, plaintextSymbol));
        }

        for (const Symbol ciphertextSymbol : subMapping)
        {
            if (static_cast<unsigned char>(ciphertextSymbol) & 0x80)
            {
                errors.emplace_back(
                    "Ciphertext symbol for plaintext '" + std::string(1, plaintextSymbol) +
                    "' uses the high bit, which Maplex reserves to mark pass-through bytes: " +
                    std::string(1, ciphertextSymbol));
            }

            const auto [owner, inserted] = ciphertextOwners.try_emplace(ciphertextSymbol, plaintextSymbol);
            if (!inserted && owner->second != plaintextSymbol)
            {
                errors.emplace_back("Ciphertext symbol is assigned to multiple plaintext symbols: " +
                                    std::string(1, ciphertextSymbol));
            }
        }
    }

    return errors;
}
}