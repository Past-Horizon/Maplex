#pragma once

#include <Maplex/Config/Configuration.h>

#include <string>
#include <string_view>

namespace Maplex::Cipher
{
class Cipher
{
public:
    explicit Cipher(Config::Configuration configuration);

    std::string Encrypt(std::string_view plaintext) const;
    std::string Decrypt(std::string_view ciphertext) const;

private:
    Config::Configuration configuration_;
};
}