CC     := cc
CFLAGS := -I. -std=c99 -Wall -Wextra -Wpedantic -O2

HEADERS := emoji_types.h emoji_dfa_classify.h emoji_dfa.h \
           emoji_scan.h emoji_ucd.h emoji_ucd_classify.h \
           emoji_presentation.h

TEST_BINARIES := emoji_scan_test emoji_dfa_classify_test \
                 emoji_presentation_test \
                 emoji_data_test emoji_sequences_test \
                 emoji_variation_sequences_test emoji_test_test

.PHONY: all test clean

all: test

%_test: test/%_test.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TEST_BINARIES)
	@echo ""
	@echo "Running tests..."
	@echo "========================================="
	@for t in $(TEST_BINARIES); do ./$$t || exit 1; done

clean:
	rm -f $(TEST_BINARIES)
