CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude
SRCDIR = src
INCDIR = include
BUILDDIR = build
TARGET = $(BUILDDIR)/simpledb

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILDDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/