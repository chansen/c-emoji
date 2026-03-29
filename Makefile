CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2

HEADERS := emoji_class.h emoji_dfa_classify.h emoji_dfa.h \
					 emoji_scan.h emoji_ucd.h emoji_ucd_classify.h

TEST_SOURCES := emoji_scan_test.c
TEST_BINARY := emoji_scan_test

.PHONY: all test clean

all: test

$(TEST_BINARY): $(TEST_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES)

test: $(TEST_BINARY)
	@echo ""
	@echo "Running tests..."
	@echo "========================================="
	@./$(TEST_BINARY)

clean:
	rm -f $(TEST_BINARY)
