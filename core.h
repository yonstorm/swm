#ifndef CORE_H
#define CORE_H

#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

/* Data structures */
typedef struct {
    int x, y, width, height;
} Rectangle;

typedef struct {
    Rectangle geometry;
    int monitor_id;  /* Physical monitor this zone belongs to */
    int zone_id;     /* 0 for single zone, 0-2 for ultrawide zones */
} LogicalZone;

typedef struct Client {
    Window window;
    int zone_index;
    struct Client *next;
} Client;

typedef struct DisplayManager {
    Display *x_display;
    int screen;
    Window root;
    
    LogicalZone *zones;
    int zone_count;
    int active_zone;
    
    /* Ultra-simple zone-based client management */
    Client **zone_clients;        /* Array of client lists per zone */
    int *zone_current_index;      /* Array of current client indices per zone */
    
    Atom command_atom;
    struct DisplayManager *next;
} DisplayManager;

typedef struct {
    DisplayManager *displays;
    DisplayManager *active_display;
    int display_count;
} WindowManager;

/* Core functions */
int calculate_zones(XineramaScreenInfo *monitors, int monitor_count, LogicalZone **zones);

/* Simplified zone-based client management functions */
Client *get_current_client_in_zone(DisplayManager *display, int zone);
int count_clients_in_zone(DisplayManager *display, int zone);
void add_client_to_zone(DisplayManager *display, int zone, Client *client);
void remove_client_from_zone(DisplayManager *display, int zone, Client *client);

#endif /* CORE_H */ 