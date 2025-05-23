#!/bin/bash

# launch_test_terminal.sh - Helper script to launch terminal in SWM test environment
# Usage: ./launch_test_terminal.sh [display_number]

DISPLAY_NUM=${1:-99}
TEST_DISPLAY=":$DISPLAY_NUM"

# Function to find available terminal emulator
find_terminal() {
    local terminals=("alacritty" "kitty" "wezterm" "gnome-terminal" "konsole" "xfce4-terminal" "mate-terminal" "lxterminal" "xterm" "urxvt" "st")
    
    for term in "${terminals[@]}"; do
        if command -v "$term" >/dev/null 2>&1; then
            echo "$term"
            return 0
        fi
    done
    
    echo ""
    return 1
}

# Check if test display is available
if ! xdpyinfo -display "$TEST_DISPLAY" >/dev/null 2>&1; then
    echo "Error: Test display $TEST_DISPLAY is not available"
    echo "Make sure you have a test environment running:"
    echo "  ./test_wm.sh -i"
    echo "  ./test_ultrawide.sh -i"
    exit 1
fi

# Find and launch terminal
TERMINAL=$(find_terminal)

if [ -z "$TERMINAL" ]; then
    echo "Error: No suitable terminal emulator found"
    echo "Please install one of: alacritty, kitty, gnome-terminal, xterm, etc."
    exit 1
fi

echo "Launching $TERMINAL on display $TEST_DISPLAY..."
echo "You can now test SWM commands like:"
echo "  ../swmctl cycle-window-next"
echo "  ../swmctl cycle-monitor-right"
echo "  ../swmctl kill-window"
echo ""
echo "Or from the project root:"
echo "  ./swmctl cycle-window-next"
echo "  ./swmctl cycle-monitor-right"
echo "  ./swmctl kill-window"

# Launch terminal with correct display
DISPLAY="$TEST_DISPLAY" "$TERMINAL" &

echo "Terminal launched (PID: $!)" 