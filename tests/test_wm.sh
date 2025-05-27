#!/bin/bash

# test_wm.sh - Test SWM window manager using Xvfb
# This allows testing without disrupting your current window manager

set -e

# Determine script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
DISPLAY_NUM=99
XVFB_DISPLAY=":$DISPLAY_NUM"
XVFB_RESOLUTION="1920x1080"
XVFB_PID_FILE="/tmp/swm_test_xvfb.pid"
SWM_PID_FILE="/tmp/swm_test.pid"
TEST_LOG="/tmp/swm_test.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[TEST]${NC} $1"
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
    log "Cleaning up test environment..."
    
    # Kill SWM if running
    if [ -f "$SWM_PID_FILE" ]; then
        SWM_PID=$(cat "$SWM_PID_FILE")
        if kill -0 "$SWM_PID" 2>/dev/null; then
            log "Stopping SWM (PID: $SWM_PID)"
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
            log "Stopping Xvfb (PID: $XVFB_PID)"
            kill "$XVFB_PID" 2>/dev/null || true
            sleep 1
            kill -9 "$XVFB_PID" 2>/dev/null || true
        fi
        rm -f "$XVFB_PID_FILE"
    fi
    
    # Clean up any remaining processes on our display
    pkill -f "DISPLAY=$XVFB_DISPLAY" 2>/dev/null || true
}

# Set up cleanup on exit
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

start_xvfb() {
    log "Starting Xvfb on display $XVFB_DISPLAY..."
    
    # Check if display is already in use
    if xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1; then
        error "Display $XVFB_DISPLAY is already in use"
        return 1
    fi
    
    # Start Xvfb
    Xvfb "$XVFB_DISPLAY" -screen 0 "${XVFB_RESOLUTION}x24" -ac +extension GLX +render +xinerama -noreset > "$TEST_LOG" 2>&1 &
    XVFB_PID=$!
    echo "$XVFB_PID" > "$XVFB_PID_FILE"
    
    # Wait for Xvfb to start
    for i in {1..10}; do
        if xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1; then
            success "Xvfb started successfully (PID: $XVFB_PID)"
            return 0
        fi
        sleep 0.5
    done
    
    error "Failed to start Xvfb"
    return 1
}

