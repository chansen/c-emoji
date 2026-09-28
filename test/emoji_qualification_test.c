/*
 * Tests for emoji_qualification_resolve(), emoji_qualification_rewrite_unqualified(),
 * and emoji_qualification_rewrite_fully().
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "emoji_qualification.h"
#include "emoji_types.h"
#include "emoji_span.h"

static int TestsRun = 0;
static int TestsPassed = 0;
static int TestsFailed = 0;

#define SPAN(cps, len, type, style) \
  emoji_span_from_type_style((cps), (len), (type), (style))

static bool cps_equal(const uint32_t* got, size_t got_n,
                      const uint32_t* exp, size_t exp_n) {
  if (got_n != exp_n)
    return false;
  for (size_t i = 0; i < got_n; i++)
    if (got[i] != exp[i])
      return false;
  return true;
}

static void print_cps_diff(const uint32_t* got, size_t got_n,
                           const uint32_t* exp, size_t exp_n) {
  size_t max = got_n > exp_n ? got_n : exp_n;
  for (size_t i = 0; i < max; i++) {
    if (i < got_n)
      printf("    [%zu] got: U+%04X\n", i, got[i]);
    if (i < exp_n)
      printf("    [%zu] exp: U+%04X\n", i, exp[i]);
  }
}

static void test_resolve(const char* name,
                         emoji_span_t span,
                         emoji_qualification_status_t exp) {
  emoji_qualification_status_t got = emoji_qualification_resolve(span);
  TestsRun++;
  if (got == exp) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s\n", name);
    printf("    got: %d exp: %d\n", got, exp);
  }
}

static void test_transform(const char* name,
                           size_t (*fn)(emoji_span_t, uint32_t*, size_t),
                           emoji_span_t span,
                           const uint32_t* exp, size_t exp_n) {
  uint32_t out[16];
  size_t got_n = fn(span, out, 16);
  bool ok = cps_equal(out, got_n, exp, exp_n);
  TestsRun++;
  if (ok) {
    TestsPassed++;
    printf("PASS - %s\n", name);
  } else {
    TestsFailed++;
    printf("FAIL - %s\n", name);
    print_cps_diff(out, got_n, exp, exp_n);
  }
}

static void test_unqualify(const char* name, emoji_span_t span,
                           const uint32_t* exp, size_t exp_n) {
  test_transform(name, emoji_qualification_rewrite_unqualified, span, exp, exp_n);
}

static void test_fully_qualify(const char* name, emoji_span_t span,
                               const uint32_t* exp, size_t exp_n) {
  test_transform(name, emoji_qualification_rewrite_fully, span, exp, exp_n);
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

enum {
  T_BASIC    = EMOJI_SEQUENCE_BASIC,
  T_KEYCAP   = EMOJI_SEQUENCE_KEYCAP,
  T_FLAG     = EMOJI_SEQUENCE_FLAG,
  T_TAG      = EMOJI_SEQUENCE_TAG,
  T_MODIFIER = EMOJI_SEQUENCE_MODIFIER,
  T_ZWJ      = EMOJI_SEQUENCE_ZWJ,

  S_UNSPECIFIED = EMOJI_PRESENTATION_UNSPECIFIED,
  S_TEXT        = EMOJI_PRESENTATION_TEXT,
  S_EMOJI       = EMOJI_PRESENTATION_EMOJI,

  Q_FULL    = EMOJI_QUALIFICATION_FULLY_QUALIFIED,
  Q_MINIMAL = EMOJI_QUALIFICATION_MINIMALLY_QUALIFIED,
  Q_NONE    = EMOJI_QUALIFICATION_UNQUALIFIED,
};

int main(void) {
  // ── BASIC / KEYCAP: no MINIMAL possible, only FULL or NONE ──────

  // 😀 - already Emoji_Presentation=Yes, no selector needed
  {
    uint32_t cps[] = {0x1F600};
    emoji_span_t span = SPAN(cps, 1, T_BASIC, S_UNSPECIFIED);
    test_resolve("Default-emoji BASIC, no VS -> FULL", span, Q_FULL);
  }

  // © - text-default, no VS at all
  {
    uint32_t cps[] = {0x00A9};
    emoji_span_t span = SPAN(cps, 1, T_BASIC, S_UNSPECIFIED);
    test_resolve("Text-default BASIC, no VS -> NONE", span, Q_NONE);
  }

  // ☺︎ - text-default + VS-15 (requests text; doesn't count as qualified)
  {
    uint32_t cps[] = {0x263A, 0xFE0E};
    emoji_span_t span = SPAN(cps, 2, T_BASIC, S_TEXT);
    test_resolve("Text-default BASIC + VS-15 -> NONE", span, Q_NONE);
  }

  // ☺️ - text-default + VS-16
  {
    uint32_t cps[] = {0x263A, 0xFE0F};
    emoji_span_t span = SPAN(cps, 2, T_BASIC, S_EMOJI);
    test_resolve("Text-default BASIC + VS-16 -> FULL", span, Q_FULL);
  }

  // 1⃣ - keycap base, no VS
  {
    uint32_t cps[] = {0x0031, 0x20E3};
    emoji_span_t span = SPAN(cps, 2, T_KEYCAP, S_UNSPECIFIED);
    test_resolve("Keycap, no VS -> NONE", span, Q_NONE);
  }

  // 1︎⃣ - keycap base + VS-15 + term
  {
    uint32_t cps[] = {0x0031, 0xFE0E, 0x20E3};
    emoji_span_t span = SPAN(cps, 3, T_KEYCAP, S_TEXT);
    test_resolve("Keycap + VS-15 -> NONE", span, Q_NONE);
  }

  // 1️⃣ - keycap base + VS-16 + term
  {
    uint32_t cps[] = {0x0031, 0xFE0F, 0x20E3};
    emoji_span_t span = SPAN(cps, 3, T_KEYCAP, S_EMOJI);
    test_resolve("Keycap + VS-16 -> FULL", span, Q_FULL);
  }

  // ── FLAG / TAG / MODIFIER: always FULL, hardcoded by type ───────

  // 🇺🇸
  {
    uint32_t cps[] = {0x1F1FA, 0x1F1F8};
    emoji_span_t span = SPAN(cps, 2, T_FLAG, S_UNSPECIFIED);
    test_resolve("RI flag pair -> FULL", span, Q_FULL);
  }

  // 🏴󠁧󠁢󠁥󠁮󠁧󠁿 - England
  {
    uint32_t cps[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_span_t span = SPAN(cps, 7, T_TAG, S_UNSPECIFIED);
    test_resolve("Complete tag sequence -> FULL", span, Q_FULL);
  }

  // 👦🏻
  {
    uint32_t cps[] = {0x1F466, 0x1F3FB};
    emoji_span_t span = SPAN(cps, 2, T_MODIFIER, S_UNSPECIFIED);
    test_resolve("Modifier sequence -> FULL", span, Q_FULL);
  }

  // ✌🏻 - Victory Hand is text-default AND a modifier base; the modifier
  // absorbs the position regardless.
  {
    uint32_t cps[] = {0x270C, 0x1F3FB};
    emoji_span_t span = SPAN(cps, 2, T_MODIFIER, S_UNSPECIFIED);
    test_resolve("Text-default modifier base + modifier -> still FULL", span, Q_FULL);
  }

  // ── ZWJ: needs per-element inspection; MINIMAL only reachable here ──

  // 👨‍👩‍👧 - every element already Emoji_Presentation=Yes
  {
    uint32_t cps[] = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    emoji_span_t span = SPAN(cps, 5, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ family, all default-emoji -> FULL", span, Q_FULL);
  }

  // "woman ZWJ heart(no VS) ZWJ woman" - the textbook minimally-qualified
  // shape: first element qualified, a middle one isn't.
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x2764, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(cps, 5, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ couple-with-heart, no VS on heart -> MINIMAL", span, Q_MINIMAL);
  }

  // Same shape, heart carries its own VS-16 this time.
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x2764, 0xFE0F, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(cps, 6, T_ZWJ, S_EMOJI);
    test_resolve("ZWJ couple-with-heart, VS-16 on heart -> FULL", span, Q_FULL);
  }

  // "heart(no VS) ZWJ woman" - FIRST element missing is the one rule
  // that demotes all the way to NONE, even though the rest is fine.
  {
    uint32_t cps[] = {0x2764, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(cps, 3, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ, first element missing selector -> NONE", span, Q_NONE);
  }

  // Constructed: "woman ZWJ woman ZWJ heart(no VS)" - missing selector on
  // the LAST element, not the first. Per the file header this should
  // still be MINIMAL, not NONE.
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x1F469, 0x200D, 0x2764};
    emoji_span_t span = SPAN(cps, 5, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ, last (non-first) element missing -> still MINIMAL", span, Q_MINIMAL);
  }

  // Constructed: Victory Hand + modifier as a non-final ZWJ element -
  // exercises the has_modifier branch inside resolve_zwj() itself, not
  // just the type-level MODIFIER hardcode above.
  {
    uint32_t cps[] = {0x270C, 0x1F3FB, 0x200D, 0x1F600};
    emoji_span_t span = SPAN(cps, 4, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ, text-default+modifier element -> FULL", span, Q_FULL);
  }

  // Same leading element, no modifier this time - now it does need its
  // own selector, and doesn't have one.
  {
    uint32_t cps[] = {0x270C, 0x200D, 0x1F600};
    emoji_span_t span = SPAN(cps, 3, T_ZWJ, S_UNSPECIFIED);
    test_resolve("ZWJ, text-default element w/o modifier or VS -> NONE", span, Q_NONE);
  }

  // style is documented as ignored for ZWJ - same codepoints as the
  // MINIMAL case above, but pass S_EMOJI instead of S_UNSPECIFIED.
  {
    uint32_t cps[] = {0x1F469, 0x200D, 0x2764, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(cps, 5, T_ZWJ, S_EMOJI);
    test_resolve("ZWJ ignores style param -> still MINIMAL", span, Q_MINIMAL);
  }

  // ── unqualify(): strips VS-16 only, unconditionally ─────────────

  {
    uint32_t src[] = {0x263A, 0xFE0F};
    uint32_t exp[] = {0x263A};
    emoji_span_t span = SPAN(src, 2, T_BASIC, S_EMOJI);
    test_unqualify("Unqualify strips VS-16", span, exp, 1);
  }

  // Fully-qualified heart chain -> minimally-qualified heart chain.
  {
    uint32_t src[] = {0x1F469, 0x200D, 0x2764, 0xFE0F, 0x200D, 0x1F469};
    uint32_t exp[] = {0x1F469, 0x200D, 0x2764, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(src, 6, T_ZWJ, S_EMOJI);
    test_unqualify("Unqualify ZWJ heart chain", span, exp, 5);
  }

  // ── fully_qualify(): inserts/replaces VS-16 wherever required ───

  {
    uint32_t src[] = {0x263A};
    uint32_t exp[] = {0x263A, 0xFE0F};
    emoji_span_t span = SPAN(src, 1, T_BASIC, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify bare text-default BASIC", span, exp, 2);
  }

  // Already default-emoji: nothing to add.
  {
    uint32_t src[] = {0x1F600};
    uint32_t exp[] = {0x1F600};
    emoji_span_t span = SPAN(src, 1, T_BASIC, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify default-emoji BASIC is a no-op", span, exp, 1);
  }

  // VS-15 gets replaced, not kept alongside a new VS-16.
  {
    uint32_t src[] = {0x263A, 0xFE0E};
    uint32_t exp[] = {0x263A, 0xFE0F};
    emoji_span_t span = SPAN(src, 2, T_BASIC, S_TEXT);
    test_fully_qualify("Fully-qualify recomputes fresh over an existing VS-15", span, exp, 2);
  }

  {
    uint32_t src[] = {0x0031, 0x20E3};
    uint32_t exp[] = {0x0031, 0xFE0F, 0x20E3};
    emoji_span_t span = SPAN(src, 2, T_KEYCAP, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify bare keycap", span, exp, 3);
  }

  // FLAG / TAG / MODIFIER: pass through unchanged (no selector to add).
  {
    uint32_t src[] = {0x1F1FA, 0x1F1F8};
    emoji_span_t span = SPAN(src, 2, T_FLAG, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify flag is a no-op", span, src, 2);
  }
  {
    uint32_t src[] = {0x1F3F4, 0xE0067, 0xE0062, 0xE0065, 0xE006E, 0xE0067, 0xE007F};
    emoji_span_t span = SPAN(src, 7, T_TAG, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify tag sequence is a no-op", span, src, 7);
  }
  {
    uint32_t src[] = {0x1F466, 0x1F3FB};
    emoji_span_t span = SPAN(src, 2, T_MODIFIER, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify modifier sequence is a no-op", span, src, 2);
  }

  // Minimally-qualified heart chain -> fully-qualified heart chain
  // (pairs with the unqualify() test above going the other way).
  {
    uint32_t src[] = {0x1F469, 0x200D, 0x2764, 0x200D, 0x1F469};
    uint32_t exp[] = {0x1F469, 0x200D, 0x2764, 0xFE0F, 0x200D, 0x1F469};
    emoji_span_t span = SPAN(src, 5, T_ZWJ, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify ZWJ heart chain", span, exp, 6);
  }

  // A redundant selector on an already-default-emoji base gets dropped,
  // *and* a required one elsewhere in the same chain still gets added -
  // net effect is same length, different shape, not a straight append.
  {
    uint32_t src[] = {0x1F600, 0xFE0F, 0x200D, 0x2764};
    uint32_t exp[] = {0x1F600, 0x200D, 0x2764, 0xFE0F};
    emoji_span_t span = SPAN(src, 4, T_ZWJ, S_EMOJI);
    test_fully_qualify("Fully-qualify drops a redundant VS-16, adds a required one", span, exp, 4);
  }

  // Victory Hand + modifier inside a ZWJ chain: already fine, no-op.
  {
    uint32_t src[] = {0x270C, 0x1F3FB, 0x200D, 0x1F600};
    emoji_span_t span = SPAN(src, 4, T_ZWJ, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify text-default+modifier ZWJ element is a no-op", span, src, 4);
  }

  // Victory Hand without a modifier: now it does need its own VS-16.
  {
    uint32_t src[] = {0x270C, 0x200D, 0x1F600};
    uint32_t exp[] = {0x270C, 0xFE0F, 0x200D, 0x1F600};
    emoji_span_t span = SPAN(src, 3, T_ZWJ, S_UNSPECIFIED);
    test_fully_qualify("Fully-qualify text-default ZWJ element w/o modifier inserts VS-16", span, exp, 4);
  }

  // Capacity contract: returns required length, never writes past dst_capacity
  {
    uint32_t src[] = {0x263A, 0xFE0E};
    uint32_t out[1];
    emoji_span_t span = SPAN(src, 2, T_BASIC, S_TEXT);
    size_t n = emoji_qualification_rewrite_fully(span, out, 1);
    TestsRun++;
    if (n == 2 && out[0] == 0x263A) {
      TestsPassed++;
      printf("PASS - rewrite_fully reports required length\n");
    }
    else {
      TestsFailed++;
      printf("FAIL - rewrite_fully reports required length (n=%zu)\n", n);
    }
  }

  print_summary();
  return TestsFailed > 0 ? 1 : 0;
}
