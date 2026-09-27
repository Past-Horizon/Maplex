#include <Maplex/Cipher/Cipher.h>
#include <Maplex/Config/ConfigGenerator.h>
#include <Maplex/Config/JsonConfiguration.h>
#include <Winux/Winux.h>

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
    std::cerr << "Usage:\n"
              << "  " << executable << " <encrypt|decrypt> <config.json> [message]\n"
              << "  " << executable << " generate <config.json> <alphabet> [trigger-id=value...]\n"
              << "  Omit triggers to generate one trigger per alphabet letter.\n"
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
    if (operation == "generate")
    {
        if (argc < 4)
        {
            PrintUsage(argv[0]);
            return 2;
        }

        try
        {
            Maplex::Config::ConfigGeneratorOptions options;
            options.Alphabet = argv[3];
            for (int index = 4; index < argc; ++index)
            {
                const std::string definition = argv[index];
                const std::size_t separator = definition.find('=');
                if (separator == std::string::npos || separator == 0 || separator + 1 == definition.size())
                {
                    throw std::invalid_argument("Triggers must use the form <trigger-id=value>.");
                }
                options.Triggers.push_back({definition.substr(0, separator), definition.substr(separator + 1)});
            }

            std::unique_ptr<Winux::Contracts::IPlatform> platform = Winux::Platform::create();
            const Maplex::Config::Configuration configuration =
                Maplex::Config::GenerateConfiguration(options, platform->crypto());
            Maplex::Config::SaveJsonConfiguration(configuration, argv[2]);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Maplex: " << error.what() << '\n';
            return 1;
        }
        return 0;
    }

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
