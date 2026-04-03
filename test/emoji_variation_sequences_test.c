/*
 * Tests all emoji and text variation sequences from
 * unicode-data/emoji-variation-sequences.txt against the scanner and
 * presentation style classifier.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "emoji_dfa.h"
#include "emoji_dfa_classify.h"
#include "emoji_scan.h"
#include "emoji_ucd_classify.h"

typedef struct {
  const char*                  name;
  emoji_presentation_style_t   style;
  int                          run;
  int                          passed;
  int                          failed;
} presentation_style_stat_t;

static presentation_style_stat_t Stats[] = {
  {"emoji style", EMOJI_PRESENTATION_EMOJI, 0, 0, 0},
  {"text style",  EMOJI_PRESENTATION_TEXT,  0, 0, 0},
};

#define NUM_STYLES (sizeof(Stats) / sizeof(Stats[0]))

static void print_summary(void) {
  int total_run = 0, total_passed = 0, total_failed = 0;
  printf("========================================================\n");
  printf("Data file: emoji-variation-sequences.txt\n");
  printf("========================================================\n");
  printf("%-32s  %6s  %6s  %6s\n", "Style", "Run", "Passed", "Failed");
  printf("--------------------------------------------------------\n");
  for (size_t i = 0; i < NUM_STYLES; i++) {
    printf("%-32s  %6d  %6d  %6d\n",
           Stats[i].name, Stats[i].run, Stats[i].passed, Stats[i].failed);
    total_run    += Stats[i].run;
    total_passed += Stats[i].passed;
    total_failed += Stats[i].failed;
  }
  printf("--------------------------------------------------------\n");
  printf("%-32s  %6d  %6d  %6d\n", "Total", total_run, total_passed, total_failed);
  printf("========================================================\n\n");
}

static bool style_field_to_stat(const char* style, presentation_style_stat_t** out) {
  for (size_t i = 0; i < NUM_STYLES; i++) {
    if (strcmp(style, Stats[i].name) == 0) {
      *out = &Stats[i];
      return true;
    }
  }
  return false;
}

static void test_variation_sequence(uint32_t base,
                                    uint32_t vs,
                                    presentation_style_stat_t* stat,
                                    int lineno) {
  stat->run++;

  uint32_t cps[2] = {base, vs};
  emoji_scan_range_t out[1];
  size_t n = emoji_scan_strict(cps, 2, out, 1);
  if (n != 1 || out[0].start != 0 || out[0].end != 1) {
    stat->failed++;
    printf("FAIL [%s]: scanner rejected U+%04X U+%04X at line %d\n",
           stat->name, base, vs, lineno);
    return;
  }

  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  uint32_t bitmask = 0;
  for (size_t i = 0; i < 2; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(cps[i]);
    state = emoji_dfa_step_record(state, klass, &bitmask);
  }

  emoji_sequence_type_t got_type = emoji_dfa_classify_type(bitmask);
  if (got_type != EMOJI_SEQUENCE_BASIC) {
    stat->failed++;
    printf("FAIL [%s]: type got %d expected %d (U+%04X U+%04X) at line %d\n",
           stat->name, got_type, EMOJI_SEQUENCE_BASIC, base, vs, lineno);
    return;
  }

  emoji_presentation_style_t got_style = emoji_dfa_classify_style(bitmask);
  if (got_style != stat->style) {
    stat->failed++;
    printf("FAIL [%s]: style got %d expected %d (U+%04X U+%04X) at line %d\n",
           stat->name, got_style, stat->style, base, vs, lineno);
    return;
  }

  stat->passed++;
}

int main(void) {
  FILE* f = fopen("unicode-data/emoji-variation-sequences.txt", "r");
  if (!f) {
    fprintf(stderr, "Cannot open unicode-data/emoji-variation-sequences.txt\n");
    return 2;
  }

  char line[512];
  int lineno = 0;
  while (fgets(line, sizeof(line), f)) {
    lineno++;

    char first;
    if (sscanf(line, " %c", &first) != 1 || first == '#')
      continue;

    {
      uint32_t base, vs;
      if (sscanf(line, " %X %X", &base, &vs) != 2)
        goto parse_fail;

      const char* semi = strchr(line, ';');
      if (!semi)
        goto parse_fail;

      char style[32];
      if (sscanf(semi + 1, " %31[^;]", style) != 1)
        goto parse_fail;

      size_t slen = strlen(style);
      while (slen > 0 && style[slen - 1] == ' ')
        style[--slen] = '\0';

      presentation_style_stat_t* stat;
      if (!style_field_to_stat(style, &stat))
        goto parse_fail;

      test_variation_sequence(base, vs, stat, lineno);
      continue;
    }
  parse_fail:
    fprintf(stderr, "Parse failure on line %d: %s", lineno, line);
  }
  fclose(f);

  int total_failed = 0;
  for (size_t i = 0; i < NUM_STYLES; i++)
    total_failed += Stats[i].failed;

  print_summary();
  return total_failed > 0 ? 1 : 0;
}
