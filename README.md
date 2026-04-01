# c-emoji — Unicode Emoji Segmentation and Extraction

A header-only C library for Unicode emoji segmentation and extraction 
implementing [UTS #51 v17.0.0](https://www.unicode.org/reports/tr51/tr51-29.html).

The core is a DFA that classifies and segments emoji sequences in a single
O(n) pass with no heap allocation. It can be used directly for custom
processing pipelines, or through the higher-level scanner API that extracts
sequence position ranges from a codepoint array.

Detection is structural, not semantic. The DFA identifies sequences by shape
— a modifier after a modifier base, a pair of regional indicators, a ZWJ
chain — but does not validate whether a given ZWJ combination has a defined
rendering in any font or platform. See [Semantic validation](#semantic-validation)
below.

## Usage

Input must be an array of Unicode codepoints. UTF-8 or UTF-16 strings must 
be decoded first.

```c
#include "emoji_scan.h"

uint32_t text[] = { 0x1F468, 0x1F3FB, 0x200D, 0x1F4BB }; // 👨🏻‍💻
emoji_scan_sequence_t seqs[16];

size_t count = emoji_scan_greedy(text, 4, seqs, 16);

for (size_t i = 0; i < count; i++) {
  printf("emoji at [%zu, %zu]\n", seqs[i].start, seqs[i].end);
}
// emoji at [0, 3]
```

`start` and `end` are inclusive indices into the codepoint array.

## Strict vs greedy

Two scanning modes handle incomplete sequences differently.

`emoji_scan_strict()` only emits a sequence when the codepoint that follows
it cleanly terminates it. A trailing ZWJ with no following emoji, or a tag
sequence without a cancel tag, is silently dropped.

`emoji_scan_greedy()` tracks the longest accepted prefix and emits it
whenever a boundary is hit, even if non-emoji codepoints appeared after the
last valid position. Incomplete sequences at end of input are emitted up to
the last accepting state.

Where the two functions differ:

| Input                            | strict  | greedy        |
|----------------------------------|---------|---------------|
| `👨 ZWJ` (trailing ZWJ)          | ∅       | `{0,0}`       |
| `👨 ZWJ ZWJ 👩` (double ZWJ)     | `{3,3}` | `{0,0} {3,3}` |
| `👨 ZWJ A` (invalid ZWJ target)  | ∅       | `{0,0}`       |
| `🏴 gb eng` (tag without cancel) | ∅       | `{0,0}`       |

## Using the DFA directly

For custom pipelines — streaming parsers, renderers, text editors — use the
DFA layer directly instead of the scanner API. This gives full control over
how boundaries are handled and avoids the overhead of building an output array.
`emoji_scan.h` is a complete working example of both strict and greedy
strategies built on the DFA.

### Character classification

Before feeding a codepoint to the DFA it must be mapped to a character class
via `emoji_ucd_classify()`. This applies the Unicode property predicates from
`emoji_ucd.h` in specificity order — keycap bases, modifiers, and regional
indicators all carry the `Emoji` property but must be classified as their own
distinct classes, or the DFA will produce wrong transitions.

### Stepping the DFA

`emoji_dfa_step()` takes a state and a class and returns the next state.
`emoji_dfa_step_record()` does the same but additionally accumulates a bitmask
of all character classes seen during the current attempt. Classes are only
recorded when the transition produces a meaningful state — codepoints that
loop back to START or produce REJECT do not set bits.

### States and boundaries

Every state belongs to one of three categories. Accepting states mean a
complete sequence ends here but may still be extended by further input.
Pending states mean the machine is inside a valid prefix with no complete
sequence yet. A boundary state (`REJECT`) means the current codepoint had
no valid transition and the current attempt is over. The full list of states
is in `emoji_dfa.h`.

Note that `emoji_dfa_is_boundary()` being true is not the same as
`emoji_dfa_is_accepting()` being false — pending states are neither.

### Handling a boundary

When a boundary is reached, the current attempt is over. What you do next
depends on your strategy: emit only if the previous state was accepting
(strict), or emit the longest accepted prefix seen so far (greedy). Either
way, the boundary codepoint must be retried from START since it may open the
next sequence. If the retry also lands on START — meaning the codepoint cannot
open any sequence, such as a lone ZWJ or a bare tag character — it must be
skipped entirely so it is not included in the next sequence's range.

### Snapshotting accepted classes

The class bitmask accumulates across the entire current attempt, including
any codepoints seen after the last accepting state. Snapshot it each time an
accepting state is reached — the snapshot reflects the accepted sequence only,
not any trailing codepoints before the boundary. Pass the snapshot to
`emoji_dfa_classify_type()` and `emoji_dfa_classify_style()` to determine
sequence type and presentation style without rescanning.

## Sequence types

Sequence types follow the Unicode taxonomy:

| Type                      | Example   | Description                          |
|---------------------------|-----------|--------------------------------------|
| `EMOJI_SEQUENCE_BASIC`    | `😀` `©️` | Single codepoint, optional VS        |
| `EMOJI_SEQUENCE_KEYCAP`   | `1️⃣`      | Digit/`#`/`*` + optional VS + U+20E3 |
| `EMOJI_SEQUENCE_FLAG`     | `🇺🇸`      | Regional indicator pair              |
| `EMOJI_SEQUENCE_TAG`      | `🏴󠁧󠁢󠁥󠁮󠁧󠁿`      | Base emoji + tag chars + cancel tag  |
| `EMOJI_SEQUENCE_MODIFIER` | `👦🏻`      | Modifier base + Fitzpatrick modifier |
| `EMOJI_SEQUENCE_ZWJ`      | `👨‍👩‍👧`      | Two or more emoji joined by U+200D   |

Presentation style:

| Style                        | Meaning                                       |
|------------------------------|-----------------------------------------------|
| `EMOJI_PRESENTATION_EMOJI`   | VS-16 (U+FE0F) present                        |
| `EMOJI_PRESENTATION_TEXT`    | VS-15 (U+FE0E) present                        |
| `EMOJI_PRESENTATION_DEFAULT` | No VS — consult `emoji_ucd_is_presentation()` |

## Semantic validation

The scanner accepts all structurally valid sequences. Callers requiring
stricter conformance should validate emitted sequences as needed:

- **ZWJ sequences** — validate against `emoji-zwj-sequences.txt` to confirm
  the combination has a defined RGI rendering.
- **Tag sequences** — validate tag characters against the subdivision codes
  in `emoji-sequences.txt`.
- **Presentation** — for `EMOJI_PRESENTATION_DEFAULT`, call
  `emoji_ucd_is_presentation()` to determine whether the codepoint renders
  as emoji or text by default.

## Example: emoji segmentation

```c
void segment(const uint32_t *codepoints, size_t len) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  size_t start = 0;

  for (size_t i = 0; i < len; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(codepoints[i]);
    emoji_dfa_state_t next  = emoji_dfa_step(state, klass);

    if (emoji_dfa_is_boundary(next)) {
      if (emoji_dfa_is_accepting(state))
        printf("EMOJI [%zu, %zu)\n", start, i);
      next  = emoji_dfa_step(EMOJI_DFA_STATE_START, klass);
      start = i;
    }

    state = next;
    if (emoji_dfa_is_start(state))
      start = i + 1;
  }

  if (start < len) {
    if (emoji_dfa_is_accepting(state))
      printf("EMOJI [%zu, %zu)\n", start, len);
  }
}
```

Ranges are half-open `[start, end)` — `end` is the index of the first
codepoint after the span.

## File overview

| File                   | Purpose |
|------------------------|---------|
| `emoji_scan.h`         | Scanner implementation — `emoji_scan_strict()`, `emoji_scan_greedy()`, `emoji_scan_sequence_t` |
| `emoji_class.h`        | Result types — `emoji_sequence_type_t`, `emoji_presentation_style_t` |
| `emoji_dfa.h`          | DFA core — state machine, transition table, `emoji_dfa_step()`, `emoji_dfa_step_record()` |
| `emoji_dfa_classify.h` | Post-scan classification from recorded class bitmask |
| `emoji_ucd.h`          | Unicode property tries — `Emoji`, `Emoji_Modifier_Base`, `Emoji_Presentation` |
| `emoji_ucd_classify.h` | Maps codepoints to DFA character classes |


## Deviations from UTS #51
 
This implementation intentionally deviates from the UTS #51 grammar in
the following ways, preferring RGI practice over the broader spec grammar:
 
**Keycap sequences as ZWJ elements** — UTS #51 defines `emoji_keycap_sequence`
a valid `emoji_core_sequence` and therefore a valid `emoji_zwj_element`, This 
implementation does not accept keycap sequences as ZWJ elements. No 
keycap-based ZWJ sequences appear in `emoji-zwj-sequences.txt`.

**Flag sequences as ZWJ elements** — UTS #51 defines `emoji_flag_sequence` as 
a valid `emoji_core_sequence` and therefore a valid `emoji_zwj_element`. This 
implementation does not accept flag pairs as components of ZWJ sequences. 
No such combinations appear in `emoji-zwj-sequences.txt`.

**Tag sequences as ZWJ elements** — UTS #51 defines `emoji_tag_sequence` as 
a valid `emoji_zwj_element`, allowing tag sequences to appear as components 
in ZWJ sequences. This implementation does not accept tag sequences as ZWJ 
elements. No tag-based ZWJ sequences appear in `emoji-zwj-sequences.txt`.

**Tag sequences restricted to U+1F3F4** — UTS #51 allows any `emoji_character`, 
`emoji_modifier_sequence`, or `emoji_presentation_sequence` as a tag base. 
This implementation only accepts U+1F3F4 WAVING BLACK FLAG as a tag base, 
matching the only tag sequences defined in `emoji-sequences.txt` in Unicode 
17.0.0 (`gbeng`, `gbsct`, `gbwls`).

**Keycap bases not accepted as bare emoji** — The digits 0–9, `#`, and `*`
carry the `Emoji` property in Unicode and would therefore be accepted as
standalone emoji sequences under the UTS #51 grammar.  This implementation
only accepts them when followed by an optional variation selector (VS-15 or
VS-16) and/or U+20E3 COMBINING ENCLOSING KEYCAP.  A bare digit, `#`, or `*`
with no following VS or enclosing keycap is never emitted as a sequence.
This matches RGI practice: no bare keycap base appears in
`emoji-sequences.txt`.

## Requirements

C99 or later

## Unicode version

Unicode 17.0.0.

## License

MIT License. Copyright (c) 2026 Christian Hansen.

## See Also

- [Emoji Segmenter](https://github.com/google/emoji-segmenter) by Google

