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

/* Find next window to focus on current zone */
Client *get_next_window_in_zone(Client *clients, int zone_index, Client *current) {
    if (clients == NULL) return NULL;
    
    Client *first_in_zone = NULL;
    Client *next_in_zone = NULL;
    bool found_current = false;
    
    for (Client *c = clients; c != NULL; c = c->next) {
        if (c->zone_index == zone_index) {
            if (first_in_zone == NULL) {
                first_in_zone = c;
            }
            
            if (found_current && next_in_zone == NULL) {
                next_in_zone = c;
                break;
            }
            
            if (c == current) {
                found_current = true;
            }
        }
    }
    
    /* If we found current but no next, cycle to first */
    if (found_current && next_in_zone == NULL) {
        return first_in_zone;
    }
    
    /* If current not found or no current, return first */
    return next_in_zone ? next_in_zone : first_in_zone;
}

/* Get next zone index for cycling */
int get_next_zone(int current_zone, int zone_count) {
    assert(zone_count > 0);
    return (current_zone + 1) % zone_count;
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