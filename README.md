# SWM - Simple Window Manager

A minimalist X11 window manager with multi-monitor and ultrawide support, following the suckless philosophy.

## Features

### Multi-Monitor & Ultrawide Support
- Detects and uses multiple physical monitors via Xinerama
- Automatically splits very wide monitors (>5000 pixels) into three logical zones (¼–½–¼) for better tiling/management
- Treats regular monitors as single logical zones

### Basic Window Management
- Manages client windows, placing them onto logical monitors/zones
- New windows open on the currently active logical monitor
- Windows are sized to fit their assigned logical monitor/zone
- Highlights the focused window with a distinct border color
- Graceful window closing with fallback to force kill

### Focus Control
- **Directional Window Focus**: Cycle forward/backward through windows on the current logical monitor
- **Directional Monitor Focus**: Cycle left/right between logical monitors/zones
- **Window Termination**: Kill the currently focused window
- Intuitive directional controls with proper wrap-around

### External Command Interface
- Basic commands (like cycling focus, changing active logical monitor) can be triggered by external scripts via a root window property
- Designed to work with sxhkd for keyboard shortcuts

### Minimalist Design
- Aims for simplicity and efficiency, following a "suckless" approach
- Core logic built with testable, pure functions where possible
- Contained primarily in `swm.c` and `core.c`

## Building

### Dependencies
- libX11
- libXinerama
- A C99 compatible compiler (gcc recommended)

### Compilation
```bash
make
```

For debug build:
```bash
make debug
```

### Installation
```bash
sudo make install
```

This installs both `swm` and `swmctl` to `/usr/local/bin/`, making them available system-wide.

### Uninstallation
```bash
sudo make uninstall
```

This removes both `swm` and `swmctl` from the system.

## Usage

### Starting the Window Manager
```bash
./swm
```

**Note**: Make sure no other window manager is running. You may need to kill your current window manager first.

### Controlling the Window Manager

#### Using swmctl (Recommended)
The `swmctl` script provides an easy way to send commands. After installation, you can use `swmctl` directly. During development, use `./swmctl`.

**Window Cycling:**
```bash
swmctl cycle-window-next    # or: swmctl cwn
swmctl cycle-window-prev    # or: swmctl cwp
swmctl cycle-window         # or: swmctl cw (defaults to next)
```

**Monitor/Zone Cycling:**
```bash
swmctl cycle-monitor-right  # or: swmctl cmr
swmctl cycle-monitor-left   # or: swmctl cml
swmctl cycle-monitor        # or: swmctl cm (defaults to right)
```

**Window Management:**
```bash
swmctl kill-window          # or: swmctl kw
swmctl move-window-left     # or: swmctl mwl
swmctl move-window-right    # or: swmctl mwr
```

**Other Commands:**
```bash
swmctl quit
```

#### Using sxhkd (Recommended Setup)
1. Install sxhkd: `sudo pacman -S sxhkd` (Arch) or equivalent
2. Copy the example configuration:
   ```bash
   mkdir -p ~/.config/sxhkd
   cp sxhkdrc.example ~/.config/sxhkd/sxhkdrc
   ```
   **Note**: The example configuration assumes `swmctl` is installed system-wide. If running from the development directory, update the commands to use `./swmctl` instead.
3. Start sxhkd: `sxhkd &`
4. Use the configured key bindings:

**Window Management:**
   - `Super + Tab`: Next window
   - `Super + Shift + Tab`: Previous window
   - `Super + j`: Alternative next window
   - `Super + k`: Alternative previous window
   - `Super + Shift + c`: Kill focused window

**Monitor/Zone Management:**
   - `Super + grave`: Right zone
   - `Super + Shift + grave`: Left zone
   - `Super + l`: Alternative right zone
   - `Super + h`: Alternative left zone

**Window Movement:**
   - `Super + Shift + h`: Move focused window to left monitor/zone
   - `Super + Shift + l`: Move focused window to right monitor/zone

**System:**
   - `Super + Shift + q`: Quit window manager

#### Direct X11 Property Method
You can also send commands directly using xprop:
```bash
# Window cycling
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 3  # Next window
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 4  # Previous window

# Monitor cycling
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 6  # Right zone
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 5  # Left zone

# Window management
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 7  # Kill focused window

# Move window between monitors
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 9  # Move window left
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 10 # Move window right

# Other commands
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 1  # Legacy window cycle
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 2  # Legacy monitor cycle
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 8  # Quit
```

## Configuration

Edit `config.h` to customize:

