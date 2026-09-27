**## Maplex**

Maplex uses substitution mappings and ordered plaintext triggers to determine which mapping is dominant while processing a message. After substitution, positional transformations can rearrange the resulting ciphertext.

**### Customization Requirement**

Mappings, Sub-mappings, triggers, trigger order, and trigger-to-Mapping assignments are configuration supplied by the person creating a Maplex setup. They must not be hardcoded as a fixed set of built-in values. The cipher logic operates on the supplied configuration. Names and values such as *`a_special`*, *`hello`*, and the example Mappings below are illustrative only; each customer can design their own.

**### Config Generator**

ConfigGenerator creates a fresh randomized Maplex configuration from an alphabet and trigger definitions supplied by the user. The generator uses the operating system's random source when creating the configuration. It randomizes Mapping contents, trigger-to-Mapping assignments, per-trigger Mapping and Sub-mapping seeds, the global symbol-shuffle seed, and optional positional transpositions.

Each plaintext symbol receives exactly three ciphertext symbols in every generated Mapping. Within a Mapping, ciphertext symbols are selected without replacement, so no symbol belongs to two different plaintext letters in that Mapping. Different Mappings may reuse ciphertext symbols because the active Mapping determines how a symbol is decoded. If the supplied ciphertext-symbol pool is too small to provide three unique symbols for every alphabet letter in each Mapping, generation fails.

The mapping count is configurable, with a minimum of one and a default of four. If no trigger definitions are supplied, ConfigGenerator creates one trigger per alphabet symbol, using that symbol as its trigger value. Custom trigger IDs and values can instead be supplied; trigger values are the plaintext strings that activate those triggers.

The generated configuration is the random part of the process. Once generated, Maplex encryption and decryption use its stored mappings, seeds, and transformation settings deterministically. Save the generated configuration and use that same file for both encryption and decryption. Generating another configuration, even with the same trigger definitions, creates a different key configuration and will not decrypt messages made with the first one.

The command-line generator accepts the output path and alphabet. Trigger definitions are optional; omitting them creates one trigger per letter:

```text
Maplex generate generated-config.json abcdefghijklmnopqrstuvwxyz a_special=hello b_special=love
```

Generated Mapping IDs use the form *`mapping-1`*, *`mapping-2`*, and so on. These IDs label the generated mappings; their contents and trigger assignments are randomized.

**### Mappings**

A Mapping is a substitution alphabet covering the entire alphabet. A Sub-mapping is the set of possible ciphertext symbols for one plaintext symbol within a Mapping. A Sub-mapping can contain up to four ciphertext symbols, and each of those symbols maps back to that plaintext symbol.

Each Mapping must have a Sub-mapping for every letter in the alphabet. There can be multiple Mappings, such as the Mapping used by *`a_special`*, the Mapping used by *`b_special`*, and so on.

Within one Mapping, ciphertext symbols assigned to different plaintext letters must not overlap. Otherwise, a ciphertext symbol could decode as more than one letter when that Mapping is active. Different Mappings may reuse ciphertext symbols because the active Mapping determines how a symbol is decoded.

**### Triggers and Their Order**

Triggers are plaintext content that secretly controls mapping progression. A trigger can be a word, a letter, a number, or a special character. A letter trigger can match a letter inside a word or on its own; number and special-character triggers can likewise occur naturally in the message. Letter triggers are matched in lowercase, so *`Hello`* contains the trigger *`h`* whether the letter appears in a word or by itself.

The configured triggers have a fixed order for that Maplex setup. The user defines the triggers and their order; the order is not a hardcoded list:

1. *`a_special`*

2. *`b_special`*

3. *`c_special`*

4. Continue in order for any additional triggers.

The trigger values can be configured. For example:

- *`a_special`* is the word *`hello`*.

- *`b_special`* is the word *`love`*.

If multiple configured trigger values match at the same position in the plaintext, use the longest matching trigger. Ignore shorter matches at that position and log that they were ignored. Continue scanning from the next character, so trigger values can still match inside words.

When a trigger appears, it selects the Mapping assigned to that trigger and uses that trigger's shuffle seeds. If the same trigger appears again before a different trigger appears, the repeat is a **diagonal duplicate**. A diagonal duplicate advances to the next trigger in the fixed order and uses that trigger's Mapping assignment and shuffle seeds, wrapping to the first trigger when needed. Each further repeat before a different trigger appears advances again. When a different trigger appears, it selects that trigger and ends the duplicate run.