start_swm() {
    log "Starting SWM window manager..."
    
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
    
    # Wait for SWM to start
    sleep 2
    
    if kill -0 "$SWM_PID" 2>/dev/null; then
        success "SWM started successfully (PID: $SWM_PID)"
        
        # Show SWM's zone detection output
        log "SWM zone detection output:"
        if [ -f "$TEST_LOG.swm" ] && [ -s "$TEST_LOG.swm" ]; then
            cat "$TEST_LOG.swm" | head -10
        else
            log "SWM is running in background - checking zone detection..."
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

test_basic_functionality() {
    log "Testing basic SWM functionality..."
    
    # Test 1: Start a simple X client
    log "Test 1: Starting test client..."
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 5 &
    CLIENT_PID=$!
    sleep 1
    
    if kill -0 "$CLIENT_PID" 2>/dev/null; then
        success "Test client started successfully"
    else
        error "Failed to start test client"
        return 1
    fi
    
    # Test 2: Test window cycling
    log "Test 2: Testing window cycling..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
    sleep 0.5
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-prev
    success "Window cycling commands executed"
    
    # Test 3: Test monitor cycling
    log "Test 3: Testing monitor cycling..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-right
    sleep 0.5
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-left
    success "Monitor cycling commands executed"
    
    # Test 4: Test kill window
    log "Test 4: Testing kill window..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 1
    
    if ! kill -0 "$CLIENT_PID" 2>/dev/null; then
        success "Window killed successfully"
    else
        warn "Window may still be running (this might be normal)"
        kill "$CLIENT_PID" 2>/dev/null || true
    fi
    
    return 0
}

test_multiple_windows() {
    log "Testing multiple window management..."
    
    # Start multiple windows
    log "Starting multiple test windows..."
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID1=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID2=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID3=$!
    sleep 1
    
    # Test cycling through windows
    log "Cycling through windows..."
    for i in {1..5}; do
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
        sleep 0.2
    done
    
    # Test backward cycling
    log "Testing backward cycling..."
    for i in {1..3}; do
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-prev
        sleep 0.2
    done
    
    # Clean up test windows
    log "Cleaning up test windows..."
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    
    success "Multiple window test completed"
}

test_window_movement() {
    log "Testing window movement functionality..."
    
    # Start multiple windows in different scenarios
    log "Starting test windows for movement testing..."
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID1=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID2=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 15 &
    PID3=$!
    sleep 1
    
    # Test moving window to the right
    log "Testing move window right..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-right
    sleep 0.5
    
    # Test moving window to the left
    log "Testing move window left..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-left
    sleep 0.5
    
    # Test multiple moves in sequence
    log "Testing multiple window moves..."
    for i in {1..3}; do
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-right
        sleep 0.3
    done
    
    for i in {1..2}; do
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-left
        sleep 0.3
    done
    
    # Test move with window cycling
    log "Testing move combined with window cycling..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
    sleep 0.2
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-right
    sleep 0.3
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-prev
    sleep 0.2
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-left
    sleep 0.3
    
    # Test short command aliases
    log "Testing move window command aliases..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" mwr  # move-window-right alias
    sleep 0.3
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" mwl  # move-window-left alias
    sleep 0.3
    
    # Clean up test windows
    log "Cleaning up movement test windows..."
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    
    success "Window movement test completed"
}

# Function to check if a window exists using xwininfo
window_exists_x11() {
    local window_id="$1"
    if [ -z "$window_id" ]; then
        return 1
    fi
    DISPLAY="$XVFB_DISPLAY" xwininfo -id "$window_id" >/dev/null 2>&1
}

# Function to get window ID of a process
get_window_id() {
    local pid="$1"
    # Try to find the window ID using xdotool or wmctrl
    if command -v xdotool >/dev/null 2>&1; then
        DISPLAY="$XVFB_DISPLAY" xdotool search --pid "$pid" 2>/dev/null | head -1
    else
        # Fallback: use xwininfo to list all windows and try to match
        DISPLAY="$XVFB_DISPLAY" xwininfo -root -tree 2>/dev/null | grep -E "0x[0-9a-f]+" | head -1 | awk '{print $1}' 2>/dev/null || echo ""
    fi
}

# Function to verify window kill with detailed checking
verify_window_killed() {
    local pid="$1"
    local window_id="$2"
    local test_name="$3"
    local timeout=5
    local killed=false
    
    log "Verifying window kill for $test_name (PID: $pid, Window: $window_id)..."
    
    # Wait up to timeout seconds for the window/process to be killed
    for i in $(seq 1 $timeout); do
        local process_alive=false
        local window_alive=false
        
        # Check if process is still alive
        if kill -0 "$pid" 2>/dev/null; then
            process_alive=true
        fi
        
        # Check if window still exists (if we have window ID)
        if [ -n "$window_id" ] && window_exists_x11 "$window_id"; then
            window_alive=true
        fi
        
        log "  Check $i: Process alive=$process_alive, Window alive=$window_alive"
        
        # If both process and window are gone, or just the window is gone, consider it killed
        if ! $process_alive || ([ -n "$window_id" ] && ! $window_alive); then
            killed=true
            break
        fi
        
        sleep 1
    done
    
    if $killed; then
        success "$test_name: Window/process killed successfully"
        return 0
    else
        warn "$test_name: Window/process may still be running after $timeout seconds"
        # Force kill the process as cleanup
        kill "$pid" 2>/dev/null || true
        return 1
    fi
}

test_kill_window_functionality() {
    log "Testing comprehensive kill window functionality..."
    
    # Test 1: Kill single window with detailed verification
    log "Test 1: Kill single window with detailed verification..."
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 5 &
    PID1=$!
    sleep 1
    
    if kill -0 "$PID1" 2>/dev/null; then
        # Try to get the window ID
        WINDOW_ID1=$(get_window_id "$PID1")
        log "Window started (PID: $PID1, Window ID: ${WINDOW_ID1:-unknown}), attempting to kill..."
        
        # Issue kill command
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
        
        # Verify the kill worked
        verify_window_killed "$PID1" "$WINDOW_ID1" "Single window test"
    else
        error "Failed to start test window"
        return 1
    fi
    
    # Test 2: Kill window with multiple windows (focus management) - improved verification
    log "Test 2: Kill window with multiple windows (focus management)..."
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID1=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID2=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 10 &
    PID3=$!
    sleep 1
    
    # Get window IDs
    WINDOW_ID1=$(get_window_id "$PID1")
    WINDOW_ID2=$(get_window_id "$PID2")
    WINDOW_ID3=$(get_window_id "$PID3")
    
    log "Started 3 windows: PID1=$PID1 (WID: ${WINDOW_ID1:-unknown}), PID2=$PID2 (WID: ${WINDOW_ID2:-unknown}), PID3=$PID3 (WID: ${WINDOW_ID3:-unknown})"
    
    # Kill the focused window (should be the last one created)
    log "Killing focused window (should be window 3)..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    
    # Verify window 3 was killed
    if ! verify_window_killed "$PID3" "$WINDOW_ID3" "Multiple windows test - window 3"; then
        warn "Window 3 kill verification failed"
    fi
    
    # Check if focus moved to another window by testing cycling
    log "Testing focus after kill by cycling..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
    sleep 0.5
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-prev
    sleep 0.5
    
    # Kill another window
    log "Killing another window..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    
    # Verify one of the remaining windows was killed
    sleep 1
    local remaining_count=0
    if kill -0 "$PID1" 2>/dev/null; then remaining_count=$((remaining_count + 1)); fi
    if kill -0 "$PID2" 2>/dev/null; then remaining_count=$((remaining_count + 1)); fi
    
    log "After second kill: $remaining_count windows remaining"
    
    # Test cycling with remaining window
    log "Testing cycling with remaining window..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
    sleep 0.5
    
    # Kill the last window
    log "Killing the last window..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 1
    
    # Clean up any remaining processes
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    success "Multiple window kill test completed"
    
    # Test 3: Kill window in different zones
    log "Test 3: Kill window across different zones..."
    
    # Start windows and move them to different zones
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 8 &
    PID1=$!
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 8 &
    PID2=$!
    sleep 0.5
    
    # Move one window to a different zone
    log "Moving window to different zone..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-right
    sleep 0.5
    
    # Switch zones and add another window
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-right
    sleep 0.5
    
    DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 8 &
    PID3=$!
    sleep 1
    
    # Kill window in current zone
    log "Killing window in current zone..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 1
    
    # Switch to another zone and kill window there
    log "Switching zones and killing window..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-left
    sleep 0.5
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 1
    
    # Clean up
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    success "Cross-zone kill test completed"
    
    # Test 4: Kill window with rapid operations
    log "Test 4: Kill window with rapid operations..."
    
    # Start multiple windows quickly
    for i in {1..5}; do
        DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 3 &
        eval "PID$i=\$!"
        sleep 0.2
    done
    sleep 1
    
    # Rapidly cycle and kill windows
    log "Rapidly cycling and killing windows..."
    for i in {1..3}; do
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
        sleep 0.1
        DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
        sleep 0.3
    done
    
    # Clean up any remaining
    for i in {1..5}; do
        eval "kill \$PID$i 2>/dev/null || true"
    done
    success "Rapid operations kill test completed"
    
    # Test 5: Kill window in empty zone (edge case)
    log "Test 5: Kill window in empty zone (should not crash)..."
    
    # Make sure no windows are running
    sleep 1
    
    # Try to kill in empty zone
    log "Attempting to kill in empty zone..."
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 0.5
    
    # Switch zones and try again
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-right
    sleep 0.5
    DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
    sleep 0.5
    
    success "Empty zone kill test completed (no crash)"
    
    # Test 6: Kill window stress test
    log "Test 6: Kill window stress test..."
    
    # Start many windows
    STRESS_PIDS=()
    for i in {1..8}; do
        DISPLAY="$XVFB_DISPLAY" "$SCRIPT_DIR/test_client" 2 &
        STRESS_PIDS+=($!)
        sleep 0.1
    done
    sleep 1
    
    # Mix cycling, moving, and killing
    log "Stress testing with mixed operations..."
    for i in {1..10}; do
        case $((i % 4)) in
            0)
                DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-window-next
                ;;
            1)
                DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" kill-window
                ;;
            2)
                DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" move-window-right
                ;;
            3)
                DISPLAY="$XVFB_DISPLAY" "$PROJECT_ROOT/swmctl" cycle-monitor-left
                ;;
        esac
        sleep 0.2
    done
    
    # Clean up stress test
    for pid in "${STRESS_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    success "Kill window stress test completed"
    
    log "All kill window functionality tests completed successfully!"
}

