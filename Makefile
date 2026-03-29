CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2

HEADERS := emoji_class.h emoji_dfa_classify.h emoji_dfa.h \
					 emoji_scan.h emoji_ucd.h emoji_ucd_classify.h

TEST_SOURCES := emoji_scan_test.c emoji_dfa_classify_test.c
TEST_BINARIES := emoji_scan_test emoji_dfa_classify_test

.PHONY: all test clean

all: test
	
%_test: %_test.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TEST_BINARIES)
	@echo ""
	@echo "Running tests..."
	@echo "========================================="
	@./emoji_scan_test
	@./emoji_dfa_classify_test

clean:
	rm -f $(TEST_BINARIES)