**### Trigger-to-Mapping Assignment**

A trigger may point to zero or one Mapping; a trigger cannot point to more than one Mapping. Multiple triggers may point to the same Mapping. For example, *`e_special`* may also point to *`a_mappings`*.

When a trigger has no Mapping assigned, look forward in trigger order for the next trigger that points to a Mapping. Use that Mapping. If no Mapping is assigned later in the order, wrap around to the first trigger with an assigned Mapping.

For example, if *`c_special`* has no Mapping assigned and there are no later triggers with an assigned Mapping, wrap around to the first trigger with an assigned Mapping.

**### Examples**

With *`hello`* configured as *`a_special`*, *`love`* as *`b_special`*, and *`forest`* as *`c_special`*:

- *`Hello, I'm John!`* triggers *`a_special`* and uses the Mapping assigned to it.

- *`Hello, I'm John, and I love cats!`* triggers *`a_special`* and then *`b_special`*, selecting the Mapping assigned to each trigger.

- *`Hello! I am John. I love to spend time with my family in the forest, and go camping.`* triggers A, B, and C once each. This has no diagonal duplicate.
- *`Hello! I am John, and I love cats! I also love dogs!`* triggers A, then B twice before another trigger appears. The second *`love`* is a diagonal duplicate, so progression advances to *`c_special`* and uses its assigned Mapping. Each further *`love`* before a different trigger appears advances again.

The central idea is that ordinary words hidden in the plaintext change which substitution mapping is dominant, following the configured trigger order.

**### Per-Trigger Shuffle Seeds**

Each trigger may have two optional, independent pseudo-random seeds associated with the Mapping it points to:

- **Mapping seed:** Controls the shuffle of complete Mapping assignments across triggers. It changes which complete Mapping a trigger points to, while preserving the rule that a trigger points to at most one Mapping. Triggers without a Mapping assignment remain eligible for the existing forward-search and wraparound behavior.
- **Sub-mapping seed:** Controls the order of the ciphertext-symbol choices inside the Sub-mappings of that trigger's assigned Mapping. It changes the choice order for each letter, not the order of letters or the trigger order.

The Mapping seed and Sub-mapping seed are configured separately, so a customer can enable either shuffle, both, or neither for a trigger. A configured seed must reproduce the same shuffle during encryption and decryption. Trigger progression still follows the configured order: a normal trigger occurrence uses that trigger's seeds, while a diagonal duplicate advances to the next trigger's seeds. If a diagonal duplicate is already at the last trigger and there is no next trigger to advance to, reuse that last trigger's own seeds rather than wrapping the seed selection. This seed fallback does not change the separate wraparound rule for resolving an unassigned Mapping.

For example, if triggers A and B point to different complete Mappings, an occurrence of A uses A's Mapping and Sub-mapping seeds. A diagonal duplicate of A advances to B and uses B's Mapping and Sub-mapping seeds. If that duplicate progression is at the last configured trigger, it reuses the last trigger's seeds. The same arrangement is reconstructed when decrypting with the same configuration and seeds.

For example, without a seed-based reorder:

- Mapping position A uses the Sub-mappings from Mapping A.

- Mapping position B uses the Sub-mappings from Mapping B.

A Mapping shuffle could swap them:

- Mapping position A uses the Sub-mappings from Mapping B.

- Mapping position B uses the Sub-mappings from Mapping A.

**### Punctuation-Driven Seed Changes**

Punctuation in the plaintext can change the seeds used by a later trigger. When Maplex encounters a punctuation character, it records that event. The next trigger that appears in the message uses its configured Mapping and Sub-mapping seeds, changed by the recorded punctuation events. This means the next trigger that occurs in the message is affected; it does not mean the next trigger in the configured order.

If more punctuation appears before that trigger, each punctuation event is included in the seed change. After the next trigger uses the changed seeds, the recorded events are cleared. The trigger still follows the normal trigger rules, including diagonal duplicate progression and Mapping assignment.

The seed-change operation must be deterministic and applied in the same order during encryption and decryption. Decryption first restores plaintext order by undoing positional transformations, then recognizes punctuation as it recovers the plaintext. If a punctuation character is also configured as a trigger, it still performs its normal trigger behavior; its punctuation event affects the next trigger occurrence after it.

If the affected trigger has no configured seed for one of the two seed types, punctuation does not create a seed for that type.