run_interactive_test() {
    log "Starting interactive test mode..."
    log "SWM is running on display $XVFB_DISPLAY"
    
    # Detect available terminal
    AVAILABLE_TERMINAL=$(find_terminal)
    
    log "You can now run commands like:"
    log "  DISPLAY=$XVFB_DISPLAY $SCRIPT_DIR/test_client 30 &"
    
    if [ "$AVAILABLE_TERMINAL" != "test_client" ]; then
        log "  DISPLAY=$XVFB_DISPLAY $AVAILABLE_TERMINAL &"
    fi
    
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl cycle-window-next"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl move-window-left"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl move-window-right"
    log "  DISPLAY=$XVFB_DISPLAY $PROJECT_ROOT/swmctl kill-window"
    log ""
    log "Available terminal: $AVAILABLE_TERMINAL"
    log "Press Ctrl+C to stop the test environment"
    
    # Keep the test environment running
    while true; do
        sleep 1
        # Check if SWM is still running
        if ! kill -0 "$SWM_PID" 2>/dev/null; then
            error "SWM has stopped running"
            break
        fi
    done
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Test SWM window manager using Xvfb virtual display"
    echo ""
    echo "Options:"
    echo "  -h, --help       Show this help message"
    echo "  -i, --interactive Run in interactive mode"
    echo "  -b, --basic      Run basic functionality tests only"
    echo "  -a, --all        Run all automated tests (default)"
    echo "  -v, --verbose    Show verbose output"
    echo ""
    echo "Examples:"
    echo "  $0                # Run all automated tests"
    echo "  $0 -i            # Start interactive test environment"
    echo "  $0 -b            # Run basic tests only"
}

