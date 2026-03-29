#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "emoji_scan.h"

static int TestsRun    = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

typedef struct {
  const char* name;
  bool passed;
} test_result_t;

static test_result_t results[256];
static size_t result_count = 0;

static bool sequences_equal(emoji_sequence_t* got, size_t got_n,
                            emoji_sequence_t* exp, size_t exp_n) {
  if (got_n != exp_n)
    return false;

  for (size_t i = 0; i < got_n; i++) {
    if (got[i].start != exp[i].start || got[i].end != exp[i].end)
      return false;
  }

  return true;
}

static void print_diff(emoji_sequence_t* got, size_t got_n,
                       emoji_sequence_t* exp, size_t exp_n) {
  size_t max = got_n > exp_n ? got_n : exp_n;

  for (size_t i = 0; i < max; i++) {
    if (i < got_n)
      printf("  [%zu] got: start=%zu, end=%zu\n", i, got[i].start, got[i].end);
    if (i < exp_n)
      printf("  [%zu] exp: start=%zu, end=%zu\n", i, exp[i].start, exp[i].end);
  }
}

static void test_run(const char* name,
                 size_t (*fn)(uint32_t*, size_t, emoji_sequence_t*, size_t),
                 uint32_t* cps,
                 size_t len,
                 emoji_sequence_t* exp,
                 size_t exp_n) {
  emoji_sequence_t out[8];
  size_t got_n = fn(cps, len, out, 8);

  bool ok = sequences_equal(out, got_n, exp, exp_n);

  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s\n", name);
    print_diff(out, got_n, exp, exp_n);
  }

  results[result_count].name = name;
  results[result_count].passed = ok;
  result_count++;
}

static void test_greedy(const char* name,
                        uint32_t* cps,
                        size_t len,
                        emoji_sequence_t* exp,
                        size_t exp_n) {
  test_run(name, emoji_scan_greedy, cps, len, exp, exp_n);
}

static void test_strict(const char* name,
                        uint32_t* cps,
                        size_t len,
                        emoji_sequence_t* exp,
                        size_t exp_n) {
  test_run(name, emoji_scan_strict, cps, len, exp, exp_n);
}

static void print_summary(void) {
  printf("\n");
  printf("========================================\n");
  printf("Test Summary\n");
  printf("========================================\n");
  printf("Total:  %d\n", TestsRun);
  printf("Passed: %d\n", TestsPassed);
  printf("Failed: %d\n", TestsFailed);
  printf("========================================\n");

  if (TestsFailed > 0) {
    printf("\nFailed tests:\n");
    for (size_t i = 0; i < result_count; i++) {
      if (!results[i].passed) {
        printf("  - %s\n", results[i].name);
      }
    }
  }

  printf("\n");
}

