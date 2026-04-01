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

static bool sequences_equal(emoji_scan_sequence_t* got, size_t got_n,
                            emoji_scan_sequence_t* exp, size_t exp_n) {
  if (got_n != exp_n)
    return false;

  for (size_t i = 0; i < got_n; i++) {
    if (got[i].start != exp[i].start || got[i].end != exp[i].end)
      return false;
  }

  return true;
}

static void print_diff(emoji_scan_sequence_t* got, size_t got_n,
                       emoji_scan_sequence_t* exp, size_t exp_n) {
  size_t max = got_n > exp_n ? got_n : exp_n;

  for (size_t i = 0; i < max; i++) {
    if (i < got_n)
      printf("  [%zu] got: start=%zu, end=%zu\n", i, got[i].start, got[i].end);
    if (i < exp_n)
      printf("  [%zu] exp: start=%zu, end=%zu\n", i, exp[i].start, exp[i].end);
  }
}

static void test_run(const char* name,
                 size_t (*fn)(const uint32_t*, size_t, emoji_scan_sequence_t*, size_t),
                 uint32_t* cps,
                 size_t len,
                 emoji_scan_sequence_t* exp,
                 size_t exp_n) {
  emoji_scan_sequence_t out[8];
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
                        emoji_scan_sequence_t* exp,
                        size_t exp_n) {
  test_run(name, emoji_scan_greedy, cps, len, exp, exp_n);
}

