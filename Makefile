CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
INCLUDES = -I./include

SRCDIR = src
TESTDIR = test
OBJDIR = obj
BINDIR = bin
TSAN_OBJDIR = obj_tsan
TSAN_BINDIR = bin_tsan
TSAN_CFLAGS = -Wall -Wextra -std=c99 -g -O1 -fsanitize=thread -fno-omit-frame-pointer

# Create required output directories
$(shell mkdir -p $(OBJDIR) $(BINDIR) $(TSAN_OBJDIR) $(TSAN_BINDIR))

# Find all source and test files
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TESTDIR)/%.c=$(BINDIR)/%)

# Objects to link with tests 
LIB_OBJS = $(filter-out $(OBJDIR)/main.o, $(OBJS))
TSAN_OBJS = $(SRCS:$(SRCDIR)/%.c=$(TSAN_OBJDIR)/%.o)
TSAN_LIB_OBJS = $(filter-out $(TSAN_OBJDIR)/main.o, $(TSAN_OBJS))

.PHONY: all clean test test_index test_wal benchmark_index test_tsan

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

$(TSAN_OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling TSAN $<..."
	$(CC) $(TSAN_CFLAGS) $(INCLUDES) -c $< -o $@

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

$(TSAN_BINDIR)/multithreaded_wal_test: $(TESTDIR)/multithreaded_wal_test.c $(TSAN_LIB_OBJS)
	@echo "Building TSAN test $@..."
	$(CC) $(TSAN_CFLAGS) $(INCLUDES) $< $(TSAN_LIB_OBJS) -o $@

test_tsan: $(TSAN_BINDIR)/multithreaded_wal_test
	@echo "Running ThreadSanitizer WAL test..."
	./$(TSAN_BINDIR)/multithreaded_wal_test

test: $(TEST_BINS)
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "--------------------"; \
		./$$test || exit 1; \
	done
	@echo "All tests completed successfully!"

clean:
	@echo "Cleaning build files and databases..."
	rm -rf $(OBJDIR) $(BINDIR) $(TSAN_OBJDIR) $(TSAN_BINDIR)
	rm -f *.db *.db.wal