- **Border colors and width**: `FOCUS_COLOR`, `UNFOCUS_COLOR`, `BORDER_WIDTH`
- **Ultrawide threshold**: `ULTRAWIDE_THRESHOLD` (default: 5000px)
- **Zone ratios**: `ZONE_LEFT_RATIO`, `ZONE_CENTER_RATIO`, `ZONE_RIGHT_RATIO`
- **Command property name**: `COMMAND_PROPERTY`

After editing `config.h`, recompile with `make clean && make`.

## Architecture

### Core Components
- **swm.c**: Main window manager implementation
- **core.c**: Pure functions for window management logic
- **core.h**: Data structures and function prototypes
- **config.h**: Configuration constants and settings
- **swmctl**: Helper script for sending commands
- **Makefile**: Build system

### Design Principles
- Pure functions for core logic (no X11 dependencies in logic functions)
- Minimal global state
- Simple data structures
- Clear separation between X11 interface and window management logic
- Directional consistency: left/previous = -1, right/next = 1

### Data Structures
- **LogicalZone**: Represents a logical monitor zone with geometry
- **Client**: Represents a managed window
- **WindowManager**: Global state container

### Command Interface
The window manager responds to the following commands via the `_SWM_COMMAND` root window property:

| Command | Value | Description |
|---------|-------|-------------|
| `CMD_CYCLE_WINDOW` | 1 | Legacy window cycling (forward) |
| `CMD_CYCLE_MONITOR` | 2 | Legacy monitor cycling (right) |
| `CMD_CYCLE_WINDOW_NEXT` | 3 | Cycle to next window |
| `CMD_CYCLE_WINDOW_PREV` | 4 | Cycle to previous window |
| `CMD_CYCLE_MONITOR_LEFT` | 5 | Cycle to left zone |
| `CMD_CYCLE_MONITOR_RIGHT` | 6 | Cycle to right zone |
| `CMD_KILL_WINDOW` | 7 | Kill currently focused window |
| `CMD_QUIT` | 8 | Quit window manager |
| `CMD_MOVE_WINDOW_LEFT` | 9 | Move focused window to left zone |
| `CMD_MOVE_WINDOW_RIGHT` | 10 | Move focused window to right zone |

### Window Termination
The kill window function implements a graceful termination approach:
1. **First attempt**: Send `WM_DELETE_WINDOW` message if the window supports it
2. **Fallback**: Use `XKillClient()` to force terminate unresponsive windows

This ensures well-behaved applications can clean up properly while still handling misbehaving applications.

## Testing

The window manager includes comprehensive tests for core logic and full integration testing using Xvfb (virtual X server). This allows you to test SWM without disrupting your current window manager.

The test suite covers:
- **Core Logic**: Zone calculation, window cycling, and window movement logic
- **Integration Tests**: Full window manager functionality including window movement between zones
- **Ultrawide Support**: Multi-zone window management and movement on ultrawide monitors
- **Command Interface**: All swmctl commands including move-window-left and move-window-right

All test files are located in the `tests/` directory. See [`tests/README.md`](tests/README.md) for detailed testing documentation.

### Quick Start

```bash
# Run all tests
make test-all

# Core logic tests only
make test

# Basic integration tests
make test-wm

# Ultrawide monitor tests
make test-ultrawide

# Interactive testing
make test-wm-interactive
make test-ultrawide-interactive
```

### Test Dependencies

The integration tests require:
- `Xvfb` (virtual X server): `sudo pacman -S xorg-server-xvfb`
- `xdpyinfo` (X display info): `sudo pacman -S xorg-xdpyinfo`

The tests automatically detect available terminal emulators and use the best one available.

## Troubleshooting

### "Cannot open display"
Make sure you're running from within an X11 session and `$DISPLAY` is set.

### "Xinerama not active"
Xinerama might not be enabled. Check your X11 configuration or try running on a system with multiple monitors.

### Window manager doesn't start
Another window manager might be running. Kill it first:
```bash
killall dwm  # or whatever WM you're using
./swm
```

### Commands don't work
Make sure sxhkd is running and the swmctl script is executable:
```bash
pgrep sxhkd  # Should show a process ID
chmod +x swmctl
```

### Directional cycling feels backwards
The directional logic follows this convention:
- **Windows**: Previous (-1) ← → Next (1)
- **Monitors**: Left (-1) ← → Right (1)

You can customize key bindings in your sxhkd configuration to match your preferences.

### Window won't close
The kill window function tries graceful termination first, then force kills. If a window still won't close, it may be a system-level issue or the application may be completely frozen.