static void test_strict(const char* name,
                        uint32_t* cps,
                        size_t len,
                        emoji_scan_sequence_t* exp,
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
    emoji_scan_sequence_t exp[] = {{0, 0}, {1, 1}};
    test_greedy("Adjacent emojis [greedy]", cps, 2, exp, 2);
    test_strict("Adjacent emojis [strict]", cps, 2, exp, 2);
  }

  // 1 - ASCII digit has Emoji property (keycap base without VS or enclosing keycap is rejected)
  {
    uint32_t cps[] = {0x0031};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone keycap base [greedy]", cps, 1, exp, 0);
    test_strict("Lone keycap base [strict]", cps, 1, exp, 0);
  }

  // Keycap base + VS-15 alone (valid, no keycap term)
  {
    uint32_t cps[] = {0x0031, 0xFE0E};  // 1︎
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap base + VS-15 (no term) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap base + VS-15 (no term) [strict]", cps, 2, exp, 1);
  }

  // Keycap base + VS-16 alone (valid, no keycap term)
  {
    uint32_t cps[] = {0x0031, 0xFE0F};  // 1️
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap base + VS-16 (no term) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap base + VS-16 (no term) [strict]", cps, 2, exp, 1);
  }

  // Keycap base + VS-15 + keycap (valid)
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3};  // 1︎⃣
    emoji_scan_sequence_t exp[] = {{0, 2}};
    test_greedy("Keycap + VS-15 + keycap term [greedy]", cps, 3, exp, 1);
    test_strict("Keycap + VS-15 + keycap term [strict]", cps, 3, exp, 1);
  }

  // Keycap base + VS-16 + keycap (valid)
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3};  // 1️⃣
    emoji_scan_sequence_t exp[] = {{0, 2}};
    test_greedy("Keycap + VS-16 + keycap term [greedy]", cps, 3, exp, 1);
    test_strict("Keycap + VS-16 + keycap term [strict]", cps, 3, exp, 1);
  }

  // Keycap base + keycap (no VS, valid)
  {
    uint32_t cps[] = {0x0031, 0x20E3};  // 1⃣
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Keycap + keycap term (no VS) [greedy]", cps, 2, exp, 1);
    test_strict("Keycap + keycap term (no VS) [strict]", cps, 2, exp, 1);
  }

  // 😀︎⃣ - non-keycap emoji + VS-15 + keycap term (VS-15 is dead end, term rejected)
  {
    uint32_t cps[] = {0x1F600, 0xFE0E, 0x20E3};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 + keycap term (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-15 + keycap term (rejected) [strict]", cps, 3, exp, 1);
  }

  // 😀️⃣ - non-keycap emoji + VS-16 + keycap term (VS-16 has no keycap term transition)
  {
    uint32_t cps[] = {0x1F600, 0xFE0F, 0x20E3};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 + keycap term (rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-16 + keycap term (rejected) [strict]", cps, 3, exp, 1);
  }

  // © - Copyright symbol (text-default emoji)
  {
    uint32_t cps[] = {0x00A9};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Text-default emoji © [greedy]", cps, 1, exp, 1);
    test_strict("Text-default emoji © [strict]", cps, 1, exp, 1);
  }

  // 👦🏻 - Boy with light skin tone modifier
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + modifier [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + modifier [strict]", cps, 2, exp, 1);
  }

  // 👍🏻🏽 - Thumbs up + two modifiers
  {
    uint32_t cps[] = {0x1F44D, 0x1F3FB, 0x1F3FD};
    emoji_scan_sequence_t exp[] = {{0, 1}, {2, 2}};
    test_greedy("Double modifier [greedy]", cps, 3, exp, 2);
    test_strict("Double modifier [strict]", cps, 3, exp, 2);
  }

  // 🏻 - Lone skin tone modifier
  {
    uint32_t cps[] = {0x1F3FB};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Modifier without base [greedy]", cps, 1, exp, 1);
    test_strict("Modifier without base [strict]", cps, 1, exp, 1);
  }


  // 🏻️ - Lone modifier + VS-16
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0F};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Lone modifier + VS-16 [greedy]", cps, 2, exp, 1);
    test_strict("Lone modifier + VS-16 [strict]", cps, 2, exp, 1);
  }

  // 🏻︎ - Lone modifier + VS-15
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0E};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Lone modifier + VS-15 [greedy]", cps, 2, exp, 1);
    test_strict("Lone modifier + VS-15 [strict]", cps, 2, exp, 1);
  }

  // 👩‍🦰🏻 - Woman + ZWJ + red hair + modifier
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F9B0, 0x1F3FB};
    emoji_scan_sequence_t exp[] = {{0, 2}, {3, 3}};
    test_greedy("Hair emoji + modifier [greedy]", cps, 4, exp, 2);
    test_strict("Hair emoji + modifier [strict]", cps, 4, exp, 2);
  }

  // 🏻🏼 - Two lone modifiers (each standalone)
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC};
    emoji_scan_sequence_t exp[] = {{0, 0}, {1, 1}};
    test_greedy("Two lone modifiers [greedy]", cps, 2, exp, 2);
    test_strict("Two lone modifiers [strict]", cps, 2, exp, 2);
  }

  // 🇺🇸 - US flag (regional indicator pair)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("RI pair [greedy]", cps, 2, exp, 1);
    test_strict("RI pair [strict]", cps, 2, exp, 1);
  }

  // 🇸 - Lone regional indicator
  {
    uint32_t cps[] = {0x1F1F8};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone RI [greedy]", cps, 1, exp, 1);
    test_strict("Lone RI [strict]", cps, 1, exp, 1);
  }

  // 🇸️ - Lone RI + VS-16
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Lone RI + VS-16 [greedy]", cps, 2, exp, 1);
    test_strict("Lone RI + VS-16 [strict]", cps, 2, exp, 1);
  }

  // 🇸︎ - Lone RI + VS-15
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0E};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Lone RI + VS-15 [greedy]", cps, 2, exp, 1);
    test_strict("Lone RI + VS-15 [strict]", cps, 2, exp, 1);
  }

  // 🇸‍👩 - Lone RI + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F1F8, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 2}};
    test_greedy("Lone RI + ZWJ + emoji [greedy]", cps, 3, exp, 1);
    test_strict("Lone RI + ZWJ + emoji [strict]", cps, 3, exp, 1);
  }

  // 🇸️‍👩 - Lone RI + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("Lone RI + VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("Lone RI + VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // 🇺🇸‍👩 - RI pair + ZWJ + emoji (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("RI pair + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("RI pair + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // 🇸🇪🇳 - Sweden flag + lone regional indicator
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_scan_sequence_t exp[] = {{0, 1}, {2, 2}};
    test_greedy("Odd RI count [greedy]", cps, 3, exp, 2);
    test_strict("Odd RI count [strict]", cps, 3, exp, 2);
  }

  // 🇸😀 - Lone regional indicator followed by emoji
  {
    uint32_t cps[] = {0x1F1F8, 0x1F600};
    emoji_scan_sequence_t exp[] = {{0, 0}, {1, 1}};
    test_greedy("RI followed by emoji [greedy]", cps, 2, exp, 2);
    test_strict("RI followed by emoji [strict]", cps, 2, exp, 2);
  }

  // ️ - Lone variation selector-16 (invalid)
  {
    uint32_t cps[] = {0xFE0F};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone VS-16 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-16 [strict]", cps, 1, exp, 0);
  }

  // ❤️️ - Heavy black heart + two VS-16 (second rejected)
  {
    uint32_t cps[] = {0x2764, 0xFE0F, 0xFE0F};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Double VS-16 [greedy]", cps, 3, exp, 1);
    test_strict("Double VS-16 [strict]", cps, 3, exp, 1);
  }

  // ✋️🏻 - Raised hand + VS-16 + modifier
  {
    uint32_t cps[] = {0x270B, 0xFE0F, 0x1F3FB};
    emoji_scan_sequence_t exp[] = {{0, 1}, {2, 2}};
    test_greedy("VS-16 followed by modifier [greedy]", cps, 3, exp, 2);
    test_strict("VS-16 followed by modifier [strict]", cps, 3, exp, 2);
  }

  // 👨‍👩‍👧 - Family: man, woman, girl (ZWJ sequence)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    emoji_scan_sequence_t exp[] = {{0, 4}};
    test_greedy("ZWJ family [greedy]", cps, 5, exp, 1);
    test_strict("ZWJ family [strict]", cps, 5, exp, 1);
  }

  // 👨‍🇸🇪 - Man + ZWJ + Sweden flag (RI not valid in ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F1F8, 0x1F1EA};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}, {2, 3}};
    emoji_scan_sequence_t exp_strict[] = {        {2, 3}};
    test_greedy("ZWJ + RI flag [greedy]", cps, 4, exp_greedy, 2);
    test_strict("ZWJ + RI flag [strict]", cps, 4, exp_strict, 1);  // ❗
  }

  // 🇸🇪‍👨 - Sweden flag + ZWJ + Man (RI not valid in ZWJ)
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x200D, 0x1F468};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("RI flag + ZWJ [greedy]", cps, 4, exp, 2);
    test_strict("RI flag + ZWJ [strict]", cps, 4, exp, 2);
  }

  // 👨‍ - Man + trailing ZWJ
  {
    uint32_t cps[] = {0x1F468, 0x200D};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}};
    emoji_scan_sequence_t exp_strict[] = {0};
    test_greedy("Trailing ZWJ [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Trailing ZWJ [strict]", cps, 2, exp_strict, 0);  // ❗
  }

  // 👨‍‍👩 - Man + two ZWJs + woman
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}, {3, 3}};
    emoji_scan_sequence_t exp_strict[] = {        {3, 3}};
    test_greedy("Double ZWJ [greedy]", cps, 4, exp_greedy, 2);
    test_strict("Double ZWJ [strict]", cps, 4, exp_strict, 1);  // ❗
  }

  // 👨‍A - Man + ZWJ + letter A (invalid target)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x0041};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}};
    emoji_scan_sequence_t exp_strict[] = {0};
    test_greedy("ZWJ followed by non-emoji [greedy]", cps, 3, exp_greedy, 1);
    test_strict("ZWJ followed by non-emoji [strict]", cps, 3, exp_strict, 0);  // ❗
  }

  // 🏴󠁧󠁢 - Black flag + tag chars without cancel tag
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}};
    emoji_scan_sequence_t exp_strict[] = {0};
    test_greedy("Tag sequence without cancel [greedy]", cps, 3, exp_greedy, 1);
    test_strict("Tag sequence without cancel [strict]", cps, 3, exp_strict, 0);  // ❗
  }

  // 🏴 - Black flag + cancel tag without tag chars
  {
    uint32_t cps[] = {0x1F3F4, 0xE007F};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}};
    emoji_scan_sequence_t exp_strict[] = {0};
    test_greedy("Cancel tag without tags [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Cancel tag without tags [strict]", cps, 2, exp_strict, 0); // ❗
  }

  // 😀👍🏻🇺🇸 - Multiple sequences (3 different types)
  {
    uint32_t cps[] = {0x1F600, 0x1F44D, 0x1F3FB, 0x1F1FA, 0x1F1F8};
    emoji_scan_sequence_t exp[] = {{0, 0}, {1, 2}, {3, 4}};
    test_greedy("Multiple sequences (3 types) [greedy]", cps, 5, exp, 3);
    test_strict("Multiple sequences (3 types) [strict]", cps, 5, exp, 3);
  }

  // 👨‍👩‍👧‍👦 - Family with two children (long ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466};
    emoji_scan_sequence_t exp[] = {{0, 6}};
    test_greedy("Long ZWJ sequence (4 emoji) [greedy]", cps, 7, exp, 1);
    test_strict("Long ZWJ sequence (4 emoji) [strict]", cps, 7, exp, 1);
  }

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England flag (complete tag sequence)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_scan_sequence_t exp[] = {{0, 6}};
    test_greedy("Complete tag sequence (England flag) [greedy]", cps, 7, exp, 1);
    test_strict("Complete tag sequence (England flag) [strict]", cps, 7, exp, 1);
  }

  // 🏴‍😀 - TAG_BASE + ZWJ + emoji (tag base as ZWJ element)
  {
    uint32_t cps[] = {0x1F3F4, 0x200D, 0x1F600};
    emoji_scan_sequence_t exp[] = {{0, 2}};
    test_greedy("TAG_BASE + ZWJ + emoji [greedy]", cps, 3, exp, 1);
    test_strict("TAG_BASE + ZWJ + emoji [strict]", cps, 3, exp, 1);
  }

  // 🏴️‍😀 - TAG_BASE + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0x200D, 0x1F600};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("TAG_BASE + VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("TAG_BASE + VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // 🏴︎ - TAG_BASE + VS-15
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0E};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("TAG_BASE + VS-15 [greedy]", cps, 2, exp, 1);
    test_strict("TAG_BASE + VS-15 [strict]", cps, 2, exp, 1);
  }

  // 🏴︎ - TAG_BASE + VS-16 + tag chars + cancel
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("TAG_BASE + VS-16 + tag chars (tag rejected) [greedy]", cps, 5, exp, 1);
    test_strict("TAG_BASE + VS-16 + tag chars (tag rejected) [strict]", cps, 5, exp, 1);
  }

  // 😀󠁧󠁢󠁿 - regular emoji + tag chars + cancel
  {
    uint32_t cps[] = {0x1F600, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Emoji + tag chars (rejected) [greedy]", cps, 4, exp, 1);
    test_strict("Emoji + tag chars (rejected) [strict]", cps, 4, exp, 1);
  }

  // 😀󠁿 - regular emoji + cancel tag
  {
    uint32_t cps[] = {0x1F600, 0xE007F};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Emoji + cancel tag (rejected) [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + cancel tag (rejected) [strict]", cps, 2, exp, 1);
  }

  // ☺︎‍👩 - VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("VS-15 + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("VS-15 + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // 🇺🇸‍👩 - RI pair + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("RI pair + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("RI pair + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // 👩‍🇺🇸 - emoji + ZWJ, RI pair (RI pair has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F1FA, 0x1F1F8};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}, {2, 3}};
    emoji_scan_sequence_t exp_strict[] = {        {2, 3}};
    test_greedy("ZWJ (rejected) + RI pair [greedy]", cps, 4, exp_greedy, 2);
    test_strict("ZWJ (rejected) + RI pair [strict]", cps, 4, exp_strict, 1);  // ❗
  }

  // 1⃣‍👩 - keycap + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x0031, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("Keycap + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("Keycap + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // 1︎⃣‍👩 - VS-15 keycap + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 2}, {4, 4}};
    test_greedy("VS-15 Keycap + ZWJ (rejected) [greedy]", cps, 5, exp, 2);
    test_strict("VS-15 Keycap + ZWJ (rejected) [strict]", cps, 5, exp, 2);
  }

  // 1️⃣‍👩 - VS-16 keycap + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 2}, {4, 4}};
    test_greedy("VS-16 Keycap + ZWJ (rejected) [greedy]", cps, 5, exp, 2);
    test_strict("VS-16 Keycap + ZWJ (rejected) [strict]", cps, 5, exp, 2);
  }

  // 👩‍1️⃣ - emoji + ZWJ + keycap (keycap has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x0031, 0xFE0F, 0x20E3};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}, {2, 4}};
    emoji_scan_sequence_t exp_strict[] = {        {2, 4}};
    test_greedy("ZWJ + Keycap (rejected) [greedy]", cps, 5, exp_greedy, 2);
    test_strict("ZWJ + Keycap (rejected) [strict]", cps, 5, exp_strict, 1); // ❗
  }

  // 🏴󠁧󠁢󠁿‍👩 - tag sequence + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE007F, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 3}, {5, 5}};
    test_greedy("Tag sequence + ZWJ (rejected) [greedy]", cps, 6, exp, 2);
    test_strict("Tag sequence + ZWJ (rejected) [strict]", cps, 6, exp, 2);
  }

  // 👩‍🏴󠁧󠁢 - emoji + ZWJ + tag sequence (tag sequence has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F3F4, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_sequence_t exp_greedy[] = {{0, 0}, {2, 5}};
    emoji_scan_sequence_t exp_strict[] = {        {2, 5}};
    test_greedy("ZWJ + Tag sequence (rejected) [greedy]", cps, 6, exp_greedy, 2);
    test_strict("ZWJ + Tag sequence (rejected) [strict]", cps, 6, exp_strict, 1); // ❗
  }

  // 👋︎‍👩 - modifier_base + VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F466, 0xFE0E, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};
    test_greedy("Modifier_base + VS-15 + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("Modifier_base + VS-15 + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // ☺️‍👩 - VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // 👦🏻‍💻 - modifier + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("Modifier + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("Modifier + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // 👨️‍👩 - modifier_base + VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("Modifier_base + VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("Modifier_base + VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // Empty input
  {
    uint32_t cps[] = {0};
    emoji_scan_sequence_t exp[] = {0};
    test_greedy("Empty input [greedy]", cps, 0, exp, 0);
    test_strict("Empty input [strict]", cps, 0, exp, 0);
  }

  // No emoji in input (just ASCII)
  {
    uint32_t cps[] = {0x0041, 0x0042, 0x0043};
    emoji_scan_sequence_t exp[] = {0};
    test_greedy("No emoji in input [greedy]", cps, 3, exp, 0);
    test_strict("No emoji in input [strict]", cps, 3, exp, 0);
  }

  // 👨🏻‍💻 - Technologist with skin tone (ZWJ after modifier)
  {
    uint32_t cps[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_scan_sequence_t exp[] = {{0, 3}};
    test_greedy("ZWJ after modifier (technologist) [greedy]", cps, 4, exp, 1);
    test_strict("ZWJ after modifier (technologist) [strict]", cps, 4, exp, 1);
  }

  // 😀🇸🇪🇳 - Emoji, flag pair, lone RI
  {
    uint32_t cps[] = {0x1F600, 0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_scan_sequence_t exp[] = {{0, 0}, {1, 2}, {3, 3}};
    test_greedy("Emoji then flag then lone RI [greedy]", cps, 4, exp, 3);
    test_strict("Emoji then flag then lone RI [strict]", cps, 4, exp, 3);
  }

  // Multiple lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF}; // 🏻🏼🏽🏾🏿
    emoji_scan_sequence_t exp[] = {{0,0}, {1,1}, {2,2}, {3,3}, {4,4}};
    test_greedy("Multiple lone modifiers [greedy]", cps, 5, exp, 5);
    test_strict("Multiple lone modifiers [strict]", cps, 5, exp, 5);
  }

  // Emoji + VS-15 (terminal state, valid)
  {
    uint32_t cps[] = {0x263A, 0xFE0E};  // ☺︎
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + VS-15 [strict]", cps, 2, exp, 1);
  }

  // Emoji + VS-16 (non-terminal, valid)
  {
    uint32_t cps[] = {0x263A, 0xFE0F};  // ☺️
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 [greedy]", cps, 2, exp, 1);
    test_strict("Emoji + VS-16 [strict]", cps, 2, exp, 1);
  }

  // Emoji + VS-15 + ZWJ + emoji (VS-15 is terminal, ZWJ rejected)
  {
    uint32_t cps[] = {0x1F468, 0xFE0E, 0x200D, 0x1F469};  // 👨︎‍👩
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 3}};  // ZWJ rejected
    test_greedy("Emoji + VS-15 + ZWJ (rejected) [greedy]", cps, 4, exp, 2);
    test_strict("Emoji + VS-15 + ZWJ (rejected) [strict]", cps, 4, exp, 2);
  }

  // Emoji + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};  // 👨️‍👩
    emoji_scan_sequence_t exp[] = {{0, 3}};  // Full ZWJ sequence
    test_greedy("Emoji + VS-16 + ZWJ + emoji [greedy]", cps, 4, exp, 1);
    test_strict("Emoji + VS-16 + ZWJ + emoji [strict]", cps, 4, exp, 1);
  }

  // Emoji + VS-15 + modifier
  {
    uint32_t cps[] = {0x1F44B, 0xFE0E, 0x1F3FB};  // 👋️🏻🏻
    emoji_scan_sequence_t exp[] = {{0, 1}, {2, 2}};
    test_greedy("Emoji + VS-15 + modifier (rejected) [greedy]", cps, 3, exp, 2);
    test_strict("Emoji + VS-15 + modifier (rejected) [strict]", cps, 3, exp, 2);
  }

  // Emoji + VS-16 + modifier (rejected after VS-16)
  {
    uint32_t cps[] = {0x1F44B, 0xFE0F, 0x1F3FB};  // 👋️🏻🏻
    emoji_scan_sequence_t exp[] = {{0, 1}, {2,2}};
    test_greedy("Emoji + VS-16 + modifier (rejected) [greedy]", cps, 3, exp, 2);
    test_strict("Emoji + VS-16 + modifier (rejected) [strict]", cps, 3, exp, 2);
  }

  // Emoji + modifier + VS-16 (rejected after modifier)
  {
    uint32_t cps[] = {0x1F44B, 0x1F3FB, 0xFE0F};  // 👋🏻️
    emoji_scan_sequence_t exp[] = {{0, 1}};  // 👋🏻, VS-16 rejected after modifier
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
    emoji_scan_sequence_t exp[] = {{0, 1}, {2, 3}, {4, 4}};
    test_greedy("Multiple emoji with different VS [greedy]", cps, 5, exp, 3);
    test_strict("Multiple emoji with different VS [strict]", cps, 5, exp, 3);
  }

  // Lone VS-15 (invalid)
  {
    uint32_t cps[] = {0xFE0E};
    emoji_scan_sequence_t exp[] = {{0, 0}};
    test_greedy("Lone VS-15 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-15 [strict]", cps, 1, exp, 0);
  }

  // Lone VS-16 (invalid)
  {
    uint32_t cps[] = {0xFE0F};
    emoji_scan_sequence_t exp[] = {0};
    test_greedy("Lone VS-16 [greedy]", cps, 1, exp, 0);
    test_strict("Lone VS-16 [strict]", cps, 1, exp, 0);
  }

  // Double VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0xFE0E};  // ☺︎︎
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-15 + VS-15 (second rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-15 + VS-15 (second rejected) [strict]", cps, 3, exp, 1);
  }

  // VS-16 followed by VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0xFE0E};  // ☺️︎
    emoji_scan_sequence_t exp[] = {{0, 1}};
    test_greedy("Emoji + VS-16 + VS-15 (second rejected) [greedy]", cps, 3, exp, 1);
    test_strict("Emoji + VS-16 + VS-15 (second rejected) [strict]", cps, 3, exp, 1);
  }

  // ZWJ sequence with VS-16 before ZWJ
  {
    uint32_t cps[] = {
      0x1F468, 0xFE0F, 0x200D,  // 👨️‍
      0x1F469, 0xFE0F           // 👩️
    };
    emoji_scan_sequence_t exp[] = {{0, 4}};  // Full family
    test_greedy("ZWJ sequence: man VS-16 ZWJ woman VS-16 [greedy]", cps, 5, exp, 1);
    test_strict("ZWJ sequence: man VS-16 ZWJ woman VS-16 [strict]", cps, 5, exp, 1);
  }

  // Mixed: keycap with VS-15, then emoji with VS-16
  {
    uint32_t cps[] = {
      0x0031, 0xFE0E, 0x20E3,  // 1︎⃣
      0x263A, 0xFE0F           // ☺️
    };
    emoji_scan_sequence_t exp[] = {{0, 2}, {3, 4}};
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
    emoji_scan_sequence_t exp[] = {{0, 1}, {3, 4}};
    test_greedy("VS-15 + text + VS-16 [greedy]", cps, 5, exp, 2);
    test_strict("VS-15 + text + VS-16 [strict]", cps, 5, exp, 2);
  }

  // Multiple text codepoints between emoji sequences (regression)
  {
    uint32_t cps[] = {
      0x263A, 0xFE0E,  // ☺︎
      0x0041,          // A
      0x0042,          // B
      0x263A, 0xFE0F   // ☺️
    };
    emoji_scan_sequence_t exp[] = {{0, 1}, {4, 5}};
    test_greedy("VS-15 + text + text + VS-16 [greedy]", cps, 6, exp, 2);
    test_strict("VS-15 + text + text + VS-16 [strict]", cps, 6, exp, 2);
  }

  print_summary();

  return TestsFailed > 0 ? 1 : 0;
}
