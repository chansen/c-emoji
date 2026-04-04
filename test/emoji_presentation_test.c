#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "emoji_dfa.h"
#include "emoji_dfa_classify.h"
#include "emoji_ucd_classify.h"
#include "emoji_presentation.h"

static int TestsRun    = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

static void test(const char*                name,
                 uint32_t*                  cps,
                 size_t                     len,
                 emoji_presentation_style_t expected) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  uint32_t recorded_bitmask = 0;

  for (size_t i = 0; i < len; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(cps[i]);
    state = emoji_dfa_step_record(state, klass, &recorded_bitmask);
  }

  emoji_presentation_style_t style = emoji_dfa_classify_style(recorded_bitmask);
  emoji_sequence_type_t      type  = emoji_dfa_classify_type(recorded_bitmask);
  emoji_presentation_style_t got   = emoji_presentation_resolve(type, style, cps[0]);

  bool ok = got == expected;
  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s (got %d, expected %d)\n", name, got, expected);
  }
}

static void print_summary(void) {
  printf("\n========================================\n");
  printf("Test Summary\n");
  printf("========================================\n");
  printf("Total:  %d\n", TestsRun);
  printf("Passed: %d\n", TestsPassed);
  printf("Failed: %d\n", TestsFailed);
  printf("========================================\n\n");
}

int main(void) {

  // BASIC: emoji-default codepoints (Emoji_Presentation=Yes)
  {
    uint32_t c[] = {0x1F600};
    test("BASIC: 😀 emoji-default no VS", c, 1, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F3FF};
    test("BASIC: 🏿 Fitzpatrick modifier no VS", c, 1, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F1F8};
    test("BASIC: 🇸 lone RI no VS", c, 1, EMOJI_PRESENTATION_EMOJI);
  }

  // BASIC: text-default codepoints (Emoji_Presentation=No)
  {
    uint32_t c[] = {0x00A9};
    test("BASIC: © text-default no VS", c, 1, EMOJI_PRESENTATION_TEXT);
  }
  {
    uint32_t c[] = {0x203C};
    test("BASIC: ‼ text-default no VS", c, 1, EMOJI_PRESENTATION_TEXT);
  }

  // BASIC: VS-16 overrides Emoji_Presentation
  {
    uint32_t c[] = {0x00A9, 0xFE0F};
    test("BASIC: ©️ text-default + VS-16", c, 2, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x263A, 0xFE0F};
    test("BASIC: ☺️ emoji-default + VS-16", c, 2, EMOJI_PRESENTATION_EMOJI);
  }

  // BASIC: VS-15 overrides Emoji_Presentation
  {
    uint32_t c[] = {0x1F600, 0xFE0E};
    test("BASIC: 😀︎ emoji-default + VS-15", c, 2, EMOJI_PRESENTATION_TEXT);
  }
  {
    uint32_t c[] = {0x263A, 0xFE0E};
    test("BASIC: ☺︎ emoji-default + VS-15", c, 2, EMOJI_PRESENTATION_TEXT);
  }

  // KEYCAP: no VS -> text presentation
  {
    uint32_t c[] = {0x0031, 0x20E3};
    test("KEYCAP: 1⃣ no VS", c, 2, EMOJI_PRESENTATION_TEXT);
  }

  // KEYCAP: VS-16 -> emoji presentation
  {
    uint32_t c[] = {0x0031, 0xFE0F, 0x20E3};
    test("KEYCAP: 1️⃣ VS-16", c, 3, EMOJI_PRESENTATION_EMOJI);
  }

  // KEYCAP: VS-15 -> text presentation
  {
    uint32_t c[] = {0x0031, 0xFE0E, 0x20E3};
    test("KEYCAP: 1︎⃣ VS-15", c, 3, EMOJI_PRESENTATION_TEXT);
  }

  // FLAG: always emoji presentation
  {
    uint32_t c[] = {0x1F1FA, 0x1F1F8};
    test("FLAG: 🇺🇸 no VS", c, 2, EMOJI_PRESENTATION_EMOJI);
  }

  // TAG: always emoji presentation
  {
    uint32_t c[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    test("TAG: 🏴󠁧󠁢󠁥󠁮󠁧󠁿 England flag no VS", c, 7, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F3F4};
    test("TAG: 🏴 bare tag base no VS", c, 1, EMOJI_PRESENTATION_EMOJI);
  }

  // MODIFIER: always emoji presentation
  {
    uint32_t c[] = {0x1F466, 0x1F3FB};
    test("MODIFIER: 👦🏻 no VS", c, 2, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F44B, 0x1F3FD};
    test("MODIFIER: 👋🏽 no VS", c, 2, EMOJI_PRESENTATION_EMOJI);
  }

  // ZWJ: always emoji presentation
  {
    uint32_t c[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    test("ZWJ: 👨‍👩‍👧 family no VS", c, 5, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    test("ZWJ: 👨️‍👩 with VS-16", c, 4, EMOJI_PRESENTATION_EMOJI);
  }
  {
    uint32_t c[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    test("ZWJ: 👨🏻‍💻 technologist no VS", c, 4, EMOJI_PRESENTATION_EMOJI);
  }

  print_summary();
  return TestsFailed > 0 ? 1 : 0;
}
