# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
INCLUDES = -I./include
SRCDIR = src
TESTDIR = test
OBJDIR = obj
BINDIR = bin
INCDIR = include

# Create directories if they don't exist
$(shell mkdir -p $(OBJDIR) $(BINDIR))

# Source files - exclude main.c for library objects
SOURCES = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Main program objects (including main.c)
MAIN_SOURCES = $(wildcard $(SRCDIR)/*.c)
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_BINARIES = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/%)

# Main targets
.PHONY: all clean test tests help main test_debug

all: $(TEST_BINARIES) main

# Build main program
main: $(BINDIR)/simpledb

$(BINDIR)/simpledb: $(MAIN_OBJECTS)
	@echo "Building main program $@..."
	$(CC) $(CFLAGS) $(MAIN_OBJECTS) -o $@

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build test binaries (exclude main.o)
$(BINDIR)/%: $(TESTDIR)/%.c $(OBJECTS)
	@echo "Building test $@..."
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJECTS) -o $@

# Individual test targets
test_index: $(BINDIR)/test_index
	@echo "Running index tests..."
	./$(BINDIR)/test_index

benchmark_index: $(BINDIR)/benchmark_index
	@echo "Running performance benchmark..."
	./$(BINDIR)/benchmark_index

test_debug: $(BINDIR)/test_index_debug
	@echo "Running debug test..."
	./$(BINDIR)/test_index_debug

# Run all tests
test: tests
tests: $(TEST_BINARIES)
	@echo "Running all tests..."
	@echo "===================="
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		echo "Running $$test..."; \
		echo "--------------------"; \
		./$$test; \
		if [ $$? -ne 0 ]; then \
			echo "❌ Test $$test FAILED"; \
			exit 1; \
		fi; \
	done
	@echo ""
	@echo "🎉 All tests completed successfully!"

# Debug build
debug: CFLAGS += -DDEBUG -g3 -O0
debug: clean all

# Release build
release: CFLAGS += -DNDEBUG -O3
release: clean all

# Valgrind memory check
valgrind: $(BINDIR)/test_index
	@echo "Running memory leak check..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(BINDIR)/test_index

# Static analysis with cppcheck (if available)
static-analysis:
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "Running static analysis..."; \
		cppcheck --enable=all --std=c99 $(SRCDIR)/*.c $(TESTDIR)/*.c; \
	else \
		echo "cppcheck not found. Install with: brew install cppcheck"; \
	fi

# Code formatting with clang-format (if available)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting code..."; \
		find $(SRCDIR) $(TESTDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		echo "Code formatted!"; \
	else \
		echo "clang-format not found. Install with: brew install clang-format"; \
	fi

# Generate compilation database for IDE support
compile_commands.json: $(MAIN_OBJECTS)
	@echo "Generating compile_commands.json..."
	@echo '[' > compile_commands.json
	@for src in $(MAIN_SOURCES); do \
		echo '  {' >> compile_commands.json; \
		echo '    "directory": "'$(PWD)'",' >> compile_commands.json; \
		echo '    "command": "$(CC) $(CFLAGS) $(INCLUDES) -c '$$src'",' >> compile_commands.json; \
		echo '    "file": "'$$src'"' >> compile_commands.json; \
		echo '  },' >> compile_commands.json; \
	done
	@sed -i '' '$$s/,$$//' compile_commands.json  # Remove last comma
	@echo ']' >> compile_commands.json

# Clean build artifacts
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.db compile_commands.json
	rm -f test*.db benchmark.db

# Deep clean (including any leftover test databases)
distclean: clean
	find . -name "*.db" -delete
	rm -f core dump.*

# Show file sizes
sizes: all
	@echo "Binary sizes:"
	@ls -lh $(BINDIR)/*

# Quick test (just the main functionality)
quick-test: $(BINDIR)/test_index
	@echo "Running quick test..."
	./$(BINDIR)/test_index

# Performance test only
perf-test: $(BINDIR)/benchmark_index
	@echo "Running performance test..."
	./$(BINDIR)/benchmark_index

# Install dependencies (macOS with Homebrew)
install-deps:
	@echo "Installing development dependencies..."
	@if command -v brew >/dev/null 2>&1; then \
		brew install valgrind cppcheck clang-format || true; \
	else \
		echo "Homebrew not found. Please install manually:"; \
		echo "- valgrind: for memory leak detection"; \
		echo "- cppcheck: for static analysis"; \
		echo "- clang-format: for code formatting"; \
	fi

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build all tests and main program"
	@echo "  main          - Build main program only"
	@echo "  test/tests    - Run all tests"
	@echo "  test_index    - Build and run index tests only"
	@echo "  test_debug    - Build and run debug test"
	@echo "  benchmark_index - Build and run benchmark only"
	@echo "  quick-test    - Run basic functionality test"
	@echo "  perf-test     - Run performance benchmark"
	@echo "  debug         - Build with debug symbols"
	@echo "  release       - Build optimized release version"
	@echo "  valgrind      - Run memory leak check"
	@echo "  static-analysis - Run cppcheck static analysis"
	@echo "  format        - Format code with clang-format"
	@echo "  compile_commands.json - Generate compilation database"
	@echo "  sizes         - Show binary sizes"
	@echo "  install-deps  - Install development dependencies"
	@echo "  clean         - Clean build artifacts"
	@echo "  distclean     - Deep clean including test databases"
	@echo "  help          - Show this help message"

# Dependencies
$(OBJDIR)/io.o: $(INCDIR)/io.h $(INCDIR)/index.h
$(OBJDIR)/kv.o: $(INCDIR)/kv.h $(INCDIR)/io.h $(INCDIR)/index.h
$(OBJDIR)/index.o: $(INCDIR)/index.h
$(OBJDIR)/main.o: $(INCDIR)/kv.h $(INCDIR)/io.h $(INCDIR)/index.h

$(BINDIR)/test_index_debug: $(OBJDIR)/kv.o $(OBJDIR)/io.o $(OBJDIR)/index.o
$(BINDIR)/test_index: $(OBJDIR)/kv.o $(OBJDIR)/io.o $(OBJDIR)/index.o
$(BINDIR)/benchmark_index: $(OBJDIR)/kv.o $(OBJDIR)/io.o $(OBJDIR)/index.o