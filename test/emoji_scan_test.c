/*
 * Tests for emoji_scan_strict() and emoji_scan_greedy().
 *
 * Most tests use test_both() which runs a single test case through both
 * scanning modes and expects the same output.  Where the two modes diverge,
 * test_greedy() and test_strict() are called separately.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "emoji_scan.h"

static int TestsRun    = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

static bool sequences_equal(const emoji_scan_range_t* got, size_t got_n,
                            const emoji_scan_range_t* exp, size_t exp_n) {
  if (got_n != exp_n)
    return false;
  for (size_t i = 0; i < got_n; i++) {
    if (got[i].start != exp[i].start || got[i].end != exp[i].end)
      return false;
  }
  return true;
}

static void print_diff(const emoji_scan_range_t* got, size_t got_n,
                       const emoji_scan_range_t* exp, size_t exp_n) {
  size_t max = got_n > exp_n ? got_n : exp_n;
  for (size_t i = 0; i < max; i++) {
    if (i < got_n)
      printf("  [%zu] got: start=%zu, end=%zu\n", i, got[i].start, got[i].end);
    if (i < exp_n)
      printf("  [%zu] exp: start=%zu, end=%zu\n", i, exp[i].start, exp[i].end);
  }
}

static void run_one(const char* name,
                    size_t (*fn)(const uint32_t*, size_t,
                                 emoji_scan_range_t*, size_t),
                    const uint32_t* cps, size_t len,
                    const emoji_scan_range_t* exp, size_t exp_n) {
  emoji_scan_range_t out[8];
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
}

static void test_greedy(const char* name,
                        uint32_t* cps, size_t len,
                        emoji_scan_range_t* exp, size_t exp_n) {
  run_one(name, emoji_scan_greedy, cps, len, exp, exp_n);
}

static void test_strict(const char* name,
                        uint32_t* cps, size_t len,
                        emoji_scan_range_t* exp, size_t exp_n) {
  run_one(name, emoji_scan_strict, cps, len, exp, exp_n);
}

static void test_both(const char* label,
                      uint32_t* cps, size_t len,
                      emoji_scan_range_t* exp, size_t exp_n) {
  char name[256];
  snprintf(name, sizeof(name), "%s [greedy]", label);
  run_one(name, emoji_scan_greedy, cps, len, exp, exp_n);
  snprintf(name, sizeof(name), "%s [strict]", label);
  run_one(name, emoji_scan_strict, cps, len, exp, exp_n);
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

  // ── Basic emoji ────────────────────────────────────────────────── 

  // 😀😃 - Two separate emoji
  {
    uint32_t cps[] = {0x1F600, 0x1F603};
    emoji_scan_range_t exp[] = {{0, 0}, {1, 1}};
    test_both("Adjacent emojis", cps, 2, exp, 2);
  }
  // © - Copyright symbol (text-default emoji)
  {
    uint32_t cps[] = {0x00A9};
    emoji_scan_range_t exp[] = {{0, 0}};
    test_both("Text-default emoji", cps, 1, exp, 1);
  }

  // ── Keycap sequences ──────────────────────────────────────────── 

  // Lone keycap base (rejected — keycap base is not accepted as bare emoji)
  {
    uint32_t cps[] = {0x0031};
    test_both("Lone keycap base", cps, 1, NULL, 0);
  }
  // Keycap base + VS-15 (no term)
  {
    uint32_t cps[] = {0x0031, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Keycap base + VS-15 (no term)", cps, 2, exp, 1);
  }
  // Keycap base + VS-16 (no term)
  {
    uint32_t cps[] = {0x0031, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Keycap base + VS-16 (no term)", cps, 2, exp, 1);
  }
  // Keycap base + VS-15 + keycap term
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3};
    emoji_scan_range_t exp[] = {{0, 2}};
    test_both("Keycap + VS-15 + term", cps, 3, exp, 1);
  }
  // Keycap base + VS-16 + keycap term
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3};
    emoji_scan_range_t exp[] = {{0, 2}};
    test_both("Keycap + VS-16 + term", cps, 3, exp, 1);
  }
  // Keycap base + keycap term (no VS)
  {
    uint32_t cps[] = {0x0031, 0x20E3};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Keycap + term (no VS)", cps, 2, exp, 1);
  }
  // Non-keycap emoji + VS-15 + keycap term (VS-15 is dead end, term rejected)
  {
    uint32_t cps[] = {0x1F600, 0xFE0E, 0x20E3};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-15 + keycap term (rejected)", cps, 3, exp, 1);
  }
  // Non-keycap emoji + VS-16 + keycap term (OPTIONAL_ZWJ has no keycap transition)
  {
    uint32_t cps[] = {0x1F600, 0xFE0F, 0x20E3};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-16 + keycap term (rejected)", cps, 3, exp, 1);
  }

  // ── Modifier sequences ────────────────────────────────────────── 

  // 👦🏻 - Boy with light skin tone
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + modifier", cps, 2, exp, 1);
  }
  // 👍🏻🏽 - Thumbs up + two modifiers (second modifier is standalone)
  {
    uint32_t cps[] = {0x1F44D, 0x1F3FB, 0x1F3FD};
    emoji_scan_range_t exp[] = {{0, 1}, {2, 2}};
    test_both("Double modifier", cps, 3, exp, 2);
  }
  // 🏻 - Lone modifier (accepted as basic emoji)
  {
    uint32_t cps[] = {0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 0}};
    test_both("Modifier without base", cps, 1, exp, 1);
  }
  // 🏻️ - Lone modifier + VS-16
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Lone modifier + VS-16", cps, 2, exp, 1);
  }
  // 🏻︎ - Lone modifier + VS-15
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Lone modifier + VS-15", cps, 2, exp, 1);
  }
  // 🏻‍👩 - Lone modifier + ZWJ + emoji (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3FB, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 0}, {2, 2}};
    test_both("Lone modifier + ZWJ + emoji", cps, 3, exp, 2);
  }
  // 🏻️‍👩 - Lone modifier + VS-16 + ZWJ + emoji (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("Lone modifier + VS-16 + ZWJ + emoji", cps, 4, exp, 2);
  }
  // 👩‍🦰🏻 - ZWJ hair + trailing modifier (modifier starts new sequence)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F9B0, 0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 2}, {3, 3}};
    test_both("Hair emoji + modifier", cps, 4, exp, 2);
  }
  // 🏻🏼 - Two lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC};
    emoji_scan_range_t exp[] = {{0, 0}, {1, 1}};
    test_both("Two lone modifiers", cps, 2, exp, 2);
  }
  // 🏻🏼🏽🏾🏿 - Five lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF};
    emoji_scan_range_t exp[] = {{0,0}, {1,1}, {2,2}, {3,3}, {4,4}};
    test_both("Multiple lone modifiers", cps, 5, exp, 5);
  }

  // ── Regional indicators / flags ───────────────────────────────── 

  // 🇺🇸 - US flag (RI pair)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("RI pair", cps, 2, exp, 1);
  }
  // 🇸 - Lone RI
  {
    uint32_t cps[] = {0x1F1F8};
    emoji_scan_range_t exp[] = {{0, 0}};
    test_both("Lone RI", cps, 1, exp, 1);
  }
  // 🇸️ - Lone RI + VS-16
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Lone RI + VS-16", cps, 2, exp, 1);
  }
  // 🇸︎ - Lone RI + VS-15
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Lone RI + VS-15", cps, 2, exp, 1);
  }
  // 🇸‍👩 - Lone RI + ZWJ + emoji (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1F8, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 0}, {2, 2}};
    test_both("Lone RI + ZWJ + emoji", cps, 3, exp, 2);
  }
  // 🇸️‍👩 - Lone RI + VS-16 + ZWJ + emoji (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("Lone RI + VS-16 + ZWJ + emoji", cps, 4, exp, 2);
  }
  // 🇺🇸‍👩 - RI pair + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("RI pair + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 🇸🇪🇳 - Sweden flag + lone RI (odd count)
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_scan_range_t exp[] = {{0, 1}, {2, 2}};
    test_both("Odd RI count", cps, 3, exp, 2);
  }
  // 🇸😀 - Lone RI followed by emoji
  {
    uint32_t cps[] = {0x1F1F8, 0x1F600};
    emoji_scan_range_t exp[] = {{0, 0}, {1, 1}};
    test_both("RI followed by emoji", cps, 2, exp, 2);
  }

  // ── Variation selectors ───────────────────────────────────────── 

  // Lone VS-16 (invalid — no base)
  {
    uint32_t cps[] = {0xFE0F};
    test_both("Lone VS-16", cps, 1, NULL, 0);
  }
  // Lone VS-15 (invalid — no base)
  {
    uint32_t cps[] = {0xFE0E};
    test_both("Lone VS-15", cps, 1, NULL, 0);
  }
  // ❤️️ - Emoji + double VS-16 (second rejected)
  {
    uint32_t cps[] = {0x2764, 0xFE0F, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Double VS-16", cps, 3, exp, 1);
  }
  // ☺︎︎ - Emoji + double VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-15 + VS-15 (second rejected)", cps, 3, exp, 1);
  }
  // ☺️︎ - Emoji + VS-16 + VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-16 + VS-15 (second rejected)", cps, 3, exp, 1);
  }
  // ✋️🏻 - VS-16 followed by modifier (modifier starts new sequence)
  {
    uint32_t cps[] = {0x270B, 0xFE0F, 0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 1}, {2, 2}};
    test_both("VS-16 followed by modifier", cps, 3, exp, 2);
  }
  // ☺︎ - Emoji + VS-15
  {
    uint32_t cps[] = {0x263A, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-15", cps, 2, exp, 1);
  }
  // ☺️ - Emoji + VS-16
  {
    uint32_t cps[] = {0x263A, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + VS-16", cps, 2, exp, 1);
  }
  // Multiple emoji with different VS
  {
    uint32_t cps[] = {
      0x263A, 0xFE0F,  // ☺️
      0x263A, 0xFE0E,  // ☺︎
      0x263A           // ☺
    };
    emoji_scan_range_t exp[] = {{0, 1}, {2, 3}, {4, 4}};
    test_both("Multiple emoji with different VS", cps, 5, exp, 3);
  }

  // ── VS asymmetry (VS-15 blocks ZWJ, VS-16 allows it) ─────────── 

  // ☺︎‍👩 - VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // ☺️‍👩 - VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨︎‍👩 - Modifier_base + VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F466, 0xFE0E, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("Modifier_base + VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 👨️‍👩 - Modifier_base + VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("Modifier_base + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨︎‍👩 - Emoji + VS-15 + ZWJ (TERMINAL has no ZWJ — rejected)
  {
    uint32_t cps[] = {0x1F468, 0xFE0E, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("Emoji + VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 👨️‍👩 - Emoji + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("Emoji + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // VS-15 blocks modifier too
  {
    uint32_t cps[] = {0x1F44B, 0xFE0E, 0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 1}, {2, 2}};
    test_both("Emoji + VS-15 + modifier", cps, 3, exp, 2);
  }
  // VS-16 blocks modifier too
  {
    uint32_t cps[] = {0x1F44B, 0xFE0F, 0x1F3FB};
    emoji_scan_range_t exp[] = {{0, 1}, {2, 2}};
    test_both("Emoji + VS-16 + modifier", cps, 3, exp, 2);
  }
  // Modifier + VS-16 (OPTIONAL_ZWJ rejects VS-16)
  {
    uint32_t cps[] = {0x1F44B, 0x1F3FB, 0xFE0F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("Emoji + modifier + VS-16 (rejected)", cps, 3, exp, 1);
  }

  // ── ZWJ sequences ─────────────────────────────────────────────── 

  // 👨‍👩‍👧 - Family
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    emoji_scan_range_t exp[] = {{0, 4}};
    test_both("ZWJ family", cps, 5, exp, 1);
  }
  // 👨‍👩‍👧‍👦 - Family with two children (long ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466};
    emoji_scan_range_t exp[] = {{0, 6}};
    test_both("Long ZWJ sequence (4 emoji)", cps, 7, exp, 1);
  }
  // 👦🏻‍💻 - Modifier + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("Modifier + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨🏻‍💻 - Technologist with skin tone
  {
    uint32_t cps[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("ZWJ after modifier (technologist)", cps, 4, exp, 1);
  }
  // ZWJ sequence with VS-16 before ZWJ
  {
    uint32_t cps[] = {
      0x1F468, 0xFE0F, 0x200D,  // 👨️‍
      0x1F469, 0xFE0F           // 👩️
    };
    emoji_scan_range_t exp[] = {{0, 4}};
    test_both("ZWJ sequence: man VS-16 ZWJ woman VS-16", cps, 5, exp, 1);
  }

  // ── ZWJ sequences — greedy / strict diverge ───────────────────── 

  // 👨‍ - Trailing ZWJ (greedy emits prefix, strict drops)
  {
    uint32_t cps[] = {0x1F468, 0x200D};
    emoji_scan_range_t exp_greedy[] = {{0, 0}};
    test_greedy("Trailing ZWJ [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Trailing ZWJ [strict]", cps, 2, NULL, 0);
  }
  // 👨‍‍👩 - Double ZWJ
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x200D, 0x1F469};
    emoji_scan_range_t exp_greedy[] = {{0, 0}, {3, 3}};
    emoji_scan_range_t exp_strict[] = {{3, 3}};
    test_greedy("Double ZWJ [greedy]", cps, 4, exp_greedy, 2);
    test_strict("Double ZWJ [strict]", cps, 4, exp_strict, 1);
  }
  // 👨‍A - ZWJ + non-emoji target
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x0041};
    emoji_scan_range_t exp_greedy[] = {{0, 0}};
    test_greedy("ZWJ followed by non-emoji [greedy]", cps, 3, exp_greedy, 1);
    test_strict("ZWJ followed by non-emoji [strict]", cps, 3, NULL, 0);
  }

  // ── Deviation: keycaps not accepted as ZWJ elements ───────────── 

  // 1⃣‍👩 - Keycap + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x0031, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("Keycap + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 1︎⃣‍👩 - VS-15 keycap + ZWJ
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 2}, {4, 4}};
    test_both("VS-15 keycap + ZWJ (rejected)", cps, 5, exp, 2);
  }
  // 1️⃣‍👩 - VS-16 keycap + ZWJ
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 2}, {4, 4}};
    test_both("VS-16 keycap + ZWJ (rejected)", cps, 5, exp, 2);
  }
  // 👩‍1️⃣ - Emoji + ZWJ + keycap (keycap base not accepted as ZWJ target)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x0031, 0xFE0F, 0x20E3};
    emoji_scan_range_t exp_greedy[] = {{0, 0}, {2, 4}};
    emoji_scan_range_t exp_strict[] = {{2, 4}};
    test_greedy("ZWJ + keycap (rejected) [greedy]", cps, 5, exp_greedy, 2);
    test_strict("ZWJ + keycap (rejected) [strict]", cps, 5, exp_strict, 1);
  }

  // ── Deviation: flags not accepted as ZWJ elements ─────────────── 

  // 👨‍🇸🇪 - Emoji + ZWJ + RI pair (RI not valid as ZWJ target)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F1F8, 0x1F1EA};
    emoji_scan_range_t exp_greedy[] = {{0, 0}, {2, 3}};
    emoji_scan_range_t exp_strict[] = {{2, 3}};
    test_greedy("ZWJ + RI flag [greedy]", cps, 4, exp_greedy, 2);
    test_strict("ZWJ + RI flag [strict]", cps, 4, exp_strict, 1);
  }
  // 🇸🇪‍👨 - Flag + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x200D, 0x1F468};
    emoji_scan_range_t exp[] = {{0, 1}, {3, 3}};
    test_both("RI flag + ZWJ", cps, 4, exp, 2);
  }

  // ── Deviation: tag sequences not accepted as ZWJ elements ─────── 

  // 🏴󠁧󠁢󠁿‍👩 - Tag sequence + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE007F, 0x200D, 0x1F469};
    emoji_scan_range_t exp[] = {{0, 3}, {5, 5}};
    test_both("Tag sequence + ZWJ (rejected)", cps, 6, exp, 2);
  }
  // 👩‍🏴󠁧󠁢󠁿 - Emoji + ZWJ + tag sequence (tag base not valid as ZWJ target)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F3F4, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_range_t exp_greedy[] = {{0, 0}, {2, 5}};
    emoji_scan_range_t exp_strict[] = {{2, 5}};
    test_greedy("ZWJ + tag sequence (rejected) [greedy]", cps, 6, exp_greedy, 2);
    test_strict("ZWJ + tag sequence (rejected) [strict]", cps, 6, exp_strict, 1);
  }

  // ── Tag sequences ─────────────────────────────────────────────── 

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England flag (complete tag sequence)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_scan_range_t exp[] = {{0, 6}};
    test_both("Complete tag sequence (England)", cps, 7, exp, 1);
  }
  // 🏴‍😀 - TAG_BASE + ZWJ + emoji (tag base as ZWJ element)
  {
    uint32_t cps[] = {0x1F3F4, 0x200D, 0x1F600};
    emoji_scan_range_t exp[] = {{0, 2}};
    test_both("TAG_BASE + ZWJ + emoji", cps, 3, exp, 1);
  }
  // 🏴️‍😀 - TAG_BASE + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0x200D, 0x1F600};
    emoji_scan_range_t exp[] = {{0, 3}};
    test_both("TAG_BASE + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 🏴︎ - TAG_BASE + VS-15
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0E};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("TAG_BASE + VS-15", cps, 2, exp, 1);
  }
  // TAG_BASE + VS-16 + tag chars + cancel (tag rejected after VS-16)
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_range_t exp[] = {{0, 1}};
    test_both("TAG_BASE + VS-16 + tags (tag rejected)", cps, 5, exp, 1);
  }
  // 😀 + tag chars + cancel (non-tag base, tags rejected)
  {
    uint32_t cps[] = {0x1F600, 0xE0067, 0xE0062, 0xE007F};
    emoji_scan_range_t exp[] = {{0, 0}};
    test_both("Emoji + tag chars (rejected)", cps, 4, exp, 1);
  }
  // 😀 + cancel tag (non-tag base, cancel rejected)
  {
    uint32_t cps[] = {0x1F600, 0xE007F};
    emoji_scan_range_t exp[] = {{0, 0}};
    test_both("Emoji + cancel tag (rejected)", cps, 2, exp, 1);
  }
  // Tag sequence without cancel tag (greedy emits prefix, strict drops)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062};
    emoji_scan_range_t exp_greedy[] = {{0, 0}};
    test_greedy("Tag without cancel [greedy]", cps, 3, exp_greedy, 1);
    test_strict("Tag without cancel [strict]", cps, 3, NULL, 0);
  }
  // Cancel tag without any tag chars (dead-end pending state)
  {
    uint32_t cps[] = {0x1F3F4, 0xE007F};
    emoji_scan_range_t exp_greedy[] = {{0, 0}};
    test_greedy("Cancel tag without specs [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Cancel tag without specs [strict]", cps, 2, NULL, 0);
  }

  // ── Mixed / multi-sequence ────────────────────────────────────── 

  // 😀👍🏻🇺🇸 - Three different types in sequence
  {
    uint32_t cps[] = {0x1F600, 0x1F44D, 0x1F3FB, 0x1F1FA, 0x1F1F8};
    emoji_scan_range_t exp[] = {{0, 0}, {1, 2}, {3, 4}};
    test_both("Multiple sequences (3 types)", cps, 5, exp, 3);
  }
  // 😀🇸🇪🇳 - Emoji, flag, lone RI
  {
    uint32_t cps[] = {0x1F600, 0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_scan_range_t exp[] = {{0, 0}, {1, 2}, {3, 3}};
    test_both("Emoji then flag then lone RI", cps, 4, exp, 3);
  }
  // 1︎⃣ + ☺️ - Keycap VS-15 then emoji VS-16
  {
    uint32_t cps[] = {
      0x0031, 0xFE0E, 0x20E3,
      0x263A, 0xFE0F
    };
    emoji_scan_range_t exp[] = {{0, 2}, {3, 4}};
    test_both("Keycap VS-15 + Emoji VS-16", cps, 5, exp, 2);
  }
  // Text between emoji sequences
  {
    uint32_t cps[] = {
      0x263A, 0xFE0E,  // ☺︎
      0x0041,          // A
      0x263A, 0xFE0F   // ☺️
    };
    emoji_scan_range_t exp[] = {{0, 1}, {3, 4}};
    test_both("VS-15 + text + VS-16", cps, 5, exp, 2);
  }
  // Multiple text codepoints between emoji sequences
  {
    uint32_t cps[] = {
      0x263A, 0xFE0E,  // ☺︎
      0x0041,          // A
      0x0042,          // B
      0x263A, 0xFE0F   // ☺️
    };
    emoji_scan_range_t exp[] = {{0, 1}, {4, 5}};
    test_both("VS-15 + text + text + VS-16", cps, 6, exp, 2);
  }

  // ── Edge cases ────────────────────────────────────────────────── 

  // Empty input
  {
    uint32_t cps[] = {0};
    test_both("Empty input", cps, 0, NULL, 0);
  }
  // All ASCII (no emoji)
  {
    uint32_t cps[] = {0x0041, 0x0042, 0x0043};
    test_both("No emoji in input", cps, 3, NULL, 0);
  }

  print_summary();
  return TestsFailed > 0 ? 1 : 0;
}
