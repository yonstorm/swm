#ifndef CORE_H
#define CORE_H

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
    Rectangle geometry;
    int zone_index;  /* Index into zones array */
    struct Client *next;
} Client;

/* Pure function prototypes */
int calculate_zones(XineramaScreenInfo *monitors, int monitor_count, LogicalZone **zones);
Client *get_next_window_in_zone(Client *clients, int zone_index, Client *current);
int get_next_zone(int current_zone, int zone_count);
Client *get_window_in_zone(Client *clients, int zone_index);

#endif /* CORE_H */ 