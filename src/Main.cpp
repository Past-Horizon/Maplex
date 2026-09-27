#include <Maplex/Cipher/Cipher.h>
#include <Maplex/Config/JsonConfiguration.h>

#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
std::string ReadMessage(int argc, char** argv)
{
    if (argc > 3)
    {
        std::string message = argv[3];
        for (int index = 4; index < argc; ++index)
        {
            message.push_back(' ');
            message += argv[index];
        }

        return message;
    }

    return {std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
}

void PrintUsage(const char* executable)
{
    std::cerr << "Usage: " << executable << " <encrypt|decrypt> <config.json> [message]\n"
              << "If message is omitted, it is read from standard input.\n";
}
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        PrintUsage(argv[0]);
        return 2;
    }

    const std::string_view operation = argv[1];
    if (operation != "encrypt" && operation != "decrypt")
    {
        PrintUsage(argv[0]);
        return 2;
    }

    try
    {
        Maplex::Cipher::Cipher cipher(Maplex::Config::LoadJsonConfiguration(argv[2]));
        const std::string message = ReadMessage(argc, argv);
        const std::string result = operation == "encrypt" ? cipher.Encrypt(message) : cipher.Decrypt(message);
        std::cout.write(result.data(), static_cast<std::streamsize>(result.size()));
        std::cout.flush();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Maplex: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
