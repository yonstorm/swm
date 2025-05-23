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
- **Cycle Window Focus**: Allows cycling keyboard focus through the windows on the current logical monitor
- **Cycle Logical Monitor Focus**: Allows changing which logical monitor (or zone) is active, moving focus to a window on it

### External Command Interface
- Basic commands (like cycling focus, changing active logical monitor) can be triggered by external scripts via a root window property
- Designed to work with sxhkd for keyboard shortcuts

### Minimalist Design
- Aims for simplicity and efficiency, following a "suckless" approach
- Core logic built with testable, pure functions where possible
- Contained primarily in `swm.c` and `config.h`

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

```bash
./swmctl cycle-window    # or: ./swmctl cw
./swmctl cycle-monitor   # or: ./swmctl cm
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
   - `Super + Tab`: Cycle windows on current zone
   - `Super + Grave`: Cycle between zones/monitors
   - `Super + j`: Alternative window cycling
   - `Super + k`: Alternative monitor cycling
   - `Super + Shift + q`: Quit window manager

#### Direct X11 Property Method
You can also send commands directly using xprop:
```bash
# Cycle windows
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 1

# Cycle monitors
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 2

# Quit
xprop -root -f _SWM_COMMAND 32i -set _SWM_COMMAND 3
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
- **config.h**: Configuration constants and settings
- **swmctl**: Helper script for sending commands
- **Makefile**: Build system

### Design Principles
- Pure functions for core logic (no X11 dependencies in logic functions)
- Minimal global state
- Simple data structures
- Clear separation between X11 interface and window management logic

### Data Structures
- **LogicalZone**: Represents a logical monitor zone with geometry
- **Client**: Represents a managed window
- **WindowManager**: Global state container

## Testing

The window manager includes assertions for testing core logic. To run with debug output:
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

## License

This project follows the suckless philosophy of simplicity and minimalism. Feel free to modify and distribute according to your needs.