CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
INCLUDES = -I./include

SRCDIR = src
TESTDIR = test
OBJDIR = obj
BINDIR = bin

# Create required output directories
$(shell mkdir -p $(OBJDIR) $(BINDIR))

# Find all source and test files
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TESTDIR)/%.c=$(BINDIR)/%)

# Objects to link with tests 
LIB_OBJS = $(filter-out $(OBJDIR)/main.o, $(OBJS))

.PHONY: all clean test test_index test_wal benchmark_index

all: main $(TEST_BINS)

main: $(BINDIR)/simpledb

# Build the main program
$(BINDIR)/simpledb: $(OBJS)
	@echo "Building $@..."
	$(CC) $(CFLAGS) $^ -o $@

# Compile source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build executables
$(BINDIR)/%: $(TESTDIR)/%.c $(LIB_OBJS)
	@echo "Building test $@..."
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB_OBJS) -o $@

# test runners
test_index: $(BINDIR)/test_index
	@echo "Running index tests..."
	./$(BINDIR)/test_index

test_wal: $(BINDIR)/test_wal
	@echo "Running WAL tests..."
	./$(BINDIR)/test_wal

benchmark_index: $(BINDIR)/benchmark_index
	@echo "Running performance benchmark..."
	./$(BINDIR)/benchmark_index

test: $(TEST_BINS)
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "--------------------"; \
		./$$test || exit 1; \
	done
	@echo "🎉 All tests completed successfully!"

clean:
	@echo "Cleaning build files and databases..."
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.db *.db.wal
