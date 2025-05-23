#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <X11/Xlib.h>
#include "config.h"
#include "core.h"

/* Pure functions for core logic */

/* Calculate logical zones from physical monitors */
int calculate_zones(XineramaScreenInfo *monitors, int monitor_count, LogicalZone **zones) {
    assert(monitors != NULL);
    assert(zones != NULL);
    
    int zone_count = 0;
    
    /* Count total zones needed */
    for (int i = 0; i < monitor_count; i++) {
        if (monitors[i].width > ULTRAWIDE_THRESHOLD) {
            zone_count += 3;  /* Split ultrawide into 3 zones */
        } else {
            zone_count += 1;  /* Regular monitor = 1 zone */
        }
    }
    
    *zones = malloc(zone_count * sizeof(LogicalZone));
    assert(*zones != NULL);
    
    int zone_index = 0;
    for (int i = 0; i < monitor_count; i++) {
        if (monitors[i].width > ULTRAWIDE_THRESHOLD) {
            /* Split ultrawide monitor into 3 zones */
            int left_width = monitors[i].width * ZONE_LEFT_RATIO;
            int center_width = monitors[i].width * ZONE_CENTER_RATIO;
            int right_width = monitors[i].width * ZONE_RIGHT_RATIO;
            
            /* Left zone */
            (*zones)[zone_index] = (LogicalZone){
                .geometry = {monitors[i].x_org, monitors[i].y_org, left_width, monitors[i].height},
                .monitor_id = i,
                .zone_id = 0
            };
            zone_index++;
            
            /* Center zone */
            (*zones)[zone_index] = (LogicalZone){
                .geometry = {monitors[i].x_org + left_width, monitors[i].y_org, center_width, monitors[i].height},
                .monitor_id = i,
                .zone_id = 1
            };
            zone_index++;
            
            /* Right zone */
            (*zones)[zone_index] = (LogicalZone){
                .geometry = {monitors[i].x_org + left_width + center_width, monitors[i].y_org, right_width, monitors[i].height},
                .monitor_id = i,
                .zone_id = 2
            };
            zone_index++;
        } else {
            /* Regular monitor = single zone */
            (*zones)[zone_index] = (LogicalZone){
                .geometry = {monitors[i].x_org, monitors[i].y_org, monitors[i].width, monitors[i].height},
                .monitor_id = i,
                .zone_id = 0
            };
            zone_index++;
        }
    }
    
    return zone_count;
}

/* Find next window to focus on current zone with direction support */
Client *get_next_window_in_zone(Client *clients, int zone_index, Client *current, int direction) {
    if (clients == NULL) return NULL;
    assert(direction == 1 || direction == -1);
    
    /* Build array of clients in this zone for easier navigation */
    Client *zone_clients[256];  /* Reasonable limit */
    int count = 0;
    
    for (Client *c = clients; c != NULL; c = c->next) {
        if (c->zone_index == zone_index && count < 256) {
            zone_clients[count++] = c;
        }
    }
    
    if (count == 0) return NULL;
    if (count == 1) return zone_clients[0];
    
    /* Find current client index */
    int current_index = -1;
    for (int i = 0; i < count; i++) {
        if (zone_clients[i] == current) {
            current_index = i;
            break;
        }
    }
    
    /* If current not found, return first client */
    if (current_index == -1) {
        return zone_clients[0];
    }
    
    /* Calculate next index based on direction */
    int next_index;
    if (direction == 1) {
        /* Forward: next window */
        next_index = (current_index + 1) % count;
    } else {
        /* Backward: previous window */
        next_index = (current_index - 1 + count) % count;
    }
    
    return zone_clients[next_index];
}

/* Get next zone index for cycling with direction support */
int get_next_zone(int current_zone, int zone_count, int direction) {
    assert(zone_count > 0);
    assert(direction == 1 || direction == -1);
    
    if (direction == 1) {
        /* Right: next zone */
        return (current_zone + 1) % zone_count;
    } else {
        /* Left: previous zone */
        return (current_zone - 1 + zone_count) % zone_count;
    }
}

/* Find a window in the specified zone */
Client *get_window_in_zone(Client *clients, int zone_index) {
    for (Client *c = clients; c != NULL; c = c->next) {
        if (c->zone_index == zone_index) {
            return c;
        }
    }
    return NULL;
} 