CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g
TARGET = simpledb
SOURCES = main.c kv.c io.c
OBJECTS = $(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) test.db

test: $(TARGET)
	./$(TARGET)

.PHONY: clean test