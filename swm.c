#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include "config.h"
#include "core.h"

/* Global state */
static WindowManager wm = {0};
static bool ignore_bad_window = false;

/* X11 error handler */
int x11_error_handler(Display *display, XErrorEvent *error) {
    /* Ignore BadWindow errors when we're checking if a window exists */
    if (ignore_bad_window && error->error_code == BadWindow) {
        return 0;
    }
    
    /* For other errors, print them */
    char error_text[256];
    XGetErrorText(display, error->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "X11 Error: %s (request code: %d, resource: 0x%lx)\n", 
            error_text, error->request_code, error->resourceid);
    
    return 0;
}

/* Helper function to safely check if a window exists */
bool window_exists(Display *display, Window window) {
    XWindowAttributes attrs;
    ignore_bad_window = true;
    int result = XGetWindowAttributes(display, window, &attrs);
    ignore_bad_window = false;
    return result != 0;
}

/* X11 helper functions */

void set_window_border(Display *display, Window window, unsigned long color) {
    /* Check if window exists before setting border */
    if (window_exists(display, window)) {
        XSetWindowBorder(display, window, color);
        XSetWindowBorderWidth(display, window, BORDER_WIDTH);
    }
}

void focus_window(Display *display, Window window) {
    /* Check if window exists before focusing */
    if (window_exists(display, window)) {
        XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(display, window);
    }
}

void resize_window_to_zone(Display *display, Window window, LogicalZone *zone) {
    /* Check if window exists before resizing */
    if (window_exists(display, window)) {
        XMoveResizeWindow(display, window, 
                          zone->geometry.x, zone->geometry.y,
                          zone->geometry.width - 2 * BORDER_WIDTH,
                          zone->geometry.height - 2 * BORDER_WIDTH);
    }
}

/* Client management using new simplified approach */

Client *create_client(Window window, int zone_index) {
    Client *client = malloc(sizeof(Client));
    assert(client != NULL);
    
    client->window = window;
    client->zone_index = zone_index;
    client->next = NULL;
    
    return client;
}

Client *find_client_by_window(DisplayManager *display, Window window) {
    for (int zone = 0; zone < display->zone_count; zone++) {
        Client *current = display->zone_clients[zone];
        while (current) {
            if (current->window == window) {
                return current;
            }
            current = current->next;
        }
    }
    return NULL;
}

/* Window manager operations using new approach */

void cycle_window_focus(void) {
    DisplayManager *display = wm.active_display;
    if (!display || display->zone_count == 0) return;
    
    int zone = display->active_zone;
    int count = count_clients_in_zone(display, zone);
    if (count <= 1) return;
    
    /* Unfocus current window */
    Client *current = get_current_client_in_zone(display, zone);
    if (current) {
        set_window_border(display->x_display, current->window, UNFOCUS_COLOR);
    }
    
    /* Move to next client */
    display->zone_current_index[zone] = (display->zone_current_index[zone] + 1) % count;
    
    /* Focus new current window */
    Client *next = get_current_client_in_zone(display, zone);
    if (next) {
        set_window_border(display->x_display, next->window, FOCUS_COLOR);
        focus_window(display->x_display, next->window);
    }
}

void cycle_window_focus_direction(int direction) {
    DisplayManager *display = wm.active_display;
    if (!display || display->zone_count == 0) return;
    
    int zone = display->active_zone;
    int count = count_clients_in_zone(display, zone);
    if (count <= 1) return;
    
    /* Unfocus current window */
    Client *current = get_current_client_in_zone(display, zone);
    if (current) {
        set_window_border(display->x_display, current->window, UNFOCUS_COLOR);
    }
    
    /* Move in specified direction */
    int current_index = display->zone_current_index[zone];
    if (direction > 0) {
        display->zone_current_index[zone] = (current_index + 1) % count;
    } else {
        display->zone_current_index[zone] = (current_index - 1 + count) % count;
    }
    
    /* Focus new current window */
    Client *next = get_current_client_in_zone(display, zone);
    if (next) {
        set_window_border(display->x_display, next->window, FOCUS_COLOR);
        focus_window(display->x_display, next->window);
    }
}

