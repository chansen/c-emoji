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
#include "emoji_span.h"
#include "emoji_types.h"

static int TestsRun    = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

static bool sequences_equal(const emoji_range_t* got, size_t got_n,
                            const emoji_range_t* exp, size_t exp_n) {
  if (got_n != exp_n)
    return false;
  for (size_t i = 0; i < got_n; i++) {
    if (got[i].start != exp[i].start || got[i].end != exp[i].end)
      return false;
    if (got[i].type != exp[i].type || got[i].style != exp[i].style)
      return false;
  }
  return true;
}

static void print_diff(const emoji_range_t* got, size_t got_n,
                       const emoji_range_t* exp, size_t exp_n) {
  size_t max = got_n > exp_n ? got_n : exp_n;
  for (size_t i = 0; i < max; i++) {
    if (i < got_n)
      printf("  [%zu] got: start=%zu, end=%zu type=%d style=%d\n", i,
             got[i].start, got[i].end, got[i].type, got[i].style);
    if (i < exp_n)
      printf("  [%zu] exp: start=%zu, end=%zu type=%d style=%d\n", i,
             exp[i].start, exp[i].end, exp[i].type, exp[i].style);
  }
}

static void run_one(const char* name,
                    size_t (*fn)(const uint32_t*, size_t,
                                 emoji_range_t*, size_t),
                    const uint32_t* cps, size_t len,
                    const emoji_range_t* exp, size_t exp_n) {
  emoji_range_t out[8];
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
                        emoji_range_t* exp, size_t exp_n) {
  run_one(name, emoji_scan_greedy, cps, len, exp, exp_n);
}

static void test_strict(const char* name,
                        uint32_t* cps, size_t len,
                        emoji_range_t* exp, size_t exp_n) {
  run_one(name, emoji_scan_strict, cps, len, exp, exp_n);
}

static void test_both(const char* label,
                      uint32_t* cps, size_t len,
                      emoji_range_t* exp, size_t exp_n) {
  char name[256];
  snprintf(name, sizeof(name), "%s [greedy]", label);
  run_one(name, emoji_scan_greedy, cps, len, exp, exp_n);
  snprintf(name, sizeof(name), "%s [strict]", label);
  run_one(name, emoji_scan_strict, cps, len, exp, exp_n);
}

static bool span_matches_range(emoji_span_t span, const uint32_t* base,
                               const emoji_range_t* exp) {
  return span.src  == base + exp->start &&
        span.len   == emoji_range_length(exp) &&
        span.type  == exp->type &&
        span.style == exp->style;
}

static void run_next_whole(const char* name,
                           bool (*fn)(const uint32_t*, size_t,
                                      emoji_span_t*, size_t*, bool),
                           const uint32_t* cps, size_t len,
                           const emoji_range_t* exp, size_t exp_n) {
  emoji_span_t out[8];
  size_t got_n = 0, pos = 0;

  while (pos < len) {
    emoji_span_t s;
    size_t next_pos;
    bool found = fn(cps + pos, len - pos, &s, &next_pos, true);
    if (found && got_n < 8)
      out[got_n++] = s;
    pos += next_pos;
    if (!found)
      break;
  }

  bool ok = (got_n == exp_n);
  for (size_t i = 0; ok && i < got_n; i++)
    ok = span_matches_range(out[i], cps, &exp[i]);

  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
    return;
  }
  TestsFailed++;
  printf("FAIL - %s\n", name);
  printf("  expected %zu sequence(s), got %zu\n", exp_n, got_n);
  for (size_t i = 0; i < got_n; i++)
    printf("    got[%zu]: offset=%td len=%zu type=%d style=%d\n",
          i, out[i].src - cps, out[i].len, out[i].type, out[i].style);
}

static void test_next_greedy(const char* name,
                             uint32_t* cps, size_t len,
                             emoji_range_t* exp, size_t exp_n) {
  run_next_whole(name, emoji_scan_next_greedy, cps, len, exp, exp_n);
}

