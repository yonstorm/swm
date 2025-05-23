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

typedef struct {
    Display *display;
    Window root;
    int screen;
    LogicalZone *zones;
    int zone_count;
    Client *clients;
    int active_zone;
    Client *focused_client;
    Atom command_atom;
} WindowManager;

/* Global state */
static WindowManager wm = {0};

/* X11 helper functions */

void set_window_border(Display *display, Window window, unsigned long color) {
    XSetWindowBorder(display, window, color);
    XSetWindowBorderWidth(display, window, BORDER_WIDTH);
}

void focus_window(Display *display, Window window) {
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, window);
}

void resize_window_to_zone(Display *display, Window window, LogicalZone *zone) {
    XMoveResizeWindow(display, window, 
                      zone->geometry.x, zone->geometry.y,
                      zone->geometry.width - 2 * BORDER_WIDTH,
                      zone->geometry.height - 2 * BORDER_WIDTH);
}

/* Client management */

Client *add_client(Window window, int zone_index) {
    Client *client = malloc(sizeof(Client));
    assert(client != NULL);
    
    client->window = window;
    client->zone_index = zone_index;
    client->next = wm.clients;
    wm.clients = client;
    
    return client;
}

void remove_client(Window window) {
    Client **current = &wm.clients;
    
    while (*current != NULL) {
        if ((*current)->window == window) {
            Client *to_remove = *current;
            *current = (*current)->next;
            
            if (wm.focused_client == to_remove) {
                wm.focused_client = NULL;
            }
            
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

Client *find_client(Window window) {
    for (Client *c = wm.clients; c != NULL; c = c->next) {
        if (c->window == window) {
            return c;
        }
    }
    return NULL;
}

/* Window manager operations */

void cycle_window_focus(void) {
    if (wm.zone_count == 0) return;
    
    Client *next = get_next_window_in_zone(wm.clients, wm.active_zone, wm.focused_client, 1);
    
    if (next != NULL) {
        /* Unfocus current window */
        if (wm.focused_client != NULL) {
            set_window_border(wm.display, wm.focused_client->window, UNFOCUS_COLOR);
        }
        
        /* Focus next window */
        wm.focused_client = next;
        set_window_border(wm.display, next->window, FOCUS_COLOR);
        focus_window(wm.display, next->window);
    }
}

void cycle_window_focus_direction(int direction) {
    if (wm.zone_count == 0) return;
    
    Client *next = get_next_window_in_zone(wm.clients, wm.active_zone, wm.focused_client, direction);
    
    if (next != NULL) {
        /* Unfocus current window */
        if (wm.focused_client != NULL) {
            set_window_border(wm.display, wm.focused_client->window, UNFOCUS_COLOR);
        }
        
        /* Focus next window */
        wm.focused_client = next;
        set_window_border(wm.display, next->window, FOCUS_COLOR);
        focus_window(wm.display, next->window);
    }
}

void cycle_monitor_focus(void) {
    if (wm.zone_count <= 1) return;
    
    /* Unfocus current window */
    if (wm.focused_client != NULL) {
        set_window_border(wm.display, wm.focused_client->window, UNFOCUS_COLOR);
    }
    
    /* Move to next zone */
    wm.active_zone = get_next_zone(wm.active_zone, wm.zone_count, 1);
    
    /* Find and focus a window in the new zone */
    Client *window_in_zone = get_window_in_zone(wm.clients, wm.active_zone);
    if (window_in_zone != NULL) {
        wm.focused_client = window_in_zone;
        set_window_border(wm.display, window_in_zone->window, FOCUS_COLOR);
        focus_window(wm.display, window_in_zone->window);
    } else {
        wm.focused_client = NULL;
    }
}

void cycle_monitor_focus_direction(int direction) {
    if (wm.zone_count <= 1) return;
    
    /* Unfocus current window */
    if (wm.focused_client != NULL) {
        set_window_border(wm.display, wm.focused_client->window, UNFOCUS_COLOR);
    }
    
    /* Move to next zone in specified direction */
    wm.active_zone = get_next_zone(wm.active_zone, wm.zone_count, direction);
    
    /* Find and focus a window in the new zone */
    Client *window_in_zone = get_window_in_zone(wm.clients, wm.active_zone);
    if (window_in_zone != NULL) {
        wm.focused_client = window_in_zone;
        set_window_border(wm.display, window_in_zone->window, FOCUS_COLOR);
        focus_window(wm.display, window_in_zone->window);
    } else {
        wm.focused_client = NULL;
    }
}

/* Event handlers */

void handle_map_request(XMapRequestEvent *event) {
    Window window = event->window;
    
    /* Add window to active zone */
    Client *client = add_client(window, wm.active_zone);
    
    /* Resize window to fit zone */
    if (wm.active_zone < wm.zone_count) {
        resize_window_to_zone(wm.display, window, &wm.zones[wm.active_zone]);
    }
    
    /* Set border and map window */
    set_window_border(wm.display, window, UNFOCUS_COLOR);
    XMapWindow(wm.display, window);
    
    /* Focus the new window */
    if (wm.focused_client != NULL) {
        set_window_border(wm.display, wm.focused_client->window, UNFOCUS_COLOR);
    }
    wm.focused_client = client;
    set_window_border(wm.display, window, FOCUS_COLOR);
    focus_window(wm.display, window);
}

void handle_unmap_notify(XUnmapEvent *event) {
    remove_client(event->window);
    
    /* If we removed the focused window, focus another in the same zone */
    if (wm.focused_client == NULL) {
        Client *next = get_window_in_zone(wm.clients, wm.active_zone);
        if (next != NULL) {
            wm.focused_client = next;
            set_window_border(wm.display, next->window, FOCUS_COLOR);
            focus_window(wm.display, next->window);
        }
    }
}

void handle_property_notify(XPropertyEvent *event) {
    if (event->atom == wm.command_atom && event->window == wm.root) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        
        if (XGetWindowProperty(wm.display, wm.root, wm.command_atom,
                              0, 1, True, XA_INTEGER,
                              &actual_type, &actual_format,
                              &nitems, &bytes_after, &data) == Success) {
            
            if (data != NULL && nitems > 0) {
                int command = *(int*)data;
                
                switch (command) {
                    case CMD_CYCLE_WINDOW:
                        cycle_window_focus();
                        break;
                    case CMD_CYCLE_WINDOW_NEXT:
                        cycle_window_focus_direction(1);
                        break;
                    case CMD_CYCLE_WINDOW_PREV:
                        cycle_window_focus_direction(-1);
                        break;
                    case CMD_CYCLE_MONITOR:
                        cycle_monitor_focus();
                        break;
                    case CMD_CYCLE_MONITOR_LEFT:
                        cycle_monitor_focus_direction(-1);
                        break;
                    case CMD_CYCLE_MONITOR_RIGHT:
                        cycle_monitor_focus_direction(1);
                        break;
                    case CMD_QUIT:
                        exit(0);
                        break;
                }
                
                XFree(data);
            }
        }
    }
}

/* Initialization */

int setup_monitors(void) {
    int monitor_count;
    XineramaScreenInfo *monitors = XineramaQueryScreens(wm.display, &monitor_count);
    
    if (monitors == NULL || monitor_count == 0) {
        fprintf(stderr, "No monitors detected\n");
        return 0;
    }
    
    wm.zone_count = calculate_zones(monitors, monitor_count, &wm.zones);
    
    printf("Detected %d monitors, created %d logical zones:\n", monitor_count, wm.zone_count);
    for (int i = 0; i < wm.zone_count; i++) {
        printf("Zone %d: %dx%d+%d+%d (monitor %d, zone %d)\n", i,
               wm.zones[i].geometry.width, wm.zones[i].geometry.height,
               wm.zones[i].geometry.x, wm.zones[i].geometry.y,
               wm.zones[i].monitor_id, wm.zones[i].zone_id);
    }
    
    XFree(monitors);
    return 1;
}

int main(void) {
    /* Open display */
    wm.display = XOpenDisplay(NULL);
    if (wm.display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    wm.screen = DefaultScreen(wm.display);
    wm.root = RootWindow(wm.display, wm.screen);
    
    /* Check for Xinerama */
    if (!XineramaIsActive(wm.display)) {
        fprintf(stderr, "Xinerama not active\n");
        XCloseDisplay(wm.display);
        return 1;
    }
    
    /* Setup monitors and zones */
    if (!setup_monitors()) {
        XCloseDisplay(wm.display);
        return 1;
    }
    
    /* Create command atom */
    wm.command_atom = XInternAtom(wm.display, COMMAND_PROPERTY, False);
    
    /* Select events */
    XSelectInput(wm.display, wm.root, 
                 SubstructureRedirectMask | SubstructureNotifyMask | PropertyChangeMask);
    
    /* Main event loop */
    XEvent event;
    while (1) {
        XNextEvent(wm.display, &event);
        
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
    free(wm.zones);
    XCloseDisplay(wm.display);
    return 0;
} 