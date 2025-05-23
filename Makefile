CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -lX11 -lXinerama

TARGET = swm
SOURCES = swm.c core.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install uninstall test test-wm test-ultrawide test-interactive

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c config.h core.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test client for window manager testing
tests/test_client: tests/test_client.c
	$(CC) $(CFLAGS) tests/test_client.c -o tests/test_client -lX11

clean:
	rm -f $(OBJECTS) $(TARGET) tests/test_swm tests/test_client

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET).new
	chmod 755 /usr/local/bin/$(TARGET).new
	mv /usr/local/bin/$(TARGET).new /usr/local/bin/$(TARGET)
	cp swmctl /usr/local/bin/swmctl.new
	chmod 755 /usr/local/bin/swmctl.new
	mv /usr/local/bin/swmctl.new /usr/local/bin/swmctl

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -f /usr/local/bin/swmctl

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Core logic tests
test: tests/test_swm
	@echo "Running core logic tests..."
	./tests/test_swm

tests/test_swm: tests/test.c core.o config.h core.h
	$(CC) $(CFLAGS) -I. tests/test.c core.o -o tests/test_swm $(LDFLAGS)
	@echo "Test binary compiled successfully"

# Window manager integration tests using Xvfb
test-wm: $(TARGET) tests/test_client
	@echo "Running window manager integration tests..."
	@echo "This will test SWM using Xvfb (virtual X server)"
	./tests/test_wm.sh

test-wm-interactive: $(TARGET) tests/test_client
	@echo "Starting interactive window manager test environment..."
	./tests/test_wm.sh -i

# Ultrawide monitor tests
test-ultrawide: $(TARGET) tests/test_client
	@echo "Running ultrawide monitor tests..."
	./tests/test_ultrawide.sh

test-ultrawide-interactive: $(TARGET) tests/test_client
	@echo "Starting interactive ultrawide test environment..."
	./tests/test_ultrawide.sh -i

test-ultrawide-compare: $(TARGET) tests/test_client
	@echo "Comparing regular vs ultrawide monitor behavior..."
	./tests/test_ultrawide.sh -c

# Run all tests
test-all: test test-wm test-ultrawide
	@echo ""
	@echo "✓ All tests completed successfully!"
	@echo "  - Core logic tests: PASSED"
	@echo "  - Window manager integration tests: PASSED"
	@echo "  - Ultrawide monitor tests: PASSED" 