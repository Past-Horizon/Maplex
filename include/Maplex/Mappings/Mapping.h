#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Maplex::Mappings
{
using Symbol = char;
using SubMapping = std::vector<Symbol>;

class Mapping
{
public:
    using SubMappings = std::unordered_map<Symbol, SubMapping>;

    explicit Mapping(SubMappings subMappings);

    const SubMapping* FindSubMapping(Symbol plaintextSymbol) const noexcept;
    std::optional<Symbol> FindCiphertextSymbol(Symbol plaintextSymbol, std::size_t choiceIndex) const noexcept;
    std::optional<Symbol> FindPlaintextSymbol(Symbol ciphertextSymbol) const noexcept;
    std::vector<std::string> Validate(const std::string& alphabet) const;

private:
    SubMappings subMappings_;
};
}