static void test_next_strict(const char* name,
                             uint32_t* cps, size_t len,
                             emoji_range_t* exp, size_t exp_n) {
  run_next_whole(name, emoji_scan_next_strict, cps, len, exp, exp_n);
}

static void test_next_both(const char* label,
                           uint32_t* cps, size_t len,
                           emoji_range_t* exp, size_t exp_n) {
  char name[256];
  snprintf(name, sizeof(name), "%s [next greedy]", label);
  run_next_whole(name, emoji_scan_next_greedy, cps, len, exp, exp_n);
  snprintf(name, sizeof(name), "%s [next strict]", label);
  run_next_whole(name, emoji_scan_next_strict, cps, len, exp, exp_n);
}

/*
 * Calls fn exactly once with the given eof and checks the outcome: if exp
 * is NULL, expects fn to report not found; otherwise expects a span
 * covering the same codepoints as *exp.
 */
static void run_next_once(const char* name,
                          bool (*fn)(const uint32_t*, size_t,
                                     emoji_span_t*, size_t*, bool),
                          const uint32_t* cps, size_t len, bool eof,
                          const emoji_range_t* exp) {
  emoji_span_t out;
  size_t position;
  bool found = fn(cps, len, &out, &position, eof);
  bool ok = (exp == NULL) ? !found : (found && span_matches_range(out, cps, exp));

  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
    return;
  }
  TestsFailed++;
  printf("FAIL - %s\n", name);
  if (exp == NULL)
    printf("  expected not found, got offset=%td len=%zu type=%d style=%d (position=%zu)\n",
          out.src - cps, out.len, out.type, out.style, position);
  else if (!found)
    printf("  expected offset=%zu len=%zu type=%d style=%d, got not found (position=%zu)\n",
          exp->start, emoji_range_length(exp), exp->type, exp->style, position);
  else
    printf("  expected offset=%zu len=%zu type=%d style=%d, got offset=%td len=%zu type=%d style=%d (position=%zu)\n",
          exp->start, emoji_range_length(exp), exp->type, exp->style,
          out.src - cps, out.len, out.type, out.style, position);
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

#define RANGE(range_start, range_end, range_type, range_style) \
  ((emoji_range_t){                                          \
      .start = (range_start),                               \
      .end   = (range_end),                                 \
      .type  = (range_type),                                \
      .style = (range_style),                               \
  })

enum {
  T_BASIC       = EMOJI_SEQUENCE_BASIC,
  T_KEYCAP      = EMOJI_SEQUENCE_KEYCAP,
  T_FLAG        = EMOJI_SEQUENCE_FLAG,
  T_TAG         = EMOJI_SEQUENCE_TAG,
  T_MODIFIER    = EMOJI_SEQUENCE_MODIFIER,
  T_ZWJ         = EMOJI_SEQUENCE_ZWJ,
  
  S_UNSPECIFIED = EMOJI_PRESENTATION_UNSPECIFIED,
  S_TEXT        = EMOJI_PRESENTATION_TEXT,
  S_EMOJI       = EMOJI_PRESENTATION_EMOJI,
};

