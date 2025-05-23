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

### Focus Control
- **Directional Window Focus**: Cycle forward/backward through windows on the current logical monitor
- **Directional Monitor Focus**: Cycle left/right between logical monitors/zones
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

### Uninstallation
```bash
sudo make uninstall
```

## Usage

### Starting the Window Manager
```bash
./swm
```

**Note**: Make sure no other window manager is running. You may need to kill your current window manager first.

### Controlling the Window Manager

#### Using swmctl (Recommended)
The `swmctl` script provides an easy way to send commands:

**Window Cycling:**
```bash
./swmctl cycle-window-next    # or: ./swmctl cwn
./swmctl cycle-window-prev    # or: ./swmctl cwp
./swmctl cycle-window         # or: ./swmctl cw (defaults to next)
```

**Monitor/Zone Cycling:**
```bash
./swmctl cycle-monitor-right  # or: ./swmctl cmr
./swmctl cycle-monitor-left   # or: ./swmctl cml
./swmctl cycle-monitor        # or: ./swmctl cm (defaults to right)
```

**Other Commands:**
```bash
./swmctl quit
```

#### Using sxhkd (Recommended Setup)
1. Install sxhkd: `sudo pacman -S sxhkd` (Arch) or equivalent
2. Copy the example configuration:
   ```bash
   mkdir -p ~/.config/sxhkd
   cp sxhkdrc.example ~/.config/sxhkd/sxhkdrc
   ```
3. Start sxhkd: `sxhkd &`
4. Use the configured key bindings:

**Window Management:**
   - `Super + Tab`: Next window
   - `Super + Shift + Tab`: Previous window
   - `Super + j`: Alternative next window
   - `Super + k`: Alternative previous window

**Monitor/Zone Management:**
   - `Super + grave`: Right zone
   - `Super + Shift + grave`: Left zone
   - `Super + l`: Alternative right zone
   - `Super + h`: Alternative left zone

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

# Other commands
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 1  # Legacy window cycle
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 2  # Legacy monitor cycle
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 7  # Quit
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
| `CMD_QUIT` | 7 | Quit window manager |

## Testing

The window manager includes comprehensive tests for core logic. To run tests:
```bash
make test
```

Tests cover:
- Monitor zone calculation (regular and ultrawide)
- Directional window cycling (forward/backward)
- Directional zone cycling (left/right)
- Edge cases (single window, single zone, wrap-around)

To run with debug output:
```bash
make debug
./swm
```

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

## License

This project follows the suckless philosophy of simplicity and minimalism. Feel free to modify and distribute according to your needs.