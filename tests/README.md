# SWM Tests

This directory contains all test-related files for the SWM window manager.

## Test Files

- **`test.c`** - Core logic unit tests (pure functions)
- **`test_client.c`** - Simple X11 test application for integration tests
- **`test_wm.sh`** - Basic window manager integration tests using Xvfb
- **`test_ultrawide.sh`** - Ultrawide monitor functionality tests
- **`launch_test_terminal.sh`** - Helper script to launch terminals in test environment

## Running Tests

From the project root directory:

### Core Logic Tests
```bash
make test
```

### Integration Tests
```bash
make test-wm              # Basic functionality tests
make test-wm-interactive  # Interactive test environment
```

### Ultrawide Tests
```bash
make test-ultrawide                # Ultrawide functionality tests
make test-ultrawide-interactive    # Interactive ultrawide testing
make test-ultrawide-compare        # Compare regular vs ultrawide
```

### All Tests
```bash
make test-all  # Runs all tests
```

## Interactive Testing

1. Start a test environment:
   ```bash
   make test-wm-interactive
   # or
   make test-ultrawide-interactive
   ```

2. In another terminal, launch applications:
   ```bash
   cd tests
   ./launch_test_terminal.sh
   ```

3. Test SWM commands:
   ```bash
   # From tests/ directory:
   ../swmctl cycle-window-next
   ../swmctl cycle-monitor-right
   
   # Or from project root:
   ./swmctl cycle-window-next
   ./swmctl cycle-monitor-right
   ```

## Test Environment

- Tests use virtual X displays (`:98`, `:99`) to avoid conflicts
- Xvfb provides the virtual X server
- Tests automatically detect available terminal emulators
- All test processes are properly cleaned up on exit

## Dependencies

- `Xvfb` (virtual X server): `sudo pacman -S xorg-server-xvfb`
- `xdpyinfo` (X display info): `sudo pacman -S xorg-xdpyinfo` 