int main(void) {

  // ── Basic emoji ────────────────────────────────────────────────── 

  // 😀😃 - Two separate emoji
  {
    uint32_t cps[] = {0x1F600, 0x1F603};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED), 
      RANGE(1, 2, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Adjacent emojis", cps, 2, exp, 2);
  }
  // © - Copyright symbol (text-default emoji)
  {
    uint32_t cps[] = {0x00A9};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Text-default emoji", cps, 1, exp, 1);
  }

  // ── Keycap sequences ──────────────────────────────────────────── 

  // 1 - Lone keycap base (rejected — keycap base is not accepted as bare emoji)
  {
    uint32_t cps[] = {0x0031};
    test_both("Lone keycap base", cps, 1, NULL, 0);
  }
  // 1︎ - Keycap base + VS-15 (no term)
  {
    uint32_t cps[] = {0x0031, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT)
    };
    test_both("Keycap base + VS-15 (no term)", cps, 2, exp, 1);
  }
  // 1️ - Keycap base + VS-16 (no term)
  {
    uint32_t cps[] = {0x0031, 0xFE0F};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("Keycap base + VS-16 (no term)", cps, 2, exp, 1);
  }
  // 1︎⃣ - Keycap base + VS-15 + keycap term
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_KEYCAP, S_TEXT)
    };
    test_both("Keycap + VS-15 + term", cps, 3, exp, 1);
  }
  // 1️⃣ - Keycap base + VS-16 + keycap term
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_KEYCAP, S_EMOJI)
    };
    test_both("Keycap + VS-16 + term", cps, 3, exp, 1);
  }
  // 1⃣ - Keycap base + keycap term (no VS)
  {
    uint32_t cps[] = {0x0031, 0x20E3};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_KEYCAP, S_UNSPECIFIED)
    };
    test_both("Keycap + term (no VS)", cps, 2, exp, 1);
  }
  // Non-keycap emoji + VS-15 + keycap term (VS-15 is dead end, term rejected)
  {
    uint32_t cps[] = {0x1F600, 0xFE0E, 0x20E3};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT)
    };
    test_both("Emoji + VS-15 + keycap term (rejected)", cps, 3, exp, 1);
  }
  // Non-keycap emoji + VS-16 + keycap term (OPTIONAL_ZWJ has no keycap transition)
  {
    uint32_t cps[] = {0x1F600, 0xFE0F, 0x20E3};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("Emoji + VS-16 + keycap term (rejected)", cps, 3, exp, 1);
  }

  // ── Modifier sequences ────────────────────────────────────────── 

  // 👦🏻 - Boy with light skin tone
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_MODIFIER, S_UNSPECIFIED)
    };
    test_both("Emoji + modifier", cps, 2, exp, 1);
  }
  // 👍🏻🏽 - Thumbs up + two modifiers (second modifier is standalone)
  {
    uint32_t cps[] = {0x1F44D, 0x1F3FB, 0x1F3FD};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_MODIFIER, S_UNSPECIFIED), 
      RANGE(2, 3, T_BASIC,    S_UNSPECIFIED)
    };
    test_both("Double modifier", cps, 3, exp, 2);
  }
  // 🏻 - Lone modifier (accepted as basic emoji)
  {
    uint32_t cps[] = {0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Modifier without base", cps, 1, exp, 1);
  }
  // 🏻️ - Lone modifier + VS-16 (Modifier is TERMINAL, has no VS-16 transition)
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0F};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone modifier + VS-16", cps, 2, exp, 1);
  }
  // 🏻︎ - Lone modifier + VS-15 (Modifier is TERMINAL, has no VS-15 transition)
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone modifier + VS-15", cps, 2, exp, 1);
  }
  // 🏻‍👩 - Lone modifier + ZWJ + emoji (Modifier is TERMINAL, has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3FB, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone modifier + ZWJ + emoji", cps, 3, exp, 2);
  }
  // 🏻️‍👩 - Lone modifier + VS-16 + ZWJ + emoji (Modifier is TERMINAL, has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3FB, 0xFE0F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone modifier + VS-16 + ZWJ + emoji", cps, 4, exp, 2);
  }
  // 👩‍🦰🏻 - ZWJ hair + trailing modifier (modifier starts new sequence)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F9B0, 0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_ZWJ,   S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Hair emoji + modifier", cps, 4, exp, 2);
  }
  // 🏻🏼 - Two lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(1, 2, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Two lone modifiers", cps, 2, exp, 2);
  }
  // 🏻🏼🏽🏾🏿 - Five lone modifiers
  {
    uint32_t cps[] = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(1, 2, T_BASIC, S_UNSPECIFIED),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED),
      RANGE(4, 5, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Multiple lone modifiers", cps, 5, exp, 5);
  }

  // ── Regional indicators / flags ───────────────────────────────── 

  // 🇺🇸 - US flag (RI pair)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_FLAG, S_UNSPECIFIED)
    };
    test_both("RI pair", cps, 2, exp, 1);
  }
  // 🇸 - Lone RI
  {
    uint32_t cps[] = {0x1F1F8};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone RI", cps, 1, exp, 1);
  }
  // 🇸️ - Lone RI + VS-16 (RI has no VS-16 transition)
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F}; 
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone RI + VS-16", cps, 2, exp, 1);
  }
  // 🇸︎ - Lone RI + VS-15 (RI has no VS-15 transition)
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone RI + VS-15", cps, 2, exp, 1);
  }
  // 🇸‍👩 - Lone RI + ZWJ + emoji (RI has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1F8, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone RI + ZWJ + emoji", cps, 3, exp, 2);
  }
  // 🇸️‍👩 - Lone RI + VS-16 + ZWJ + emoji (RI has no VS-16 transition)
  {
    uint32_t cps[] = {0x1F1F8, 0xFE0F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Lone RI + VS-16 + ZWJ + emoji", cps, 4, exp, 2);
  }
  // 🇺🇸‍👩 - RI pair + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_FLAG,  S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("RI pair + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 🇸🇪🇳 - Sweden flag + lone RI (odd count)
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_FLAG,  S_UNSPECIFIED),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Odd RI count", cps, 3, exp, 2);
  }
  // 🇸😀 - Lone RI followed by emoji
  {
    uint32_t cps[] = {0x1F1F8, 0x1F600};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(1, 2, T_BASIC, S_UNSPECIFIED)
    };
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
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("Double VS-16", cps, 3, exp, 1);
  }
  // ☺︎︎ - Emoji + double VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT)
    };
    test_both("Emoji + VS-15 + VS-15 (second rejected)", cps, 3, exp, 1);
  }
  // ☺️︎ - Emoji + VS-16 + VS-15 (second rejected)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("Emoji + VS-16 + VS-15 (second rejected)", cps, 3, exp, 1);
  }
  // ✋️🏻 - VS-16 followed by modifier (modifier starts new sequence)
  {
    uint32_t cps[] = {0x270B, 0xFE0F, 0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("VS-16 followed by modifier", cps, 3, exp, 2);
  }
  // ☺︎ - Emoji + VS-15
  {
    uint32_t cps[] = {0x263A, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT)
    };
    test_both("Emoji + VS-15", cps, 2, exp, 1);
  }
  // ☺️ - Emoji + VS-16
  {
    uint32_t cps[] = {0x263A, 0xFE0F};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("Emoji + VS-16", cps, 2, exp, 1);
  }
  // Multiple emoji with different VS
  {
    uint32_t cps[] = {
      0x263A, 0xFE0F,  // ☺️
      0x263A, 0xFE0E,  // ☺︎
      0x263A           // ☺
    };
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI),
      RANGE(2, 4, T_BASIC, S_TEXT),
      RANGE(4, 5, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Multiple emoji with different VS", cps, 5, exp, 3);
  }

  // ── VS asymmetry (VS-15 blocks ZWJ, VS-16 allows it) ─────────── 

  // ☺︎‍👩 - VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x263A, 0xFE0E, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // ☺️‍👩 - VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x263A, 0xFE0F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_EMOJI)
    };
    test_both("VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨︎‍👩 - Modifier_base + VS-15 + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F466, 0xFE0E, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Modifier_base + VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 👨️‍👩 - Modifier_base + VS-16 + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_EMOJI)
    };
    test_both("Modifier_base + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨︎‍👩 - Emoji + VS-15 + ZWJ (TERMINAL has no ZWJ — rejected)
  {
    uint32_t cps[] = {0x1F468, 0xFE0E, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji + VS-15 + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 👨️‍👩 - Emoji + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F468, 0xFE0F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_EMOJI)
    };
    test_both("Emoji + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // VS-15 blocks modifier too
  {
    uint32_t cps[] = {0x1F44B, 0xFE0E, 0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji + VS-15 + modifier", cps, 3, exp, 2);
  }
  // VS-16 blocks modifier too
  {
    uint32_t cps[] = {0x1F44B, 0xFE0F, 0x1F3FB};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI),
      RANGE(2, 3, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji + VS-16 + modifier", cps, 3, exp, 2);
  }
  // Modifier + VS-16 (OPTIONAL_ZWJ rejects VS-16)
  {
    uint32_t cps[] = {0x1F44B, 0x1F3FB, 0xFE0F};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_MODIFIER, S_UNSPECIFIED)
    };
    test_both("Emoji + modifier + VS-16 (rejected)", cps, 3, exp, 1);
  }

  // ── ZWJ elements reject VS-15 ───────────────────────────────────

  // 👨‍👩︎ - VS-15 after a ZWJ element terminates the chain
  //        (EMOJI_ZWJ has no VS-15 transition, unlike standalone EMOJI)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("ZWJ element + VS-15 (rejected)", cps, 4, exp, 1);
  }
  // 👨‍👦︎ - VS-15 after a modifier-base ZWJ element
  //        (MODIFIER_BASE_ZWJ has no VS-15 transition)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F466, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("ZWJ modifier-base element + VS-15 (rejected)", cps, 4, exp, 1);
  }
  // 👨‍👦🏻︎ - VS-15 after a modifier inside a ZWJ chain
  //        (OPTIONAL_ZWJ accepts only ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F466, 0x1F3FB, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("ZWJ element + modifier + VS-15 (rejected)", cps, 5, exp, 1);
  }
  // 👨‍👩︎‍👧 - VS-15 mid-chain: terminates the sequence, the trailing
  //         ZWJ + emoji re-scan as a lone emoji (VS-15 and ZWJ have no base)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0xFE0E, 0x200D, 0x1F467};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_ZWJ,   S_UNSPECIFIED),
      RANGE(5, 6, T_BASIC, S_UNSPECIFIED)
    };
    test_both("VS-15 mid-chain blocks ZWJ continuation", cps, 6, exp, 2);
  }

  // ── ZWJ sequences ─────────────────────────────────────────────── 

  // 👨‍👩‍👧 - Family
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    emoji_range_t exp[] = {
      RANGE(0, 5, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("ZWJ family", cps, 5, exp, 1);
  }
  // 👨‍👩‍👧‍👦 - Family with two children (long ZWJ)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466};
    emoji_range_t exp[] = {
      RANGE(0, 7, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("Long ZWJ sequence (4 emoji)", cps, 7, exp, 1);
  }
  // 👦🏻‍💻 - Modifier + ZWJ + emoji (OPTIONAL_ZWJ allows ZWJ)
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("Modifier + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 👨🏻‍💻 - Technologist with skin tone
  {
    uint32_t cps[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("ZWJ after modifier (technologist)", cps, 4, exp, 1);
  }
  // ZWJ sequence with VS-16 before ZWJ
  {
    uint32_t cps[] = {
      0x1F468, 0xFE0F, 0x200D,  // 👨️‍
      0x1F469, 0xFE0F           // 👩️
    };
    emoji_range_t exp[] = {
      RANGE(0, 5, T_ZWJ, S_EMOJI)
    };
    test_both("ZWJ sequence: man VS-16 ZWJ woman VS-16", cps, 5, exp, 1);
  }

  // ── ZWJ sequences — greedy / strict diverge ───────────────────── 

  // 👨‍ - Trailing ZWJ (greedy emits prefix, strict drops)
  {
    uint32_t cps[] = {0x1F468, 0x200D};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_greedy("Trailing ZWJ [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Trailing ZWJ [strict]", cps, 2, NULL, 0);
  }
  // 👨‍‍👩 - Double ZWJ
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x200D, 0x1F469};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    emoji_range_t exp_strict[] = {
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_greedy("Double ZWJ [greedy]", cps, 4, exp_greedy, 2);
    test_strict("Double ZWJ [strict]", cps, 4, exp_strict, 1);
  }
  // 👨‍A - ZWJ + non-emoji target
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x0041};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_greedy("ZWJ followed by non-emoji [greedy]", cps, 3, exp_greedy, 1);
    test_strict("ZWJ followed by non-emoji [strict]", cps, 3, NULL, 0);
  }

  // ── Deviation: keycaps not accepted as ZWJ elements ───────────── 

  // 1⃣‍👩 - Keycap + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x0031, 0x20E3, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_KEYCAP, S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC,  S_UNSPECIFIED)
    };
    test_both("Keycap + ZWJ (rejected)", cps, 4, exp, 2);
  }
  // 1︎⃣‍👩 - VS-15 keycap + ZWJ
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_KEYCAP, S_TEXT),
      RANGE(4, 5, T_BASIC,  S_UNSPECIFIED)
    };
    test_both("VS-15 keycap + ZWJ (rejected)", cps, 5, exp, 2);
  }
  // 1️⃣‍👩 - VS-16 keycap + ZWJ
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_KEYCAP, S_EMOJI),
      RANGE(4, 5, T_BASIC,  S_UNSPECIFIED)
    };
    test_both("VS-16 keycap + ZWJ (rejected)", cps, 5, exp, 2);
  }
  // 👩‍1️⃣ - Emoji + ZWJ + keycap (keycap base not accepted as ZWJ target)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x0031, 0xFE0F, 0x20E3};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC,  S_UNSPECIFIED),
      RANGE(2, 5, T_KEYCAP, S_EMOJI)
    };
    emoji_range_t exp_strict[] = {
      RANGE(2, 5, T_KEYCAP, S_EMOJI)
    };
    test_greedy("ZWJ + keycap (rejected) [greedy]", cps, 5, exp_greedy, 2);
    test_strict("ZWJ + keycap (rejected) [strict]", cps, 5, exp_strict, 1);
  }

  // ── Deviation: flags not accepted as ZWJ elements ─────────────── 

  // 👨‍🇸🇪 - Emoji + ZWJ + RI pair (RI not valid as ZWJ target)
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F1F8, 0x1F1EA};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(2, 4, T_FLAG,  S_UNSPECIFIED)
    };
    emoji_range_t exp_strict[] = {
      RANGE(2, 4, T_FLAG,  S_UNSPECIFIED)
    };
    test_greedy("ZWJ + RI flag [greedy]", cps, 4, exp_greedy, 2);
    test_strict("ZWJ + RI flag [strict]", cps, 4, exp_strict, 1);
  }
  // 🇸🇪‍👨 - Flag + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F1F8, 0x1F1EA, 0x200D, 0x1F468};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_FLAG,  S_UNSPECIFIED),
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("RI flag + ZWJ", cps, 4, exp, 2);
  }

  // ── Deviation: tag sequences not accepted as ZWJ elements ─────── 

  // 🏴󠁧󠁢󠁿‍👩 - Tag sequence + ZWJ (TERMINAL has no ZWJ transition)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE007F, 0x200D, 0x1F469};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_TAG,   S_UNSPECIFIED),
      RANGE(5, 6, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Tag sequence + ZWJ (rejected)", cps, 6, exp, 2);
  }
  // 👩‍🏴󠁧󠁢󠁿 - Emoji + ZWJ + tag sequence (tag base not valid as ZWJ target)
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F3F4, 0xE0067, 0xE0062, 0xE007F};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(2, 6, T_TAG,   S_UNSPECIFIED)
    };
    emoji_range_t exp_strict[] = {
      RANGE(2, 6, T_TAG,   S_UNSPECIFIED)
    };
    test_greedy("ZWJ + tag sequence (rejected) [greedy]", cps, 6, exp_greedy, 2);
    test_strict("ZWJ + tag sequence (rejected) [strict]", cps, 6, exp_strict, 1);
  }

  // ── Tag sequences ─────────────────────────────────────────────── 

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England flag (complete tag sequence)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_range_t exp[] = {
      RANGE(0, 7, T_TAG, S_UNSPECIFIED)
    };
    test_both("Complete tag sequence (England)", cps, 7, exp, 1);
  }
  // 🏴‍😀 - TAG_BASE + ZWJ + emoji (tag base as ZWJ element)
  {
    uint32_t cps[] = {0x1F3F4, 0x200D, 0x1F600};
    emoji_range_t exp[] = {
      RANGE(0, 3, T_ZWJ, S_UNSPECIFIED)
    };
    test_both("TAG_BASE + ZWJ + emoji", cps, 3, exp, 1);
  }
  // 🏴️‍😀 - TAG_BASE + VS-16 + ZWJ + emoji
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0x200D, 0x1F600};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_EMOJI)
    };
    test_both("TAG_BASE + VS-16 + ZWJ + emoji", cps, 4, exp, 1);
  }
  // 🏴︎ - TAG_BASE + VS-15
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0E};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT)
    };
    test_both("TAG_BASE + VS-15", cps, 2, exp, 1);
  }
  // TAG_BASE + VS-16 + tag chars + cancel (tag rejected after VS-16)
  {
    uint32_t cps[] = {0x1F3F4, 0xFE0F, 0xE0067, 0xE0062, 0xE007F};
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_EMOJI)
    };
    test_both("TAG_BASE + VS-16 + tags (tag rejected)", cps, 5, exp, 1);
  }
  // 😀 + tag chars + cancel (non-tag base, tags rejected)
  {
    uint32_t cps[] = {0x1F600, 0xE0067, 0xE0062, 0xE007F};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji + tag chars (rejected)", cps, 4, exp, 1);
  }
  // 😀 + cancel tag (non-tag base, cancel rejected)
  {
    uint32_t cps[] = {0x1F600, 0xE007F};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji + cancel tag (rejected)", cps, 2, exp, 1);
  }
  // Tag sequence without cancel tag (greedy emits prefix, strict drops)
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_greedy("Tag without cancel [greedy]", cps, 3, exp_greedy, 1);
    test_strict("Tag without cancel [strict]", cps, 3, NULL, 0);
  }
  // Cancel tag without any tag chars (dead-end pending state)
  {
    uint32_t cps[] = {0x1F3F4, 0xE007F};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_greedy("Cancel tag without specs [greedy]", cps, 2, exp_greedy, 1);
    test_strict("Cancel tag without specs [strict]", cps, 2, NULL, 0);
  }

  // ── Mixed / multi-sequence ────────────────────────────────────── 

  // 😀👍🏻🇺🇸 - Three different types in sequence
  {
    uint32_t cps[] = {0x1F600, 0x1F44D, 0x1F3FB, 0x1F1FA, 0x1F1F8};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC,    S_UNSPECIFIED),
      RANGE(1, 3, T_MODIFIER, S_UNSPECIFIED),
      RANGE(3, 5, T_FLAG,     S_UNSPECIFIED)
    };
    test_both("Multiple sequences (3 types)", cps, 5, exp, 3);
  }
  // 😀🇸🇪🇳 - Emoji, flag, lone RI
  {
    uint32_t cps[] = {0x1F600, 0x1F1F8, 0x1F1EA, 0x1F1F3};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED),
      RANGE(1, 3, T_FLAG,  S_UNSPECIFIED), 
      RANGE(3, 4, T_BASIC, S_UNSPECIFIED)
    };
    test_both("Emoji then flag then lone RI", cps, 4, exp, 3);
  }
  // 1︎⃣ + ☺️ - Keycap VS-15 then emoji VS-16
  {
    uint32_t cps[] = {
      0x0031, 0xFE0E, 0x20E3,
      0x263A, 0xFE0F
    };
    emoji_range_t exp[] = {
      RANGE(0, 3, T_KEYCAP, S_TEXT),
      RANGE(3, 5, T_BASIC,  S_EMOJI)
    };
    test_both("Keycap VS-15 + Emoji VS-16", cps, 5, exp, 2);
  }
  // Text between emoji sequences
  {
    uint32_t cps[] = {
      0x263A, 0xFE0E,  // ☺︎
      0x0041,          // A
      0x263A, 0xFE0F   // ☺️
    };
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(3, 5, T_BASIC, S_EMOJI)
    };
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
    emoji_range_t exp[] = {
      RANGE(0, 2, T_BASIC, S_TEXT),
      RANGE(4, 6, T_BASIC, S_EMOJI)
    };
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

  // ── Incremental scanning (scan_next_*) ────────────────────────── 

  // 👨🏻‍💻 - Single sequence, delivered whole
  {
    uint32_t cps[] = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_UNSPECIFIED)
    };
    test_next_both("Man technologist ZWJ sequence", cps, 4, exp, 1);
  }
  // 😀👍🏻🇺🇸 - Multiple sequences drained across repeated calls
  {
    uint32_t cps[] = {0x1F600, 0x1F44D, 0x1F3FB, 0x1F1FA, 0x1F1F8};
    emoji_range_t exp[] = {
      RANGE(0, 1, T_BASIC,    S_UNSPECIFIED),
      RANGE(1, 3, T_MODIFIER, S_UNSPECIFIED),
      RANGE(3, 5, T_FLAG,     S_UNSPECIFIED)
    };
    test_next_both("Multiple sequences drained in one buffer", cps, 5, exp, 3);
  }
  // 😀A - A boundary already in hand resolves immediately even with
  // eof == false: the codepoint after 😀 proves it can't extend further,
  // regardless of what might still be coming.
  {
    uint32_t cps[] = {0x1F600, 0x0041};
    emoji_range_t exp = RANGE(0, 1, T_BASIC, S_UNSPECIFIED);
    run_next_once("Boundary resolves before eof [next greedy]",
                  emoji_scan_next_greedy, cps, 2, false, &exp);
    run_next_once("Boundary resolves before eof [next strict]",
                  emoji_scan_next_strict, cps, 2, false, &exp);
  }
  // 👨🏻‍ | 💻 - A ZWJ chain split right after the joiner must wait for
  // more input, then resolve once the rest arrives.
  {
    uint32_t prefix[] = {0x1F468, 0x1F3FB, 0x200D};
    uint32_t full[]   = {0x1F468, 0x1F3FB, 0x200D, 0x1F4BB};
    emoji_range_t exp[] = {
      RANGE(0, 4, T_ZWJ, S_UNSPECIFIED)
    };
    run_next_once("ZWJ chain split — prefix withheld [next greedy]",
                  emoji_scan_next_greedy, prefix, 3, false, NULL);
    run_next_once("ZWJ chain split — prefix withheld [next strict]",
                  emoji_scan_next_strict, prefix, 3, false, NULL);
    test_next_both("ZWJ chain split — resolved once complete", full, 4, exp, 1);
  }
  // 🏴gb | seng🏴󠁿 - A tag sequence split before its cancel tag must wait,
  // then resolve once the cancel tag arrives.
  {
    uint32_t prefix[] = {0x1F3F4, 0xE0067, 0xE0062};
    uint32_t full[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_range_t exp[] = {
      RANGE(0, 7, T_TAG, S_UNSPECIFIED)
    };
    run_next_once("Tag sequence split — prefix withheld [next greedy]",
                  emoji_scan_next_greedy, prefix, 3, false, NULL);
    run_next_once("Tag sequence split — prefix withheld [next strict]",
                  emoji_scan_next_strict, prefix, 3, false, NULL);
    test_next_both("Tag sequence split — resolved once complete", full, 7, exp, 1);
  }
  // 👨‍A - Trailing ZWJ at true eof: greedy emits the prefix, strict drops it
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x0041};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_next_greedy("ZWJ then non-emoji at eof [next greedy]", cps, 3, exp_greedy, 1);
    test_next_strict("ZWJ then non-emoji at eof [next strict]", cps, 3, NULL, 0);
  }
  // Tag sequence without a cancel tag at true eof — same divergence
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062};
    emoji_range_t exp_greedy[] = {
      RANGE(0, 1, T_BASIC, S_UNSPECIFIED)
    };
    test_next_greedy("Tag without cancel at eof [next greedy]", cps, 3, exp_greedy, 1);
    test_next_strict("Tag without cancel at eof [next strict]", cps, 3, NULL, 0);
  }
  // Empty input
  {
    uint32_t cps[] = {0};
    test_next_both("Empty input", cps, 0, NULL, 0);
  }
  // All ASCII (no emoji)
  {
    uint32_t cps[] = {0x0041, 0x0042, 0x0043};
    test_next_both("No emoji in input", cps, 3, NULL, 0);
  }

  print_summary();
  return TestsFailed > 0 ? 1 : 0;
}