void cycle_monitor_focus(void) {
    DisplayManager *display = wm.active_display;
    if (!display || display->zone_count <= 1) return;
    
    /* Unfocus current window */
    Client *current = get_current_client_in_zone(display, display->active_zone);
    if (current) {
        set_window_border(display->x_display, current->window, UNFOCUS_COLOR);
    }
    
    /* Move to next zone */
    display->active_zone = (display->active_zone + 1) % display->zone_count;
    
    /* Focus window in new zone */
    Client *next = get_current_client_in_zone(display, display->active_zone);
    if (next) {
        set_window_border(display->x_display, next->window, FOCUS_COLOR);
        focus_window(display->x_display, next->window);
    }
}

void cycle_monitor_focus_direction(int direction) {
    DisplayManager *display = wm.active_display;
    if (!display || display->zone_count <= 1) return;
    
    /* Unfocus current window */
    Client *current = get_current_client_in_zone(display, display->active_zone);
    if (current) {
        set_window_border(display->x_display, current->window, UNFOCUS_COLOR);
    }
    
    /* Move to zone in specified direction */
    int zone_count = display->zone_count;
    if (direction > 0) {
        display->active_zone = (display->active_zone + 1) % zone_count;
    } else {
        display->active_zone = (display->active_zone - 1 + zone_count) % zone_count;
    }
    
    /* Focus window in new zone */
    Client *next = get_current_client_in_zone(display, display->active_zone);
    if (next) {
        set_window_border(display->x_display, next->window, FOCUS_COLOR);
        focus_window(display->x_display, next->window);
    }
}

