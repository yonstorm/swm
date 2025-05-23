#!/bin/bash

# test_ultrawide.sh - Test SWM ultrawide monitor functionality
# Simulates ultrawide monitors using Xvfb with custom resolutions

set -e

# Determine script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
DISPLAY_NUM=98
XVFB_DISPLAY=":$DISPLAY_NUM"
ULTRAWIDE_RESOLUTION="5120x1440"  # Ultrawide resolution
REGULAR_RESOLUTION="1920x1080"    # Regular resolution
XVFB_PID_FILE="/tmp/swm_ultrawide_xvfb.pid"
SWM_PID_FILE="/tmp/swm_ultrawide.pid"
TEST_LOG="/tmp/swm_ultrawide_test.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[ULTRAWIDE-TEST]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

cleanup() {
    log "Cleaning up ultrawide test environment..."
    
    # Kill SWM if running
    if [ -f "$SWM_PID_FILE" ]; then
        SWM_PID=$(cat "$SWM_PID_FILE")
        if kill -0 "$SWM_PID" 2>/dev/null; then
            kill "$SWM_PID" 2>/dev/null || true
            sleep 1
            kill -9 "$SWM_PID" 2>/dev/null || true
        fi
        rm -f "$SWM_PID_FILE"
    fi
    
    # Kill Xvfb if running
    if [ -f "$XVFB_PID_FILE" ]; then
        XVFB_PID=$(cat "$XVFB_PID_FILE")
        if kill -0 "$XVFB_PID" 2>/dev/null; then
            kill "$XVFB_PID" 2>/dev/null || true
            sleep 1
            kill -9 "$XVFB_PID" 2>/dev/null || true
        fi
        rm -f "$XVFB_PID_FILE"
    fi
    
    pkill -f "DISPLAY=$XVFB_DISPLAY" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

# Function to find available terminal emulator
find_terminal() {
    # List of terminal emulators to try, in order of preference
    local terminals=("alacritty" "kitty" "wezterm" "gnome-terminal" "konsole" "xfce4-terminal" "mate-terminal" "lxterminal" "xterm" "urxvt" "st")
    
    for term in "${terminals[@]}"; do
        if command -v "$term" >/dev/null 2>&1; then
            echo "$term"
            return 0
        fi
    done
    
    # Fallback: try to use our test client
    echo "test_client"
    return 0
}

start_ultrawide_xvfb() {
    local resolution="$1"
    log "Starting Xvfb with $resolution resolution..."
    
    # Check if display is already in use
    if xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1; then
        error "Display $XVFB_DISPLAY is already in use"
        return 1
    fi
    
    # Start Xvfb with custom resolution
    Xvfb "$XVFB_DISPLAY" -screen 0 "${resolution}x24" -ac +extension GLX +render +xinerama -noreset > "$TEST_LOG" 2>&1 &
    XVFB_PID=$!
    echo "$XVFB_PID" > "$XVFB_PID_FILE"
    
    # Wait for Xvfb to start
    for i in {1..10}; do
        if xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1; then
            success "Xvfb started with $resolution (PID: $XVFB_PID)"
            return 0
        fi
        sleep 0.5
    done
    
    error "Failed to start Xvfb"
    return 1
}

start_swm_ultrawide() {
    log "Starting SWM for ultrawide testing..."
    
    # Make sure SWM is built
    if [ ! -x "$PROJECT_ROOT/swm" ]; then
        log "Building SWM..."
        cd "$PROJECT_ROOT" && make clean && make && cd "$SCRIPT_DIR"
    fi
    
    # Make sure test client is built
    if [ ! -x "$SCRIPT_DIR/test_client" ]; then
        log "Building test client..."
        cd "$PROJECT_ROOT" && make tests/test_client && cd "$SCRIPT_DIR"
    fi
    
    # Start SWM and capture output
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swm" > "$TEST_LOG.swm" 2>&1 &
    SWM_PID=$!
    echo "$SWM_PID" > "$SWM_PID_FILE"
    
    # Wait for SWM to start and capture its output
    sleep 3
    
    if kill -0 "$SWM_PID" 2>/dev/null; then
        success "SWM started successfully (PID: $SWM_PID)"
        
        # Show SWM's zone detection output
        log "SWM zone detection output:"
        if [ -f "$TEST_LOG.swm" ] && [ -s "$TEST_LOG.swm" ]; then
            cat "$TEST_LOG.swm" | head -10
        else
            log "SWM is running in background - attempting to get zone info..."
            # Try to get output directly (this will fail with BadAccess since SWM is already running)
            # but we can capture the zone detection output before the error
            ZONE_OUTPUT=$(timeout 2 sh -c "DISPLAY='$XVFB_DISPLAY' '$PROJECT_ROOT/swm' 2>&1" | grep -E "(Detected|Zone)" | head -5 2>/dev/null || true)
            
            if [ -n "$ZONE_OUTPUT" ]; then
                log "Zone detection successful:"
                echo "$ZONE_OUTPUT"
            else
                log "Zone detection output not captured in logs (SWM is running normally)"
                log "This is expected when SWM runs as a background process"
            fi
        fi
        
        return 0
    else
        error "Failed to start SWM"
        if [ -f "$TEST_LOG.swm" ]; then
            cat "$TEST_LOG.swm"
        fi
        return 1
    fi
}

test_ultrawide_zones() {
    log "Testing ultrawide zone functionality..."
    
    # Start multiple windows to test zone placement
    log "Starting test windows in different zones..."
    
    # Window 1 - should go to active zone
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID1=$!
    sleep 1
    
    # Cycle to next zone and add another window
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-right
    sleep 0.5
    
    # Window 2 - should go to next zone
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID2=$!
    sleep 1
    
    # Cycle to next zone and add another window
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-right
    sleep 0.5
    
    # Window 3 - should go to third zone
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID3=$!
    sleep 1
    
    # Test cycling through zones
    log "Testing zone cycling with multiple windows..."
    for i in {1..6}; do
        log "Cycling to zone (iteration $i)..."
        DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-right
        sleep 0.5
    done
    
    # Test backward zone cycling
    log "Testing backward zone cycling..."
    for i in {1..3}; do
        log "Cycling backward (iteration $i)..."
        DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-left
        sleep 0.5
    done
    
    # Test window cycling within zones
    log "Testing window cycling within current zone..."
    for i in {1..4}; do
        DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-window-next
        sleep 0.3
    done
    
    # Test window movement between zones
    log "Testing window movement between ultrawide zones..."
    
    # Move focused window to the right through all zones
    log "Moving window right through all zones..."
    for i in {1..4}; do
        log "Moving window right (iteration $i)..."
        DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl move-window-right
        sleep 0.5
    done
    
    # Move focused window to the left through all zones
    log "Moving window left through all zones..."
    for i in {1..3}; do
        log "Moving window left (iteration $i)..."
        DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl move-window-left
        sleep 0.5
    done
    
    # Test move with different focused windows
    log "Testing window movement with different focused windows..."
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-window-next
    sleep 0.3
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl move-window-right
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-window-prev
    sleep 0.3
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl move-window-left
    sleep 0.5
    
    # Test command aliases
    log "Testing window movement command aliases..."
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl mwr  # move-window-right alias
    sleep 0.3
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl mwl  # move-window-left alias
    sleep 0.3
    
    # Clean up test windows
    log "Cleaning up test windows..."
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    
    success "Ultrawide zone testing completed"
}

test_regular_vs_ultrawide() {
    log "Testing regular monitor (should create 1 zone)..."
    
    # Stop current test and restart with regular resolution
    cleanup
    sleep 2
    
    start_ultrawide_xvfb "$REGULAR_RESOLUTION"
    start_swm_ultrawide
    
    log "Testing regular monitor zone behavior..."
    
    # Start a window
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 5 &
    CLIENT_PID=$!
    sleep 1
    
    # Try zone cycling (should not do much with only 1 zone)
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-right
    DISPLAY="$XVFB_DISPLAY" $PROJECT_ROOT/swmctl cycle-monitor-left
    
    # Clean up
    kill "$CLIENT_PID" 2>/dev/null || true
    
    success "Regular monitor test completed"
    
    # Restart with ultrawide for comparison
    cleanup
    sleep 2
    
    log "Restarting with ultrawide resolution for comparison..."
    start_ultrawide_xvfb "$ULTRAWIDE_RESOLUTION"
    start_swm_ultrawide
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Test SWM ultrawide monitor functionality using Xvfb"
    echo ""
    echo "Options:"
    echo "  -h, --help       Show this help message"
    echo "  -u, --ultrawide  Test ultrawide functionality (default)"
    echo "  -r, --regular    Test regular monitor functionality"
    echo "  -c, --compare    Test both regular and ultrawide"
    echo "  -i, --interactive Run in interactive mode"
    echo ""
    echo "Examples:"
    echo "  $0                # Test ultrawide functionality"
    echo "  $0 -c            # Compare regular vs ultrawide"
    echo "  $0 -i            # Interactive ultrawide testing"
}

run_interactive_ultrawide() {
    log "Starting interactive ultrawide test mode..."
    log "SWM is running on display $XVFB_DISPLAY with ultrawide resolution"
    log "The monitor should be split into 3 zones (1/4 - 1/2 - 1/4)"
    log ""
    
    # Detect available terminal
    AVAILABLE_TERMINAL=$(find_terminal)
    
    log "Try these commands:"
    log "  DISPLAY=$XVFB_DISPLAY $SCRIPT_DIR/test_client 30 &"
    
    if [ "$AVAILABLE_TERMINAL" != "test_client" ]; then
        log "  DISPLAY=$XVFB_DISPLAY $AVAILABLE_TERMINAL &"
    fi
    
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl cycle-monitor-right"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl cycle-monitor-left"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl cycle-window-next"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl move-window-left"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl move-window-right"
    log ""
    log "Available terminal: $AVAILABLE_TERMINAL"
    log "Press Ctrl+C to stop the test environment"
    
    while true; do
        sleep 1
        if ! kill -0 "$SWM_PID" 2>/dev/null; then
            error "SWM has stopped running"
            break
        fi
    done
}

# Parse arguments
INTERACTIVE=false
TEST_MODE="ultrawide"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -u|--ultrawide)
            TEST_MODE="ultrawide"
            shift
            ;;
        -r|--regular)
            TEST_MODE="regular"
            shift
            ;;
        -c|--compare)
            TEST_MODE="compare"
            shift
            ;;
        -i|--interactive)
            INTERACTIVE=true
            shift
            ;;
        *)
            error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Main execution
main() {
    log "Starting SWM ultrawide test environment..."
    
    # Check dependencies
    if ! command -v Xvfb >/dev/null 2>&1; then
        error "Xvfb not found. Please install: sudo pacman -S xorg-server-xvfb"
        exit 1
    fi
    
    case "$TEST_MODE" in
        "ultrawide")
            start_ultrawide_xvfb "$ULTRAWIDE_RESOLUTION"
            start_swm_ultrawide
            
            if [ "$INTERACTIVE" = true ]; then
                run_interactive_ultrawide
            else
                test_ultrawide_zones
            fi
            ;;
        "regular")
            start_ultrawide_xvfb "$REGULAR_RESOLUTION"
            start_swm_ultrawide
            
            if [ "$INTERACTIVE" = true ]; then
                run_interactive_ultrawide
            else
                log "Regular monitor testing completed"
            fi
            ;;
        "compare")
            test_regular_vs_ultrawide
            if [ "$INTERACTIVE" = false ]; then
                test_ultrawide_zones
            fi
            ;;
    esac
    
    if [ "$INTERACTIVE" = false ]; then
        success "Ultrawide testing completed successfully!"
        log "Check logs at: $TEST_LOG and $TEST_LOG.swm"
    fi
}

main "$@" 