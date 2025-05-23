CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -lX11 -lXinerama

TARGET = swm
SOURCES = swm.c core.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c config.h core.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) test_swm

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	chmod 755 /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

test: test_swm
	@echo "Running tests..."
	./test_swm

test_swm: test.c core.o config.h core.h
	$(CC) $(CFLAGS) test.c core.o -o test_swm $(LDFLAGS)
	@echo "Test binary compiled successfully" 