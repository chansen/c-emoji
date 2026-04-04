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

/* Sequence classification from a recorded bitmask.
 *
 * These functions interpret a bitmask snapshot taken at the last accepting
 * state during a call sequence to emoji_dfa_step_record(). Passing the live
 * accumulator after the sequence has ended produces meaningless results.
 *
 * emoji_dfa_classify_type() priority order is load-bearing: a ZWJ sequence
 * containing a modifier will have multiple class bits set; the ordering
 * ensures the dominant structural feature wins.
 *
 * emoji_dfa_classify_style() returns DEFAULT when no variation selector was
 * seen. The caller must consult the Emoji_Presentation property to determine
 * the actual rendering for text-default codepoints.
 */
#ifndef EMOJI_DFA_CLASSIFY_H
#define EMOJI_DFA_CLASSIFY_H
#include <stddef.h>
#include <stdint.h>

#include "emoji_class.h"
#include "emoji_dfa.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline emoji_sequence_type_t emoji_dfa_classify_type(uint32_t recorded_bitmask) {
  // Check low 16 bits for class
  if (recorded_bitmask & (1u << EMOJI_DFA_CLASS_ZWJ))
    return EMOJI_SEQUENCE_ZWJ;
  if (recorded_bitmask & (1u << EMOJI_DFA_CLASS_TAG_SPEC))
    return EMOJI_SEQUENCE_TAG;
  // Check high 16 bits for state
  if ((recorded_bitmask & (1u << (EMOJI_DFA_STATE_RI + 16))) &&
      (recorded_bitmask & (1u << (EMOJI_DFA_STATE_TERMINAL + 16))))
    return EMOJI_SEQUENCE_FLAG;
  if ((recorded_bitmask & (1u << EMOJI_DFA_CLASS_MODIFIER_BASE)) &&
      (recorded_bitmask & (1u << EMOJI_DFA_CLASS_MODIFIER)))
    return EMOJI_SEQUENCE_MODIFIER;
  if (recorded_bitmask & (1u << EMOJI_DFA_CLASS_KEYCAP_TERM))
    return EMOJI_SEQUENCE_KEYCAP;
  return EMOJI_SEQUENCE_BASIC;
}

static inline emoji_presentation_style_t emoji_dfa_classify_style(uint32_t recorded_bitmask) {
  if (recorded_bitmask & (1u << EMOJI_DFA_CLASS_VS15))
    return EMOJI_PRESENTATION_TEXT;
  if (recorded_bitmask & (1u << EMOJI_DFA_CLASS_VS16))
    return EMOJI_PRESENTATION_EMOJI;
  return EMOJI_PRESENTATION_DEFAULT;
}

#ifdef __cplusplus
}
#endif
#endif // EMOJI_DFA_CLASSIFY_H
