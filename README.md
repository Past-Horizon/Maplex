# Maplex — Configurable C++ Message Obfuscation

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)](CMakePresets.json)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue)](CMakeLists.txt)

Maplex is a configurable C++ library and command-line tool for transforming messages through randomized substitution mappings, trigger-driven state changes, symbol reshuffling, and positional transpositions.

It's mainly an experiment in building a weird, configurable message cipher. It is not a replacement for modern authenticated encryption.

## Design Goals

* **Configurable** : mappings, triggers, alphabets, seeds, and transformations come from the configuration.
* **Randomized** : configurations are generated using the operating system's random source.
* **Reversible** : the same saved configuration can decrypt the message it was used to encrypt.
* **Composable** : multiple transformation systems can be combined together.
* **Embeddable** : available as both a C++ library and command-line tool.

## Platform Support

| Platform | Status        | Notes                      |
| -------- | ------------- | -------------------------- |
| Windows  | ✅ Most mature | Primary development target |
| Linux    | 🟡 Supported  | Less frequently tested     |

## Features

* User-defined plaintext alphabets
* Multiple substitution Mappings
* Up to four ciphertext choices per plaintext symbol
* Trigger-driven Mapping progression
* Longest-trigger matching
* Diagonal duplicate trigger progression
* Per-trigger Mapping and Sub-mapping seeds
* Punctuation-driven seed changes
* Global ciphertext-symbol reshuffling
* Diagonal and reversed-diagonal transpositions
* JSON configuration generation and loading
* Command-line encryption and decryption

## Quick Start

```text
Maplex generate <config.json> <alphabet> [trigger-id=value...]
Maplex encrypt <config.json> [message]
Maplex decrypt <config.json> [message]
```

Example:

```text
Maplex generate generated-config.json "abcdefghijklmnopqrstuvwxyz " a_special=hello b_special=love
Maplex encrypt generated-config.json < message.txt > ciphertext.txt
Maplex decrypt generated-config.json < ciphertext.txt > recovered.txt
```

Keep the generated configuration. It is required to decrypt messages made with it.

## How It Works

Maplex processes plaintext in its original order.

Triggers can change the active Mapping and shuffle state while the message is being processed. Repeating the same trigger before another trigger appears creates a **diagonal duplicate**, which advances to the next trigger in the configured order.

For example:

```
Heyo
```

and:

```
Heyoo
```

can produce completely different ciphertext because the second `o` changes the trigger state.

After substitution and trigger processing, Maplex can also reshuffle ciphertext symbols and apply positional transpositions.

Mappings, triggers, trigger order, and their assignments are all configuration supplied rather than hardcoded.

## Configuration Generation

`ConfigGenerator` creates a fresh randomized configuration from the supplied alphabet and triggers.

It generates randomized:

* Mapping contents
* Trigger-to-Mapping assignments
* Per-trigger seeds
* Symbol shuffle seed
* Positional transformations

If no triggers are supplied, one trigger is generated for each alphabet symbol.

Generated configurations currently use four Mappings and three ciphertext choices per plaintext symbol by default.

## Security

Maplex is intended for experimentation and message obfuscation.

It does not provide authenticated encryption, integrity protection, or the security guarantees of a vetted AEAD construction.

## History

Maplex started as a small experiment after I started messing around with the idea of making a configurable cipher.

It started with substitution mappings, then grew into trigger progression, shuffled seeds, diagonal duplicates, symbol reshuffling, punctuation-driven changes, and transpositions.

## Vision

I want Maplex to stay relatively small while being a good place to experiment with unique cipher ideas and stateful transformations.

The configuration should be able to change how the system behaves without having to change the core cipher logic.

## Documentation

Detailed behavior is documented in [docs/MaplexCore.md](docs/MaplexCore.md).

Examples and configuration files are available under [examples](examples).

## License

Maplex uses the MIT license. See [LICENSE](LICENSE) for details.
