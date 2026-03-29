#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "emoji_dfa.h"
#include "emoji_dfa_classify.h"
#include "emoji_ucd_classify.h"

static int TestsRun = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

static void test_type(const char* name,
                      uint32_t* cps,
                      size_t len,
                      emoji_sequence_type_t expected_type) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  uint32_t classes = 0, accepted_classes = 0;

  for (size_t i = 0; i < len; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(cps[i]);
    state = emoji_dfa_step_record(state, klass, &classes);
    if (emoji_dfa_is_accepting(state))
      accepted_classes = classes;
  }

  emoji_sequence_type_t got = emoji_dfa_classify_type(accepted_classes);
  bool ok = got == expected_type;

  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s (got %d, expected %d)\n", name, got, expected_type);
  }
}

static void test_style(const char* name,
                       uint32_t* cps,
                       size_t len,
                       emoji_presentation_style_t expected_style) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  uint32_t classes = 0, accepted_classes = 0;

  for (size_t i = 0; i < len; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(cps[i]);
    state = emoji_dfa_step_record(state, klass, &classes);
    if (emoji_dfa_is_accepting(state))
      accepted_classes = classes;
  }

  emoji_presentation_style_t got = emoji_dfa_classify_style(accepted_classes);
  bool ok = got == expected_style;

  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s (got %d, expected %d)\n", name, got, expected_style);
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

  // 😀 - basic emoji, no combining characters
  {
    uint32_t c[] = {0x1F600};
    test_type("BASIC: bare emoji", c, 1, EMOJI_SEQUENCE_BASIC);
  }

  // ©️ - text-default emoji with VS-16
  {
    uint32_t c[] = {0x00A9, 0xFE0F};
    test_type("BASIC: emoji + VS-16", c, 2, EMOJI_SEQUENCE_BASIC);
  }

  // ☺︎ - emoji with VS-15
  {
    uint32_t c[] = {0x263A, 0xFE0E};
    test_type("BASIC: emoji + VS-15", c, 2, EMOJI_SEQUENCE_BASIC);
  }

  // 1️⃣ - keycap with VS-16
  {
    uint32_t c[] = {0x0031, 0xFE0F, 0x20E3};
    test_type("KEYCAP: digit + VS-16 + term", c, 3, EMOJI_SEQUENCE_KEYCAP);
  }

  // 1⃣ - keycap without VS
  {
    uint32_t c[] = {0x0031, 0x20E3};
    test_type("KEYCAP: digit + term (no VS)", c, 2, EMOJI_SEQUENCE_KEYCAP);
  }

  // 🇺🇸 - US flag (RI pair)
  {
    uint32_t c[] = {0x1F1FA, 0x1F1F8};
    test_type("FLAG: RI pair", c, 2, EMOJI_SEQUENCE_FLAG);
  }

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England flag (tag sequence)
  {
    uint32_t c[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065,
                    0xE006E, 0xE0067, 0xE007F};
    test_type("TAG: England flag", c, 7, EMOJI_SEQUENCE_TAG);
  }

  // 👦🏻 - boy with light skin tone
  {
    uint32_t c[] = {0x1F466, 0x1F3FB};
    test_type("MODIFIER: base + Fitzpatrick", c, 2, EMOJI_SEQUENCE_MODIFIER);
  }

  // 👨‍👩‍👧 - family ZWJ sequence
  {
    uint32_t c[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    test_type("ZWJ: family", c, 5, EMOJI_SEQUENCE_ZWJ);
  }

  // 👨🏻‍💻 - ZWJ sequence with modifier (ZWJ wins over MODIFIER)
  {
    uint32_t c[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    test_type("ZWJ wins over MODIFIER", c, 4, EMOJI_SEQUENCE_ZWJ);
  }

  // 😀 - no variation selector
  {
    uint32_t c[] = {0x1F600};
    test_style("DEFAULT: no VS", c, 1, EMOJI_PRESENTATION_DEFAULT);
  }

  // ☺️ - VS-16 emoji presentation
  {
    uint32_t c[] = {0x263A, 0xFE0F};
    test_style("EMOJI: VS-16", c, 2, EMOJI_PRESENTATION_EMOJI);
  }

  // ☺︎ - VS-15 text presentation
  {
    uint32_t c[] = {0x263A, 0xFE0E};
    test_style("TEXT: VS-15", c, 2, EMOJI_PRESENTATION_TEXT);
  }

  // 1️⃣ - keycap with VS-16
  {
    uint32_t c[] = {0x0031, 0xFE0F, 0x20E3};
    test_style("EMOJI: keycap with VS-16", c, 3, EMOJI_PRESENTATION_EMOJI);
  }

  // 1︎⃣ - keycap with VS-15
  {
    uint32_t c[] = {0x0031, 0xFE0E, 0x20E3};
    test_style("TEXT: keycap with VS-15", c, 3, EMOJI_PRESENTATION_TEXT);
  }

  // 👨️‍👩 - ZWJ sequence with VS-16 before ZWJ
  {
    uint32_t c[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    test_style("EMOJI: ZWJ sequence with VS-16", c, 4, EMOJI_PRESENTATION_EMOJI);
  }

  // 🇺🇸 - flag has no VS
  {
    uint32_t c[] = {0x1F1FA, 0x1F1F8};
    test_style("DEFAULT: RI flag no VS", c, 2, EMOJI_PRESENTATION_DEFAULT);
  }

  print_summary();
  return TestsFailed > 0 ? 1 : 0;
}