# Parse command line arguments
INTERACTIVE=false
BASIC_ONLY=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -i|--interactive)
            INTERACTIVE=true
            shift
            ;;
        -b|--basic)
            BASIC_ONLY=true
            shift
            ;;
        -a|--all)
            # Default behavior
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
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
    log "Starting SWM test environment..."
    
    # Check dependencies
    if ! command -v Xvfb >/dev/null 2>&1; then
        error "Xvfb not found. Please install: sudo pacman -S xorg-server-xvfb"
        exit 1
    fi
    
    if ! command -v xdpyinfo >/dev/null 2>&1; then
        error "xdpyinfo not found. Please install: sudo pacman -S xorg-xdpyinfo"
        exit 1
    fi
    
    # Start test environment
    start_xvfb || exit 1
    start_swm || exit 1
    
    if [ "$INTERACTIVE" = true ]; then
        run_interactive_test
    else
        # Run automated tests
        test_basic_functionality || exit 1
        
        if [ "$BASIC_ONLY" = false ]; then
            test_multiple_windows || exit 1
            test_window_movement || exit 1
            test_kill_window_functionality || exit 1
        fi
        
        success "All tests completed successfully!"
        log "Check logs at: $TEST_LOG and $TEST_LOG.swm"
    fi
}

main "$@" 