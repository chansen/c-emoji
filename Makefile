CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2

HEADERS := emoji_dfa.h emoji_ucd.h emoji_scan.h 

TEST_SOURCES := emoji_scan_test.c
TEST_BINARY := emoji_scan_test

.PHONY: all test clean

all: test

$(TEST_BINARY): $(TEST_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES)

test: $(TEST_BINARY)
	@echo ""
	@echo "Running tests..."
	@echo "========================================="
	@./$(TEST_BINARY)

clean:
	rm -f $(TEST_BINARY)
