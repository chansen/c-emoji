/*
 * Tests all emoji sequences from unicode-data/emoji-test.txt.
 *
 * Each sequence is scanned with emoji_scan_next_strict() and the
 * resulting span resolved via emoji_qualification_resolve(); the
 * resolved status must match the file's fully-qualified /
 * minimally-qualified / unqualified label.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "emoji_scan.h"
#include "emoji_qualification.h"

typedef struct {
  const char* name;
  int         run;
  int         passed;
  int         failed;
} qualification_stat_t;

static qualification_stat_t Stats[] = {
    {"fully-qualified",     0, 0, 0},
    {"minimally-qualified", 0, 0, 0},
    {"unqualified",         0, 0, 0},
    {"component",           0, 0, 0},
};

#define NUM_QUALS (sizeof(Stats) / sizeof(Stats[0]))

static void print_summary(void) {
  int total_run = 0, total_passed = 0, total_failed = 0;

  printf("\n========================================================\n");
  printf("Data file: emoji-test.txt\n");
  printf("========================================================\n");
  printf("%-32s  %6s  %6s  %6s\n", "Qualification", "Run", "Passed", "Failed");
  printf("--------------------------------------------------------\n");
  for (size_t i = 0; i < NUM_QUALS; i++) {
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

static bool qualification_to_stat(const char* qual, qualification_stat_t** out) {
  for (size_t i = 0; i < NUM_QUALS; i++) {
    if (strcmp(qual, Stats[i].name) == 0) {
      *out = &Stats[i];
      return true;
    }
  }
  return false;
}

/* "component" has no matching emoji_qualification_status_t value. Returns
 * false for it (and for anything unrecognized), true with *out set for the
 * three resolve() actually distinguishes. */
static bool status_to_expected(const char* status, emoji_qualification_status_t* out) {
  if (strcmp(status, "fully-qualified") == 0) {
    *out = EMOJI_QUALIFICATION_FULLY_QUALIFIED;
    return true;
  }
  if (strcmp(status, "minimally-qualified") == 0) {
    *out = EMOJI_QUALIFICATION_MINIMALLY_QUALIFIED;
    return true;
  }
  if (strcmp(status, "unqualified") == 0) {
    *out = EMOJI_QUALIFICATION_UNQUALIFIED;
    return true;
  }
  return false;
}

static void test_sequence(uint32_t* cps,
                          size_t len,
                          qualification_stat_t* stat,
                          int lineno) {
  stat->run++;

  emoji_span_t out;
  size_t position = 0;
  if (!emoji_scan_next_strict(cps, len, &out, &position, true) ||
      out.src != cps || out.len != len || position != len) {
    stat->failed++;
    printf("FAIL [%s]: scanner rejected U+%04X .. at line %d\n",
           stat->name, cps[0], lineno);
    return;
  }

  emoji_qualification_status_t expected;
  if (!status_to_expected(stat->name, &expected)) {
    // Component lines map to no expected status; a 1/1 scan above is all
    // they can be tested for.
    stat->passed++;
    return;
  }

  emoji_qualification_status_t got = emoji_qualification_resolve(out);
  if (got != expected) {
    stat->failed++;
    printf("FAIL [%s]: U+%04X .. resolved %d, expected %d, at line %d\n",
           stat->name, cps[0], got, expected, lineno);
    return;
  }

  stat->passed++;
}

int main(void) {
  FILE* f = fopen("unicode-data/emoji-test.txt", "r");
  if (!f) {
    fprintf(stderr, "Cannot open unicode-data/emoji-test.txt\n");
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
      const char* semi = strchr(line, ';');
      if (!semi)
        goto parse_fail;

      char qual[32];
      if (sscanf(semi + 1, " %31[^ ;#]", qual) != 1)
        goto parse_fail;

      qualification_stat_t* stat;
      if (!qualification_to_stat(qual, &stat))
        goto parse_fail;

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
    fprintf(stderr, "Parse failure on line %d: %s", lineno, line);
  }
  fclose(f);

  int total_failed = 0;
  for (size_t i = 0; i < NUM_QUALS; i++)
    total_failed += Stats[i].failed;

  print_summary();
  return total_failed > 0 ? 1 : 0;
}

