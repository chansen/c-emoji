/*
 * Tests Emoji, Emoji_Presentation, Emoji_Modifier and Emoji_Modifier_Base
 * property coverage from unicode-data/emoji-data.txt against the predicates 
 * in emoji_ucd.h.
 */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include "emoji_ucd.h"

typedef bool (*predicate_t)(uint32_t);

typedef struct {
  const char* name;
  predicate_t pred;
  int         tested;
  int         passed;
  int         failed;
} property_stat_t;

static property_stat_t Stats[] = {
    {"Emoji",               emoji_ucd_is_emoji,         0, 0, 0},
    {"Emoji_Presentation",  emoji_ucd_is_presentation,  0, 0, 0},
    {"Emoji_Modifier",      emoji_ucd_is_modifier,      0, 0, 0},
    {"Emoji_Modifier_Base", emoji_ucd_is_modifier_base, 0, 0, 0},
};

#define NUM_PROPS (sizeof(Stats) / sizeof(Stats[0]))

static void print_summary(void) {
  int total_tested = 0, total_passed = 0, total_failed = 0;
  printf("\n========================================================\n");
  printf("Data file: emoji-data.txt\n");
  printf("========================================================\n");
  printf("%-32s  %6s  %6s  %6s\n", "Property", "Tested", "Passed", "Failed");
  printf("--------------------------------------------------------\n");
  for (size_t i = 0; i < NUM_PROPS; i++) {
    printf("%-32s  %6d  %6d  %6d\n",
           Stats[i].name, Stats[i].tested, Stats[i].passed, Stats[i].failed);
    total_tested += Stats[i].tested;
    total_passed += Stats[i].passed;
    total_failed += Stats[i].failed;
  }
  printf("--------------------------------------------------------\n");
  printf("%-32s  %6d  %6d  %6d\n", "Total", total_tested, total_passed, total_failed);
  printf("========================================================\n\n");
}

static bool property_to_stat(const char* prop, property_stat_t** out) {
  for (size_t i = 0; i < NUM_PROPS; i++) {
    if (strcmp(prop, Stats[i].name) == 0) {
      *out = &Stats[i];
      return true;
    }
  }
  return false;
}

static void test_range(uint32_t cp_start,
                       uint32_t cp_end,
                       property_stat_t* stat,
                       int lineno) {
  for (uint32_t cp = cp_start; cp <= cp_end; cp++) {
    stat->tested++;
    if (stat->pred(cp)) {
      stat->passed++;
    } else {
      stat->failed++;
      printf("FAIL [%s]: U+%04X returned false at line %d\n",
             stat->name, cp, lineno);
    }
  }
}

int main(void) {
  FILE* f = fopen("unicode-data/emoji-data.txt", "r");
  if (!f) {
    fprintf(stderr, "Cannot open unicode-data/emoji-data.txt\n");
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
      char token[32];
      uint32_t cp_start, cp_end;
      if (sscanf(line, " %31s", token) != 1)
        goto parse_fail;
      char* dotdot = strstr(token, "..");
      if (dotdot) {
        *dotdot = '\0';
        cp_start = (uint32_t)strtoul(token, NULL, 16);
        cp_end   = (uint32_t)strtoul(dotdot + 2, NULL, 16);
      } else {
        cp_start = cp_end = (uint32_t)strtoul(token, NULL, 16);
      }

      const char* semi = strchr(line, ';');
      if (!semi)
        goto parse_fail;

      char prop[64];
      if (sscanf(semi + 1, " %63[^ ;]", prop) != 1)
        goto parse_fail;

      property_stat_t* stat;
      if (!property_to_stat(prop, &stat))
        continue;

      test_range(cp_start, cp_end, stat, lineno);
      continue;
    }
  parse_fail:
    fprintf(stderr, "Parse failure on line %d: %s", lineno, line);
  }
  fclose(f);

  int total_failed = 0;
  for (size_t i = 0; i < NUM_PROPS; i++)
    total_failed += Stats[i].failed;

  print_summary();
  return total_failed > 0 ? 1 : 0;
}
