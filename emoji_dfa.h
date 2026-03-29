/*
 * Copyright (c) 2026 Christian Hansen <chansen@cpan.org>
 * <https://github.com/chansen/c-emoji>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* DFA core for Unicode emoji sequence recognition (UTS #51 v17.0.0).
 *
 * STATE CATEGORIES:
 *
 * Every state is exactly one of:
 *
 *   Accepting  (EMOJI, MODIFIER_BASE, MODIFIER, VS15, VS16) - a complete
 *              sequence ends here but may still be extended by further input.
 *   Pending    (KEYCAP_BASE, RI1, TAG, ZWJ) - inside a valid prefix; no
 *              complete sequence yet.
 *   Boundary   (REJECT) - the current attempt is over; the codepoint that
 *              caused this state must be retried from START, since it may
 *              open the next sequence.
 *
 * <https://www.unicode.org/reports/tr51/tr51-29.html>
 */
#ifndef EMOJI_DFA_H
#define EMOJI_DFA_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EMOJI_DFA_STATE_REJECT = 0,
  EMOJI_DFA_STATE_START,
  EMOJI_DFA_STATE_KEYCAP_BASE,
  EMOJI_DFA_STATE_RI1,
  EMOJI_DFA_STATE_EMOJI,
  EMOJI_DFA_STATE_MODIFIER_BASE,
  EMOJI_DFA_STATE_MODIFIER,
  EMOJI_DFA_STATE_VS15,
  EMOJI_DFA_STATE_VS16,
  EMOJI_DFA_STATE_TAG,
  EMOJI_DFA_STATE_ZWJ,
  EMOJI_DFA_STATE_COUNT
} emoji_dfa_state_t;

typedef enum {
  EMOJI_DFA_CLASS_OTHER = 0,
  EMOJI_DFA_CLASS_EMOJI,            // Property Emoji
  EMOJI_DFA_CLASS_MODIFIER_BASE,    // Property Emoji_Modifier_Base
  EMOJI_DFA_CLASS_MODIFIER,         // Property Emoji_Modifier
  EMOJI_DFA_CLASS_VS15,
  EMOJI_DFA_CLASS_VS16,
  EMOJI_DFA_CLASS_RI,               // Property Regional_Indicator
  EMOJI_DFA_CLASS_KEYCAP_BASE,
  EMOJI_DFA_CLASS_KEYCAP_TERM,
  EMOJI_DFA_CLASS_TAG_SPEC,
  EMOJI_DFA_CLASS_TAG_TERM,
  EMOJI_DFA_CLASS_ZWJ,
  EMOJI_DFA_CLASS_COUNT
} emoji_dfa_class_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(EMOJI_DFA_STATE_REJECT == 0, "EMOJI_DFA_STATE_REJECT must be zero");
#endif