int main(void) {

  // 😀😃 - Two separate emoji
  {
    uint32_t cps[] = {0x1F600, 0x1F603};
    emoji_sequence_t exp[] = {{0, 0}, {1, 1}};
    test_greedy("Adjacent emojis [greedy]", cps, 2, exp, 2);
    test_strict("Adjacent emojis [strict]", cps, 2, exp, 2);
  }

  // 1 - ASCII digit has Emoji property (keycap base without VS or enclosing keycap is rejected)
  {
    uint32_t cps[] = {0x0031};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone keycap base [greedy]", cps, 1, exp, 0);
    test_strict("Lone keycap base [strict]", cps, 1, exp, 0);
  }

  // Keycap base + VS-15 alone (valid, no keycap term)
  {
    uint32_t cps[] = {0x0031, 0xFE0E};  // 1︎
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap base + VS-15 (no term) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap base + VS-15 (no term) [strict]", cps, 2, exp, 1);
  }

  // Keycap base + VS-16 alone (valid, no keycap term)
  {
    uint32_t cps[] = {0x0031, 0xFE0F};  // 1️
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap base + VS-16 (no term) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap base + VS-16 (no term) [strict]", cps, 2, exp, 1);
  }

  // Keycap base + VS-15 + keycap (valid)
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3};  // 1︎⃣
    emoji_sequence_t exp[] = {{0, 2}};
    test_greedy("Keycap + VS-15 + keycap term [greedy]", cps, 3, exp, 1);
    test_strict("Keycap + VS-15 + keycap term [strict]", cps, 3, exp, 1);
  }

  // Keycap base + VS-16 + keycap (valid)
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3};  // 1️⃣
    emoji_sequence_t exp[] = {{0, 2}};
    test_greedy("Keycap + VS-16 + keycap term [greedy]", cps, 3, exp, 1);
    test_strict("Keycap + VS-16 + keycap term [strict]", cps, 3, exp, 1);
  }

  // Keycap base + keycap (no VS, valid)
  {
    uint32_t cps[] = {0x0031, 0x20E3};  // 1⃣
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap + keycap term (no VS) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap + keycap term (no VS) [strict]", cps, 2, exp, 1);
  }

  // 😀︎⃣ - non-keycap emoji + VS-15 + keycap term (VS-15 is dead end, term rejected)
  {
    uint32_t cps[] = {0x1F600, 0xFE0E, 0x20E3};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 + keycap term (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-15 + keycap term (rejected) [strict]", cps, 3, exp, 1);
  }

  // 😀️⃣ - non-keycap emoji + VS-16 + keycap term (VS-16 has no keycap term transition)
  {
    uint32_t cps[] = {0x1F600, 0xFE0F, 0x20E3};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 + keycap term (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-16 + keycap term (rejected) [strict]", cps, 3, exp, 1);
  }

  // © - Copyright symbol (text-default emoji)
  {
    uint32_t cps[] = {0x00A9};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Text-default emoji © [greedy]", cps, 1, exp, 1);
    test_strict("Text-default emoji © [strict]", cps, 1, exp, 1);
  }

  // 👦🏻 - Boy with light skin tone modifier
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + modifier [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + modifier [strict]", cps, 2, exp, 1);
  }

  // 👍🏻🏽 - Thumbs up + two modifiers (only first valid)
  {
    uint32_t cps[] = {0x1F44D, 0x1F3FB, 0x1F3FD};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Double modifier [greedy]", cps, 3, exp, 1);
    test_strict("Double modifier [strict]", cps, 3, exp, 1);
  }

  // 🏻 - Lone skin tone modifier (invalid)
  {
    uint32_t cps[] = {0x1F3FB};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Modifier without base [greedy]", cps, 1, exp, 0);
    test_strict("Modifier without base [strict]", cps, 1, exp, 0);
  }

  // 👩‍🦰🏻 - Woman + ZWJ + red hair + modifier (modifier rejected)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F9B0, 0x1F3FB};
    emoji_sequence_t exp[] = {{0, 2}};
    test_greedy("Hair emoji + modifier [greedy]", cps, 4, exp, 1);
    test_strict("Hair emoji + modifier [strict]", cps, 4, exp, 1);
  }

  // 🇺🇸 - US flag (regional indicator pair)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("RI pair [greedy]", cps, 2, exp, 1);
    test_strict("RI pair [strict]", cps, 2, exp, 1);
  }

  // 🇸🇪🇳 - Sweden flag + lone regional indicator
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Odd RI count [greedy]", cps, 3, exp, 1);
    test_strict("Odd RI count [strict]", cps, 3, exp, 1);
  }

  // 🇸😀 - Lone regional indicator followed by emoji
  {
    uint32_t cps[] = {0x1F1F8, 0x1F600};
    emoji_sequence_t exp[] = {{1, 1}};
    test_greedy("RI followed by emoji [greedy]", cps, 2, exp, 1);
    test_strict("RI followed by emoji [strict]", cps, 2, exp, 1);
  }

  // ️ - Lone variation selector-16 (invalid)
  {
    uint32_t cps[] = {0xFE0F};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone VS-16 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-16 [strict]", cps, 1, exp, 0);
  }

  // ❤️️ - Heavy black heart + two VS-16 (second rejected)
  {
    uint32_t cps[] = {0x2764, 0xFE0F, 0xFE0F};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Double VS-16 [greedy]", cps, 3, exp, 1);
    test_strict("Double VS-16 [strict]", cps, 3, exp, 1);
  }

  // ✋️🏻 - Raised hand + VS-16 + modifier (invalid order)
  {
    uint32_t cps[] = {0x270B, 0xFE0F, 0x1F3FB};
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("VS-16 followed by modifier [greedy]", cps, 3, exp, 1);
    test_strict("VS-16 followed by modifier [strict]", cps, 3, exp, 1);
  }

  // 👨‍👩‍👧 - Family: man, woman, girl (ZWJ sequence)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    emoji_sequence_t exp[] = {{0, 4}};
    test_greedy("ZWJ family [greedy]", cps, 5, exp, 1);
    test_strict("ZWJ family [strict]", cps, 5, exp, 1);
  }

  // 👨‍🇸🇪 - Man + ZWJ + Sweden flag
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F1F8, 0x1F1EA};
    emoji_sequence_t exp[] = {{0, 3}};
    test_greedy("ZWJ + RI flag [greedy]", cps, 4, exp, 1);
    test_strict("ZWJ + RI flag [strict]", cps, 4, exp, 1);
  }

  // 👨‍ - Man + trailing ZWJ
  {
    uint32_t cps[] = {0x1F468, 0x200D};
    emoji_sequence_t exp_greedy[] = {{0, 0}};
    emoji_sequence_t exp_strict[] = {0};
    test_greedy("Trailing ZWJ [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Trailing ZWJ [strict]", cps, 2, exp_strict, 0);  // ❗
  }

  // 👨‍‍👩 - Man + two ZWJs + woman
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x200D, 0x1F469};
    emoji_sequence_t exp_greedy[] = {{0, 0}, {3, 3}};
    emoji_sequence_t exp_strict[] = {        {3, 3}};
    test_greedy("Double ZWJ [greedy]", cps, 4, exp_greedy, 2);
    test_strict("Double ZWJ [strict]", cps, 4, exp_strict, 1);  // ❗
  }

  // 👨‍A - Man + ZWJ + letter A (invalid target)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x0041};
    emoji_sequence_t exp_greedy[] = {{0, 0}};
    emoji_sequence_t exp_strict[] = {0};
    test_greedy("ZWJ followed by non-emoji [greedy]", cps, 3, exp_greedy, 1);
    test_strict("ZWJ followed by non-emoji [strict]", cps, 3, exp_strict, 0);  // ❗
  }

  // 🏴󠁧󠁢 - Black flag + tag chars without cancel tag
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062};
    emoji_sequence_t exp_greedy[] = {{0, 0}};
    emoji_sequence_t exp_strict[] = {0};
    test_greedy("Tag sequence without cancel [greedy]", cps, 3, exp_greedy, 1);
    test_strict("Tag sequence without cancel [strict]", cps, 3, exp_strict, 0);  // ❗
  }

  // 🏴 - Black flag + cancel tag without tag chars
  {
    uint32_t cps[] = {0x1F3F4, 0xE007F};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Cancel tag without tags [greedy]", cps, 2, exp, 1);
    test_strict("Cancel tag without tags [strict]", cps, 2, exp, 1);
  }

  // 😀👍🏻🇺🇸 - Multiple sequences (3 different types)
  {
    uint32_t cps[] = {0x1F600, 0x1F44D, 0x1F3FB, 0x1F1FA, 0x1F1F8};
    emoji_sequence_t exp[] = {{0, 0}, {1, 2}, {3, 4}};
    test_greedy("Multiple sequences (3 types) [greedy]", cps, 5, exp, 3);
    test_strict("Multiple sequences (3 types) [strict]", cps, 5, exp, 3);
  }

  // 👨‍👩‍👧‍👦 - Family with two children (long ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466};
    emoji_sequence_t exp[] = {{0, 6}};
    test_greedy("Long ZWJ sequence (4 emoji) [greedy]", cps, 7, exp, 1);
    test_strict("Long ZWJ sequence (4 emoji) [strict]", cps, 7, exp, 1);
  }

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England flag (complete tag sequence)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_sequence_t exp[] = {{0, 6}};
    test_greedy("Complete tag sequence (England flag) [greedy]", cps, 7, exp, 1);
    test_strict("Complete tag sequence (England flag) [strict]", cps, 7, exp, 1);
  }

  // Empty input
  {
    uint32_t cps[] = {0};
    emoji_sequence_t exp[] = {0};
    test_greedy("Empty input [greedy]", cps, 0, exp, 0);
    test_strict("Empty input [strict]", cps, 0, exp, 0);
  }

  // No emoji in input (just ASCII)
  {
    uint32_t cps[] = {0x0041, 0x0042, 0x0043};
    emoji_sequence_t exp[] = {0};
    test_greedy("No emoji in input [greedy]", cps, 3, exp, 0);
    test_strict("No emoji in input [strict]", cps, 3, exp, 0);
  }

  // 👨🏻‍💻 - Technologist with skin tone (ZWJ after modifier)
  {
    uint32_t cps[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_sequence_t exp[] = {{0, 3}};
    test_greedy("ZWJ after modifier (technologist) [greedy]", cps, 4, exp, 1);
    test_strict("ZWJ after modifier (technologist) [strict]", cps, 4, exp, 1);
  }

  // 😀🇸🇪🇳 - Emoji, flag pair, lone RI
  {
    uint32_t cps[] = {0x1F600, 0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_sequence_t exp[] = {{0, 0}, {1, 2}};
    test_greedy("Emoji then flag then lone RI [greedy]", cps, 4, exp, 2);
    test_strict("Emoji then flag then lone RI [strict]", cps, 4, exp, 2);
  }

  // Multiple lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF};
    emoji_sequence_t exp[] = {0};
    test_greedy("Multiple lone modifiers [greedy]", cps, 5, exp, 0);
    test_strict("Multiple lone modifiers [strict]", cps, 5, exp, 0);
  }

  // Emoji + VS-15 (terminal state, valid)
  {
    uint32_t cps[] = {0x263A, 0xFE0E};  // ☺︎
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + VS-15 [strict]", cps, 2, exp, 1);
  }

  // Emoji + VS-16 (non-terminal, valid)
  {
    uint32_t cps[] = {0x263A, 0xFE0F};  // ☺️
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + VS-16 [strict]", cps, 2, exp, 1);
  }

  // Emoji + VS-15 + ZWJ + emoji (VS-15 is terminal, ZWJ rejected)
  {
    uint32_t cps[] = {0x1F468, 0xFE0E, 0x200D, 0x1F469};  // 👨︎‍👩
    emoji_sequence_t exp[] = {{0, 1}, {3, 3}};  // ZWJ rejected
    test_greedy("Emoji + VS-15 + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("Emoji + VS-15 + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // Emoji + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};  // 👨️‍👩
    emoji_sequence_t exp[] = {{0, 3}};  // Full ZWJ sequence
    test_greedy("Emoji + VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("Emoji + VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // Emoji + VS-15 + modifier (rejected after VS-15)
  {
    uint32_t cps[] = {0x1F44B, 0xFE0E, 0x1F3FB};  // 👋︎🏻
    emoji_sequence_t exp[] = {{0, 1}};  // Just 👋︎, modifier rejected
    test_greedy("Emoji + VS-15 + modifier (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-15 + modifier (rejected) [strict]", cps, 3, exp, 1);
  }

  // Emoji + VS-16 + modifier (rejected after VS-16)
  {
    uint32_t cps[] = {0x1F44B, 0xFE0F, 0x1F3FB};  // 👋️🏻
    emoji_sequence_t exp[] = {{0, 1}};  // Just 👋️, modifier rejected
    test_greedy("Emoji + VS-16 + modifier (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-16 + modifier (rejected) [strict]", cps, 3, exp, 1);
  }

  // Emoji + modifier + VS-16 (rejected after modifier)
  {
    uint32_t cps[] = {0x1F44B, 0x1F3FB, 0xFE0F};  // 👋🏻️
    emoji_sequence_t exp[] = {{0, 1}};  // 👋🏻, VS-16 rejected after modifier
    test_greedy("Emoji + modifier + VS-16 (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + modifier + VS-16 (rejected) [strict]", cps, 3, exp, 1);
  }

  // Multiple emoji with different VS
  {
    uint32_t cps[] = {
      0x263A, 0xFE0F,  // ☺️ (VS-16)
      0x263A, 0xFE0E,  // ☺︎ (VS-15)
      0x263A           // ☺ (no VS)
    };
    emoji_sequence_t exp[] = {{0, 1}, {2, 3}, {4, 4}};
    test_greedy("Multiple emoji with different VS [greedy]", cps, 5, exp, 3);
    test_strict("Multiple emoji with different VS [strict]", cps, 5, exp, 3);
  }

  // Lone VS-15 (invalid)
  {
    uint32_t cps[] = {0xFE0E};
    emoji_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone VS-15 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-15 [strict]", cps, 1, exp, 0);
  }

  // Lone VS-16 (invalid)
  {
    uint32_t cps[] = {0xFE0F};
    emoji_sequence_t exp[] = {0};
    test_greedy("Lone VS-16 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-16 [strict]", cps, 1, exp, 0);
  }

  // Double VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0xFE0E};  // ☺︎︎
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 + VS-15 (second rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-15 + VS-15 (second rejected) [strict]", cps, 3, exp, 1);
  }

  // VS-16 followed by VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0xFE0E};  // ☺️︎
    emoji_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 + VS-15 (second rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-16 + VS-15 (second rejected) [strict]", cps, 3, exp, 1);
  }

  // ZWJ sequence with VS-16 before ZWJ
  {
    uint32_t cps[] = {
      0x1F468, 0xFE0F, 0x200D,  // 👨️‍
      0x1F469, 0xFE0F           // 👩️
    };
    emoji_sequence_t exp[] = {{0, 4}};  // Full family
    test_greedy("ZWJ sequence: man VS-16 ZWJ woman VS-16 [greedy]", cps, 5, exp, 1);
    test_strict("ZWJ sequence: man VS-16 ZWJ woman VS-16 [strict]", cps, 5, exp, 1);
  }

  // Mixed: keycap with VS-15, then emoji with VS-16
  {
    uint32_t cps[] = {
      0x0031, 0xFE0E, 0x20E3,  // 1︎⃣
      0x263A, 0xFE0F           // ☺️
    };
    emoji_sequence_t exp[] = {{0, 2}, {3, 4}};
    test_greedy("Keycap VS-15 + Emoji VS-16 [greedy]", cps, 5, exp, 2);
    test_strict("Keycap VS-15 + Emoji VS-16 [strict]", cps, 5, exp, 2);
  }

  // Text between VS-15 and VS-16 sequences
  {
    uint32_t cps[] = {
      0x263A, 0xFE0E,  // ☺︎
      0x0041,          // A
      0x263A, 0xFE0F   // ☺️
    };
    emoji_sequence_t exp[] = {{0, 1}, {3, 4}};
    test_greedy("VS-15 + text + VS-16 [greedy]", cps, 5, exp, 2);
    test_strict("VS-15 + text + VS-16 [strict]", cps, 5, exp, 2);
  }

  print_summary();

  return TestsFailed > 0 ? 1 : 0;
}
