#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <X11/Xlib.h>
#include "config.h"
#include "core.h"

/* Core logic functions */

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

/* Simplified zone-based client management functions */

Client *get_current_client_in_zone(DisplayManager *display, int zone) {
    if (!display || zone < 0 || zone >= display->zone_count) {
        return NULL;
    }
    
    Client *clients = display->zone_clients[zone];
    int current_index = display->zone_current_index[zone];
    
    if (!clients || current_index < 0) {
        return NULL;
    }
    
    /* Walk to the current index */
    Client *current = clients;
    for (int i = 0; i < current_index && current; i++) {
        current = current->next;
    }
    
    return current;
}

int count_clients_in_zone(DisplayManager *display, int zone) {
    if (!display || zone < 0 || zone >= display->zone_count) {
        return 0;
    }
    
    int count = 0;
    Client *current = display->zone_clients[zone];
    while (current) {
        count++;
        current = current->next;
    }
    
    return count;
}

void add_client_to_zone(DisplayManager *display, int zone, Client *client) {
    if (!display || !client || zone < 0 || zone >= display->zone_count) {
        return;
    }
    
    client->zone_index = zone;
    client->next = display->zone_clients[zone];
    display->zone_clients[zone] = client;
    
    /* If this is the first client in the zone, make it current */
    if (display->zone_current_index[zone] < 0) {
        display->zone_current_index[zone] = 0;
    }
}

void remove_client_from_zone(DisplayManager *display, int zone, Client *client) {
    if (!display || !client || zone < 0 || zone >= display->zone_count) {
        return;
    }
    
    Client **current = &display->zone_clients[zone];
    int index = 0;
    
    while (*current) {
        if (*current == client) {
            *current = client->next;
            
            /* Update current index if needed */
            int current_index = display->zone_current_index[zone];
            if (index == current_index) {
                /* Removed the current client */
                if (*current) {
                    /* Keep same index, now points to next client */
                } else if (index > 0) {
                    /* Move to previous client */
                    display->zone_current_index[zone] = index - 1;
                } else {
                    /* No clients left */
                    display->zone_current_index[zone] = -1;
                }
            } else if (index < current_index) {
                /* Removed a client before current, adjust index */
                display->zone_current_index[zone]--;
            }
            
            return;
        }
        current = &(*current)->next;
        index++;
    }
} 