/* clang-format off */
static const emoji_dfa_state_t
emoji_dfa_table[EMOJI_DFA_STATE_COUNT][EMOJI_DFA_CLASS_COUNT] = {
  [EMOJI_DFA_STATE_START] = {
    [EMOJI_DFA_CLASS_OTHER]         = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_MODIFIER]      = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_VS15]          = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_VS16]          = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_TAG_SPEC]      = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_TAG_TERM]      = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_KEYCAP_TERM]   = EMOJI_DFA_STATE_START,
    [EMOJI_DFA_CLASS_ZWJ]           = EMOJI_DFA_STATE_START,

    [EMOJI_DFA_CLASS_RI]            = EMOJI_DFA_STATE_RI1,
    [EMOJI_DFA_CLASS_EMOJI]         = EMOJI_DFA_STATE_EMOJI,
    [EMOJI_DFA_CLASS_MODIFIER_BASE] = EMOJI_DFA_STATE_MODIFIER_BASE,
    [EMOJI_DFA_CLASS_KEYCAP_BASE]   = EMOJI_DFA_STATE_KEYCAP_BASE,
  },
  [EMOJI_DFA_STATE_KEYCAP_BASE] = {
    [EMOJI_DFA_CLASS_VS15]          = EMOJI_DFA_STATE_VS15,
    [EMOJI_DFA_CLASS_VS16]          = EMOJI_DFA_STATE_VS16,
    [EMOJI_DFA_CLASS_KEYCAP_TERM]   = EMOJI_DFA_STATE_EMOJI,
  },
  [EMOJI_DFA_STATE_RI1] = {
    [EMOJI_DFA_CLASS_RI]            = EMOJI_DFA_STATE_EMOJI,
  },
  [EMOJI_DFA_STATE_EMOJI] = {
    [EMOJI_DFA_CLASS_VS15]          = EMOJI_DFA_STATE_VS15,
    [EMOJI_DFA_CLASS_VS16]          = EMOJI_DFA_STATE_VS16,
    [EMOJI_DFA_CLASS_TAG_SPEC]      = EMOJI_DFA_STATE_TAG,
    [EMOJI_DFA_CLASS_ZWJ]           = EMOJI_DFA_STATE_ZWJ,
  },
  [EMOJI_DFA_STATE_MODIFIER_BASE] = {
    [EMOJI_DFA_CLASS_VS15]          = EMOJI_DFA_STATE_VS15,
    [EMOJI_DFA_CLASS_VS16]          = EMOJI_DFA_STATE_VS16,
    [EMOJI_DFA_CLASS_TAG_SPEC]      = EMOJI_DFA_STATE_TAG,
    [EMOJI_DFA_CLASS_ZWJ]           = EMOJI_DFA_STATE_ZWJ,
    [EMOJI_DFA_CLASS_MODIFIER]      = EMOJI_DFA_STATE_MODIFIER,
  },
  [EMOJI_DFA_STATE_MODIFIER] = {
    [EMOJI_DFA_CLASS_ZWJ]           = EMOJI_DFA_STATE_ZWJ,
  },
  [EMOJI_DFA_STATE_VS15] = {
    [EMOJI_DFA_CLASS_KEYCAP_TERM]   = EMOJI_DFA_STATE_EMOJI,
  },
  [EMOJI_DFA_STATE_VS16] = {
    [EMOJI_DFA_CLASS_KEYCAP_TERM]   = EMOJI_DFA_STATE_EMOJI,
    [EMOJI_DFA_CLASS_TAG_SPEC]      = EMOJI_DFA_STATE_TAG,
    [EMOJI_DFA_CLASS_ZWJ]           = EMOJI_DFA_STATE_ZWJ,
  },
  [EMOJI_DFA_STATE_TAG] = {
    [EMOJI_DFA_CLASS_TAG_SPEC]      = EMOJI_DFA_STATE_TAG,
    [EMOJI_DFA_CLASS_TAG_TERM]      = EMOJI_DFA_STATE_EMOJI,
  },
  [EMOJI_DFA_STATE_ZWJ] = {
    [EMOJI_DFA_CLASS_RI]            = EMOJI_DFA_STATE_RI1,
    [EMOJI_DFA_CLASS_EMOJI]         = EMOJI_DFA_STATE_EMOJI,
    [EMOJI_DFA_CLASS_MODIFIER_BASE] = EMOJI_DFA_STATE_MODIFIER_BASE,
  }
};
/* clang-format on */

static inline bool emoji_dfa_is_accepting(emoji_dfa_state_t state) {
  return state == EMOJI_DFA_STATE_EMOJI ||
         state == EMOJI_DFA_STATE_MODIFIER_BASE ||
         state == EMOJI_DFA_STATE_MODIFIER ||
         state == EMOJI_DFA_STATE_VS15 ||
         state == EMOJI_DFA_STATE_VS16;
}

static inline bool emoji_dfa_is_boundary(emoji_dfa_state_t state) {
  return state == EMOJI_DFA_STATE_REJECT;
}

static inline bool emoji_dfa_is_start(emoji_dfa_state_t state) {
  return state == EMOJI_DFA_STATE_START;
}

static inline emoji_dfa_state_t emoji_dfa_step(emoji_dfa_state_t state,
                                               emoji_dfa_class_t klass) {
  return emoji_dfa_table[state][klass];
}

static inline emoji_dfa_state_t emoji_dfa_step_record(emoji_dfa_state_t state,
                                                      emoji_dfa_class_t klass,
                                                      uint32_t* recorded_classes) {
  state = emoji_dfa_step(state, klass);
  if (state != EMOJI_DFA_STATE_START && state != EMOJI_DFA_STATE_REJECT)
    *recorded_classes |= 1u << klass;
  return state;
}

#ifdef __cplusplus
}
#endif
#endif // EMOJI_DFA_H