void kill_focused_window(void) {
    printf("[DEBUG] kill_focused_window() called\n");
    fflush(stdout);
    
    DisplayManager *display = wm.active_display;
    if (!display) {
        printf("[DEBUG] kill_focused_window: No active display, returning early\n");
        fflush(stdout);
        return;
    }
    printf("[DEBUG] kill_focused_window: Active display found\n");
    fflush(stdout);
    
    printf("[DEBUG] kill_focused_window: Active zone = %d, Zone count = %d\n", 
           display->active_zone, display->zone_count);
    fflush(stdout);
    
    Client *current = get_current_client_in_zone(display, display->active_zone);
    if (!current) {
        printf("[DEBUG] kill_focused_window: No current client in zone %d, returning early\n", 
               display->active_zone);
        fflush(stdout);
        
        /* Debug: Check if there are any clients in this zone */
        int client_count = count_clients_in_zone(display, display->active_zone);
        printf("[DEBUG] kill_focused_window: Client count in zone %d = %d\n", 
               display->active_zone, client_count);
        fflush(stdout);
        
        /* Debug: Check current index */
        printf("[DEBUG] kill_focused_window: Current index in zone %d = %d\n", 
               display->active_zone, display->zone_current_index[display->active_zone]);
        fflush(stdout);
        
        return;
    }
    
    Window window = current->window;
    printf("[DEBUG] kill_focused_window: Found current client with window 0x%lx in zone %d\n", 
           window, display->active_zone);
    fflush(stdout);
    
    /* Check if window still exists */
    if (!window_exists(display->x_display, window)) {
        printf("[DEBUG] kill_focused_window: Window 0x%lx no longer exists, cleaning up\n", window);
        fflush(stdout);
        remove_client_from_zone(display, display->active_zone, current);
        free(current);
        return;
    }
    printf("[DEBUG] kill_focused_window: Window 0x%lx exists, proceeding with kill\n", window);
    fflush(stdout);
    
    /* Try to close window gracefully first (WM_DELETE_WINDOW) */
    Atom wm_delete_window = XInternAtom(display->x_display, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(display->x_display, "WM_PROTOCOLS", False);
    printf("[DEBUG] kill_focused_window: Got atoms - WM_DELETE_WINDOW=%ld, WM_PROTOCOLS=%ld\n", 
           wm_delete_window, wm_protocols);
    fflush(stdout);
    
    Atom *protocols;
    int n_protocols;
    bool supports_delete = false;
    
    if (XGetWMProtocols(display->x_display, window, &protocols, &n_protocols)) {
        printf("[DEBUG] kill_focused_window: Window supports %d protocols\n", n_protocols);
        fflush(stdout);
        for (int i = 0; i < n_protocols; i++) {
            printf("[DEBUG] kill_focused_window: Protocol %d: %ld\n", i, protocols[i]);
            fflush(stdout);
            if (protocols[i] == wm_delete_window) {
                supports_delete = true;
                printf("[DEBUG] kill_focused_window: Window supports WM_DELETE_WINDOW\n");
                fflush(stdout);
                break;
            }
        }
        XFree(protocols);
    } else {
        printf("[DEBUG] kill_focused_window: XGetWMProtocols failed for window 0x%lx\n", window);
        fflush(stdout);
    }
    
    if (supports_delete) {
        printf("[DEBUG] kill_focused_window: Sending WM_DELETE_WINDOW message to window 0x%lx\n", window);
        fflush(stdout);
        /* Send WM_DELETE_WINDOW message */
        XEvent event;
        event.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = wm_protocols;
        event.xclient.format = 32;
        event.xclient.data.l[0] = wm_delete_window;
        event.xclient.data.l[1] = CurrentTime;
        
        int result = XSendEvent(display->x_display, window, False, NoEventMask, &event);
        printf("[DEBUG] kill_focused_window: XSendEvent result = %d\n", result);
        fflush(stdout);
    } else {
        printf("[DEBUG] kill_focused_window: Force killing window 0x%lx with XKillClient\n", window);
        fflush(stdout);
        /* Force kill the window */
        XKillClient(display->x_display, window);
    }
    
    XFlush(display->x_display);
    printf("[DEBUG] kill_focused_window: XFlush completed, kill command sent\n");
    fflush(stdout);
}

void move_focused_window_to_zone_direction(int direction) {
    DisplayManager *display = wm.active_display;
    if (!display || display->zone_count <= 1) return;
    
    Client *current = get_current_client_in_zone(display, display->active_zone);
    if (!current) return;
    
    /* Calculate target zone */
    int zone_count = display->zone_count;
    int target_zone;
    if (direction > 0) {
        target_zone = (display->active_zone + 1) % zone_count;
    } else {
        target_zone = (display->active_zone - 1 + zone_count) % zone_count;
    }
    
    /* Remove from current zone */
    remove_client_from_zone(display, display->active_zone, current);
    
    /* Add to target zone */
    add_client_to_zone(display, target_zone, current);
    
    /* Resize and move window to new zone */
    resize_window_to_zone(display->x_display, current->window, &display->zones[target_zone]);
    
    /* Update active zone to follow the window */
    display->active_zone = target_zone;
    
    XFlush(display->x_display);
}

/* Event handlers */

void handle_map_request(XMapRequestEvent *event) {
    DisplayManager *display = wm.active_display;
    if (!display) return;
    
    Window window = event->window;
    
    /* Create and add client to active zone */
    Client *client = create_client(window, display->active_zone);
    add_client_to_zone(display, display->active_zone, client);
    
    /* Resize window to fit zone */
    if (display->active_zone < display->zone_count) {
        resize_window_to_zone(display->x_display, window, &display->zones[display->active_zone]);
    }
    
    /* Set border and map window */
    set_window_border(display->x_display, window, UNFOCUS_COLOR);
    XMapWindow(display->x_display, window);
    
    /* Unfocus previous window and focus the new one */
    Client *previous = get_current_client_in_zone(display, display->active_zone);
    if (previous && previous != client) {
        set_window_border(display->x_display, previous->window, UNFOCUS_COLOR);
    }
    
    /* Make the new client current (it's already at index 0 since we add to front) */
    display->zone_current_index[display->active_zone] = 0;
    set_window_border(display->x_display, window, FOCUS_COLOR);
    focus_window(display->x_display, window);
}

void handle_unmap_notify(XUnmapEvent *event) {
    DisplayManager *display = wm.active_display;
    if (!display) return;
    
    Client *client = find_client_by_window(display, event->window);
    if (!client) return;
    
    int zone = client->zone_index;
    
    /* Remove client from zone */
    remove_client_from_zone(display, zone, client);
    free(client);
    
    /* Focus the new current window in the zone if any */
    Client *next = get_current_client_in_zone(display, zone);
    if (next) {
        /* Check if the window still exists before trying to manipulate it */
        if (!window_exists(display->x_display, next->window)) {
            /* Window doesn't exist anymore, remove this client too */
            remove_client_from_zone(display, zone, next);
            free(next);
            /* Try to find another window to focus */
            next = get_current_client_in_zone(display, zone);
        }
        
        if (next) {
            /* Verify window still exists before setting border and focus */
            if (window_exists(display->x_display, next->window)) {
                set_window_border(display->x_display, next->window, FOCUS_COLOR);
                focus_window(display->x_display, next->window);
            }
        }
    }
}

void handle_property_notify(XPropertyEvent *event) {
    DisplayManager *display = wm.active_display;
    if (!display || event->atom != display->command_atom || event->window != display->root) {
        return;
    }
    
    printf("[DEBUG] handle_property_notify: Command property changed\n");
    fflush(stdout);
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    
    if (XGetWindowProperty(display->x_display, display->root, display->command_atom,
                          0, 1, True, XA_INTEGER,
                          &actual_type, &actual_format,
                          &nitems, &bytes_after, &data) == Success) {
        
        if (data != NULL && nitems > 0) {
            int command = *(int*)data;
            printf("[DEBUG] handle_property_notify: Received command %d\n", command);
            fflush(stdout);
            
            switch (command) {
                case CMD_CYCLE_WINDOW:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_WINDOW\n");
                    fflush(stdout);
                    cycle_window_focus();
                    break;
                case CMD_CYCLE_WINDOW_NEXT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_WINDOW_NEXT\n");
                    fflush(stdout);
                    cycle_window_focus_direction(1);
                    break;
                case CMD_CYCLE_WINDOW_PREV:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_WINDOW_PREV\n");
                    fflush(stdout);
                    cycle_window_focus_direction(-1);
                    break;
                case CMD_CYCLE_MONITOR:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_MONITOR\n");
                    fflush(stdout);
                    cycle_monitor_focus();
                    break;
                case CMD_CYCLE_MONITOR_LEFT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_MONITOR_LEFT\n");
                    fflush(stdout);
                    cycle_monitor_focus_direction(-1);
                    break;
                case CMD_CYCLE_MONITOR_RIGHT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_CYCLE_MONITOR_RIGHT\n");
                    fflush(stdout);
                    cycle_monitor_focus_direction(1);
                    break;
                case CMD_KILL_WINDOW:
                    printf("[DEBUG] handle_property_notify: Executing CMD_KILL_WINDOW\n");
                    fflush(stdout);
                    kill_focused_window();
                    break;
                case CMD_MOVE_WINDOW_LEFT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_MOVE_WINDOW_LEFT\n");
                    fflush(stdout);
                    move_focused_window_to_zone_direction(-1);
                    break;
                case CMD_MOVE_WINDOW_RIGHT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_MOVE_WINDOW_RIGHT\n");
                    fflush(stdout);
                    move_focused_window_to_zone_direction(1);
                    break;
                case CMD_QUIT:
                    printf("[DEBUG] handle_property_notify: Executing CMD_QUIT\n");
                    fflush(stdout);
                    exit(0);
                    break;
                default:
                    printf("[DEBUG] handle_property_notify: Unknown command %d\n", command);
                    fflush(stdout);
                    break;
            }
            
            XFree(data);
        } else {
            printf("[DEBUG] handle_property_notify: No data or empty data received\n");
            fflush(stdout);
        }
    } else {
        printf("[DEBUG] handle_property_notify: XGetWindowProperty failed\n");
        fflush(stdout);
    }
}

/* Initialization */

DisplayManager *create_display_manager(Display *x_display, int screen, Window root) {
    DisplayManager *display = malloc(sizeof(DisplayManager));
    assert(display != NULL);
    
    display->x_display = x_display;
    display->screen = screen;
    display->root = root;
    display->zones = NULL;
    display->zone_count = 0;
    display->active_zone = 0;
    display->zone_clients = NULL;
    display->zone_current_index = NULL;
    display->command_atom = XInternAtom(x_display, COMMAND_PROPERTY, False);
    display->next = NULL;
    
    return display;
}

int setup_display_zones(DisplayManager *display) {
    int monitor_count;
    XineramaScreenInfo *monitors = XineramaQueryScreens(display->x_display, &monitor_count);
    
    if (monitors == NULL || monitor_count == 0) {
        fprintf(stderr, "No monitors detected\n");
        return 0;
    }
    
    display->zone_count = calculate_zones(monitors, monitor_count, &display->zones);
    
    /* Allocate zone-based client management arrays */
    display->zone_clients = calloc(display->zone_count, sizeof(Client*));
    display->zone_current_index = malloc(display->zone_count * sizeof(int));
    assert(display->zone_clients != NULL && display->zone_current_index != NULL);
    
    /* Initialize all zones to have no clients */
    for (int i = 0; i < display->zone_count; i++) {
        display->zone_clients[i] = NULL;
        display->zone_current_index[i] = -1;
    }
    
    printf("Detected %d monitors, created %d logical zones:\n", monitor_count, display->zone_count);
    for (int i = 0; i < display->zone_count; i++) {
        printf("Zone %d: %dx%d+%d+%d (monitor %d, zone %d)\n", i,
               display->zones[i].geometry.width, display->zones[i].geometry.height,
               display->zones[i].geometry.x, display->zones[i].geometry.y,
               display->zones[i].monitor_id, display->zones[i].zone_id);
    }
    
    XFree(monitors);
    return 1;
}

int main(void) {
    /* Set up X11 error handler */
    XSetErrorHandler(x11_error_handler);
    
    /* Open display */
    Display *x_display = XOpenDisplay(NULL);
    if (x_display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    int screen = DefaultScreen(x_display);
    Window root = RootWindow(x_display, screen);
    
    /* Check for Xinerama */
    if (!XineramaIsActive(x_display)) {
        fprintf(stderr, "Xinerama not active\n");
        XCloseDisplay(x_display);
        return 1;
    }
    
    /* Create display manager */
    DisplayManager *display = create_display_manager(x_display, screen, root);
    
    /* Setup monitors and zones */
    if (!setup_display_zones(display)) {
        XCloseDisplay(x_display);
        return 1;
    }
    
    /* Initialize window manager */
    wm.displays = display;
    // TODO: this needs to be changed when we have multiple displays
    wm.active_display = display;
    wm.display_count = 1;
    
    /* Select events */
    XSelectInput(x_display, root, 
                 SubstructureRedirectMask | SubstructureNotifyMask | PropertyChangeMask);
    
    /* Main event loop */
    XEvent event;
    while (1) {
        XNextEvent(x_display, &event);
        
        switch (event.type) {
            case MapRequest:
                handle_map_request(&event.xmaprequest);
                break;
            case UnmapNotify:
                handle_unmap_notify(&event.xunmap);
                break;
            case PropertyNotify:
                handle_property_notify(&event.xproperty);
                break;
        }
    }
    
    /* Cleanup */
    if (display->zones) free(display->zones);
    if (display->zone_clients) free(display->zone_clients);
    if (display->zone_current_index) free(display->zone_current_index);
    free(display);
    XCloseDisplay(x_display);
    
    return 0;
} 