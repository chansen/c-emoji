/*
 * Tests all RGI emoji sequences from unicode-data/emoji-sequences.txt and
 * unicode-data/emoji-zwj-sequences.txt against the scanner and sequence 
 * classifier.
 *
 * Each sequence is fed through emoji_scan_strict() and verified accepted,
 * then classified via emoji_dfa_step_record() and verified against the
 * type_field in the data file.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "emoji_dfa.h"
#include "emoji_dfa_classify.h"
#include "emoji_scan.h"
#include "emoji_ucd_classify.h"

typedef struct {
  const char*           name;
  emoji_sequence_type_t type;
  int                   run;
  int                   passed;
  int                   failed;
} sequence_type_stat_t;

static sequence_type_stat_t Stats[] = {
    {"Basic_Emoji",                 EMOJI_SEQUENCE_BASIC,    0, 0, 0},
    {"Emoji_Keycap_Sequence",       EMOJI_SEQUENCE_KEYCAP,   0, 0, 0},
    {"RGI_Emoji_Modifier_Sequence", EMOJI_SEQUENCE_MODIFIER, 0, 0, 0},
    {"RGI_Emoji_Flag_Sequence",     EMOJI_SEQUENCE_FLAG,     0, 0, 0},
    {"RGI_Emoji_Tag_Sequence",      EMOJI_SEQUENCE_TAG,      0, 0, 0},
    {"RGI_Emoji_ZWJ_Sequence",      EMOJI_SEQUENCE_ZWJ,      0, 0, 0},
};

#define NUM_TYPES (sizeof(Stats) / sizeof(Stats[0]))

static void print_summary(void) {
  int total_run = 0, total_passed = 0, total_failed = 0;
  printf("\n========================================================\n");
  printf("Data files: emoji-sequences.txt\n");
  printf("            emoji-zwj-sequences.txt\n");
  printf("========================================================\n");
  printf("%-32s  %6s  %6s  %6s\n", "Sequence Type", "Run", "Passed", "Failed");
  printf("--------------------------------------------------------\n");
  for (size_t i = 0; i < NUM_TYPES; i++) {
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

static bool type_field_to_stat(const char* tf, sequence_type_stat_t** out) {
  for (size_t i = 0; i < NUM_TYPES; i++) {
    if (strcmp(tf, Stats[i].name) == 0) {
      *out = &Stats[i];
      return true;
    }
  }
  return false;
}

static void test_sequence(uint32_t* cps,
                          size_t len,
                          sequence_type_stat_t* stat,
                          int lineno) {
  stat->run++;

  emoji_scan_range_t out[1];
  size_t n = emoji_scan_strict(cps, len, out, 1);
  if (n != 1 || out[0].start != 0 || out[0].end != len - 1) {
    stat->failed++;
    printf("FAIL [%s]: scanner rejected U+%04X .. at line %d\n",
           stat->name, cps[0], lineno);
    return;
  }

  emoji_dfa_state_t state = EMOJI_DFA_STATE_START;
  uint32_t bitmask = 0, accepted_bitmask = 0;
  for (size_t i = 0; i < len; i++) {
    emoji_dfa_class_t klass = emoji_ucd_classify(cps[i]);
    state = emoji_dfa_step_record(state, klass, &bitmask);
    if (emoji_dfa_is_accepting(state))
      accepted_bitmask = bitmask;
  }

  emoji_sequence_type_t got = emoji_dfa_classify_type(accepted_bitmask);
  if (got != stat->type) {
    stat->failed++;
    printf("FAIL [%s]: classify_type got %d expected %d (U+%04X ..) at line %d\n",
           stat->name, got, stat->type, cps[0], lineno);
    return;
  }

  stat->passed++;
}

static void process_file(FILE* f, const char* filename) {
  char line[512];
  int lineno = 0;
  while (fgets(line, sizeof(line), f)) {
    lineno++;

    char first;
    if (sscanf(line, " %c", &first) != 1 || first == '#')
      continue;

    {
      const char* semi = strchr(line, ';');
      if (!semi)
        goto parse_fail;

      char type_field[64];
      if (sscanf(semi + 1, " %63[^ ;]", type_field) != 1)
        goto parse_fail;

      sequence_type_stat_t* stat;
      if (!type_field_to_stat(type_field, &stat))
        goto parse_fail;

      // Check for a range (e.g. "231A..231B ; Basic_Emoji")
      char token[32];
      if (sscanf(line, " %31s", token) != 1)
        goto parse_fail;

      char* dotdot = strstr(token, "..");
      if (dotdot) {
        *dotdot = '\0';
        uint32_t cp_start = (uint32_t)strtoul(token,      NULL, 16);
        uint32_t cp_end   = (uint32_t)strtoul(dotdot + 2, NULL, 16);
        for (uint32_t cp = cp_start; cp <= cp_end; cp++)
          test_sequence(&cp, 1, stat, lineno);
        continue;
      }

      // Multi-codepoint sequence
      uint32_t cps[32];
      size_t len = 0;
      const char* p = line;
      while (len < 32) {
        unsigned cp_raw;
        int n = 0;
        if (sscanf(p, " %X%n", &cp_raw, &n) != 1)
          break;
        cps[len++] = (uint32_t)cp_raw;
        p += n;
        if (*p == ';')
          break;
      }

      if (len == 0)
        goto parse_fail;

      test_sequence(cps, len, stat, lineno);
      continue;
    }
  parse_fail:
    fprintf(stderr, "Parse failure in %s on line %d: %s", filename, lineno, line);
  }
}

int main(void) {
  static const char* files[] = {
      "unicode-data/emoji-sequences.txt",
      "unicode-data/emoji-zwj-sequences.txt",
  };

  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    FILE* f = fopen(files[i], "r");
    if (!f) {
      fprintf(stderr, "Cannot open %s\n", files[i]);
      return 2;
    }
    process_file(f, files[i]);
    fclose(f);
  }

  int total_failed = 0;
  for (size_t i = 0; i < NUM_TYPES; i++)
    total_failed += Stats[i].failed;

  print_summary();
  return total_failed > 0 ? 1 : 0;
}