**### Trigger-Driven Ciphertext-Symbol Reshuffling**

Maplex can optionally change the effective ciphertext symbols used by every Mapping after each recognized trigger. This is a global symbol permutation: it changes the ciphertext symbol produced for each Mapping output without changing the plaintext alphabet, trigger values, or trigger order.

The permutation is one-to-one over the configured ciphertext alphabet. Every ciphertext symbol is replaced by exactly one symbol, and no two symbols are replaced by the same symbol. The same permutation is applied to the output of every Mapping, so the relationship between plaintext letters and ciphertext symbols changes across trigger intervals even when the active Mapping does not change.

The reshuffle is enabled by the optional top-level JSON setting *`symbolShuffleSeed`*. If this setting is omitted, global ciphertext-symbol reshuffling is disabled. Maplex maintains a reshuffle state initialized from this seed. After recognizing a trigger, it derives the next state deterministically from the previous state, the trigger selected by trigger progression, and the trigger occurrence count. The next state determines the next global ciphertext-symbol permutation. A repeated trigger therefore uses the resolved diagonal-progression position when deriving the next permutation. Encryption and decryption must use the same derivation and permutation order.

The trigger that causes a reshuffle is processed with the permutation that was active before it. The new permutation takes effect only for the symbols after that trigger. For example, if *`hello`* is configured as *`a_special`*, Maplex encodes *`hello`* with the current permutation, recognizes *`a_special`*, derives the next permutation, and encodes the following text with that permutation.

During decryption, Maplex first restores ciphertext order by undoing positional transformations. It then reverses the currently active symbol permutation before decoding each symbol through the active Mapping. After recovering a trigger, it performs the same trigger progression and state update as encryption, so subsequent symbols are decoded using the corresponding new permutation. The permutation must have a defined inverse, and the trigger itself must always be decoded using the permutation that was active before it.

Because the symbol permutation changes at trigger boundaries, one fixed ciphertext-symbol substitution does not describe the entire message. The transformation remains reproducible because each new permutation depends only on the starting seed and the trigger events already recovered from the message.
**### Diagonal Block Transposition**

After trigger processing and substitution, Maplex can rearrange ciphertext positions using diagonal block transposition.

The ciphertext is divided into fixed-size blocks and arranged into a grid. The symbols are then read diagonally instead of from left to right.

For example:

```
A B C D
E F G H
I J K L
M N O P
```

Reading the grid diagonally produces an order such as:

```
A
E B
I F C
M J G D
N K H
O L
P
```

The resulting order is used as the ciphertext sequence for that block.

Diagonal block transposition only changes the positions of ciphertext symbols. It does not change which symbol each plaintext letter was mapped to.

**### Reversed Diagonal Transposition**

Maplex can apply a second diagonal traversal that reads each diagonal in the opposite direction.

Using the same block:

```
A B C D
E F G H
I J K L
M N O P
```

the reversed diagonal order becomes:

```
A
B E
C F I
D G J M
H K N
L O
P
```

This produces a different positional permutation rather than simply restoring the original order.

The initial diagonal transposition and reversed diagonal transposition are separate positional transformations. They can therefore be combined to produce a more complex ordering while leaving the underlying substitution mappings unchanged.

The diagonal transformations are applied after trigger processing and substitution so that triggers always operate on the original plaintext order. During decryption, the positional transformations are reversed before the substitution mappings are decoded.

The central idea is that Maplex can change both the symbols used for plaintext and the positions in which those symbols appear, while keeping trigger progression independent from positional rearrangement.

**### Decryption Order and Reversibility**

Decryption must undo each positional transformation in the reverse order in which encryption applied it. For example, if encryption applies diagonal block transposition and then reversed diagonal transposition, decryption first undoes reversed diagonal transposition, then undoes diagonal block transposition. The reversed diagonal traversal is a separate transformation; it must not be assumed to undo the first traversal by itself.

Encryption and decryption must use the same block dimensions and the same per-trigger Mapping and Sub-mapping seeds. Each positional transformation must have a defined inverse so the ciphertext symbols return to their original order before substitution decoding begins. The handling of a final block that is shorter than the configured block size must also be defined identically for encryption and decryption.

After the positional transformations are undone, decode symbols in their restored plaintext order. Check recovered plaintext for triggers as it is decoded so trigger progression and diagonal duplicates follow the original message order. Encryption and decryption must use the same trigger order and each trigger's configured Mapping and Sub-mapping seeds.
