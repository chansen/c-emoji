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

/* Unicode emoji sequence scanner (UTS #51 v17.0.0).
 *
 * Scans an array of codepoints and returns the position ranges of all emoji 
 * sequences found.
 *
 * Two scanning modes are provided:
 *
 *   emoji_scan_strict() - conservative.  Only emits a sequence when the next
 *     codepoint cleanly terminates it.  Incomplete sequences at end of input
 *     (e.g. a ZWJ with no following emoji, or a tag sequence without a cancel
 *     tag) are silently dropped.
 *
 *   emoji_scan_greedy() - permissive.  Emits the longest valid prefix seen
 *     whenever a boundary is reached, even if non-emoji codepoints followed
 *     the last valid position.  Incomplete sequences at end of input are
 *     emitted up to the last accepting state.  Prefer this when scanning a
 *     complete buffer and partial sequences at the end should not be lost.
 *
 * Output sequences are written to out[] as {start, end} index pairs, both
 * inclusive, into the codepoints array.  If more sequences are found than
 * max_out allows, the excess are silently discarded.
 *
 * Neither function performs semantic validation.  Sequences that are
 * structurally valid but have no defined rendering (e.g. two arbitrary emoji
 * joined by ZWJ with no RGI combination) are emitted without error.  Callers
 * requiring strict RGI conformance must validate emitted sequences against
 * the emoji-sequences.txt and emoji-zwj-sequences.txt data files.
 */
#ifndef EMOJI_SCAN_H
#define EMOJI_SCAN_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "emoji_scan.h"
#include "emoji_dfa.h"
#include "emoji_ucd_classify.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t start;
  size_t end;
} emoji_sequence_t;

static inline size_t emoji_scan_strict(uint32_t* codepoints,
                                       size_t len,
                                       emoji_sequence_t* out,
                                       size_t max_out) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  size_t start = 0, count = 0;

  for (size_t i = 0; i < len && count < max_out; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(codepoints[i]);
    emoji_dfa_state_t next = emoji_dfa_step(state, klass);

    if (emoji_dfa_is_boundary(next)) {
      if (emoji_dfa_is_accepting(state)) {
        out[count++] = (emoji_sequence_t){
          .start = start,
          .end   = i - 1
        };
      }
      state = emoji_dfa_step(EMOJI_DFA_STATE_START, klass);
      start = emoji_dfa_is_start(state) ? i + 1 : i;
    } else {
      state = next;
    }
  }

  if (start < len && count < max_out && emoji_dfa_is_accepting(state)) {
    out[count++] = (emoji_sequence_t){
      .start = start, 
      .end   = len - 1
    };
  }
  return count;
}

static inline size_t emoji_scan_greedy(uint32_t* codepoints,
                                       size_t len,
                                       emoji_sequence_t* out,
                                       size_t max_out) {
  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  size_t start = 0, end = 0, count = 0;
  bool has_accept = false;

  for (size_t i = 0; i < len && count < max_out; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(codepoints[i]);
    emoji_dfa_state_t next = emoji_dfa_step(state, klass);

    if (emoji_dfa_is_boundary(next)) {
      if (has_accept) {
        out[count++] = (emoji_sequence_t){
          .start = start, 
          .end   = end
        };
        has_accept = false;
      }
      state = emoji_dfa_step(EMOJI_DFA_STATE_START, klass);
      start = emoji_dfa_is_start(state) ? i + 1 : i;
    } else {
      state = next;
    }

    if (emoji_dfa_is_accepting(state)) {
      has_accept = true;
      end = i;
    }
  }

  if (has_accept && count < max_out) {
    out[count++] = (emoji_sequence_t){
      .start = start, 
      .end   = end
    };
  }
  return count;
}

#ifdef __cplusplus
}
#endif
#endif /* EMOJI_SCAN_H */
