CC     := cc
CFLAGS := -I. -std=c99 -Wall -Wextra -Wpedantic -O2

HEADERS := emoji_dfa.h \
           emoji_dfa_classify.h \
           emoji_presentation.h \
           emoji_range.h \
           emoji_scan.h \
           emoji_span.h \
           emoji_types.h \
           emoji_ucd.h \
           emoji_ucd_builtin.h \
           emoji_ucd_classify.h \
           emoji_ucd_icu.h

TEST_BINARIES := emoji_data_test \
                 emoji_dfa_classify_test \
                 emoji_presentation_test \
                 emoji_scan_test \
                 emoji_sequences_test \
                 emoji_test_test \
                 emoji_variation_sequences_test


ICU_TEST_BINARIES := $(addsuffix _icu,$(TEST_BINARIES))

ICU_CFLAGS := $(shell pkg-config --cflags icu-uc 2>/dev/null)
ICU_LIBS   := $(shell pkg-config --libs icu-uc 2>/dev/null)

.PHONY: all test test-icu clean

all: test

%_test: test/%_test.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

%_test_icu: test/%_test.c $(HEADERS)
	$(CC) $(CFLAGS) -DEMOJI_UCD_USE_ICU $(ICU_CFLAGS) -o $@ $< $(ICU_LIBS)

test: $(TEST_BINARIES)
	@echo ""
	@echo "Running tests (builtin UCD backend)..."
	@echo "========================================="
	@for t in $(TEST_BINARIES); do ./$$t || exit 1; done

test-icu:
	@pkg-config --exists icu-uc || { echo "ICU4C not found (pkg-config icu-uc missing). Install libicu-dev or your platform's ICU4C dev package."; exit 1; }
	$(MAKE) $(ICU_TEST_BINARIES)
	@echo ""
	@echo "Running tests (ICU4C UCD backend)..."
	@echo "========================================="
	@for t in $(ICU_TEST_BINARIES); do ./$$t || exit 1; done

clean:
	rm -f $(TEST_BINARIES) $(ICU_TEST_BINARIES)
