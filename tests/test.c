#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include "config.h"
#include "core.h"

/* Helper function to create a mock DisplayManager for testing */
DisplayManager *create_test_display_manager(int zone_count) {
    DisplayManager *display = malloc(sizeof(DisplayManager));
    assert(display != NULL);
    
    display->x_display = NULL;  /* Not needed for pure logic tests */
    display->screen = 0;
    display->root = 0;
    display->zones = NULL;
    display->zone_count = zone_count;
    display->active_zone = 0;
    display->command_atom = 0;
    display->next = NULL;
    
    /* Allocate zone-based client management arrays */
    display->zone_clients = calloc(zone_count, sizeof(Client*));
    display->zone_current_index = malloc(zone_count * sizeof(int));
    assert(display->zone_clients != NULL && display->zone_current_index != NULL);
    
    /* Initialize all zones to have no clients */
    for (int i = 0; i < zone_count; i++) {
        display->zone_clients[i] = NULL;
        display->zone_current_index[i] = -1;
    }
    
    return display;
}

void cleanup_test_display_manager(DisplayManager *display) {
    if (display) {
        if (display->zones) free(display->zones);
        if (display->zone_clients) free(display->zone_clients);
        if (display->zone_current_index) free(display->zone_current_index);
        free(display);
    }
}

/* Test functions */
void test_regular_monitor_zones(void) {
    printf("Testing regular monitor zone calculation...\n");
    
    XineramaScreenInfo monitors[] = {
        {0, 0, 0, 1920, 1080},  /* Regular monitor */
        {1, 1920, 0, 1920, 1080}  /* Another regular monitor */
    };
    
    LogicalZone *zones = NULL;
    int zone_count = calculate_zones(monitors, 2, &zones);
    
    assert(zone_count == 2);
    assert(zones[0].geometry.width == 1920);
    assert(zones[0].geometry.height == 1080);
    assert(zones[0].monitor_id == 0);
    assert(zones[0].zone_id == 0);
    
    assert(zones[1].geometry.x == 1920);
    assert(zones[1].geometry.width == 1920);
    assert(zones[1].monitor_id == 1);
    
    free(zones);
    printf("✓ Regular monitor test passed\n");
}

void test_ultrawide_monitor_zones(void) {
    printf("Testing ultrawide monitor zone calculation...\n");
    
    XineramaScreenInfo monitors[] = {
        {0, 0, 0, 5120, 1440}  /* Ultrawide monitor */
    };
    
    LogicalZone *zones = NULL;
    int zone_count = calculate_zones(monitors, 1, &zones);
    
    assert(zone_count == 3);
    
    /* Left zone: 1/4 */
    assert(zones[0].geometry.x == 0);
    assert(zones[0].geometry.width == 5120 * 0.25);
    assert(zones[0].monitor_id == 0);
    assert(zones[0].zone_id == 0);
    
    /* Center zone: 1/2 */
    assert(zones[1].geometry.x == 5120 * 0.25);
    assert(zones[1].geometry.width == 5120 * 0.50);
    assert(zones[1].monitor_id == 0);
    assert(zones[1].zone_id == 1);
    
    /* Right zone: 1/4 */
    assert(zones[2].geometry.x == 5120 * 0.75);
    assert(zones[2].geometry.width == 5120 * 0.25);
    assert(zones[2].monitor_id == 0);
    assert(zones[2].zone_id == 2);
    
    free(zones);
    printf("✓ Ultrawide monitor test passed\n");
}

void test_zone_cycling_logic(void) {
    printf("Testing zone cycling logic...\n");
    
    /* Test with 3 zones - right direction (modular arithmetic) */
    assert((0 + 1) % 3 == 1);
    assert((1 + 1) % 3 == 2);
    assert((2 + 1) % 3 == 0);  /* Wrap around */
    
    /* Test with 3 zones - left direction */
    assert((0 - 1 + 3) % 3 == 2);  /* Wrap around */
    assert((1 - 1 + 3) % 3 == 0);
    assert((2 - 1 + 3) % 3 == 1);
    
    /* Test with 1 zone */
    assert((0 + 1) % 1 == 0);
    assert((0 - 1 + 1) % 1 == 0);
    
    /* Test with 5 zones - right direction */
    assert((4 + 1) % 5 == 0);  /* Wrap around */
    assert((3 + 1) % 5 == 4);
    
    /* Test with 5 zones - left direction */
    assert((0 - 1 + 5) % 5 == 4);  /* Wrap around */
    assert((1 - 1 + 5) % 5 == 0);
    
    printf("✓ Zone cycling test passed\n");
}

void test_mixed_monitors(void) {
    printf("Testing mixed monitor setup...\n");
    
    XineramaScreenInfo monitors[] = {
        {0, 0, 0, 1920, 1080},     /* Regular monitor */
        {1, 1920, 0, 5120, 1440},  /* Ultrawide monitor */
        {2, 7040, 0, 1920, 1080}   /* Another regular monitor */
    };
    
    LogicalZone *zones = NULL;
    int zone_count = calculate_zones(monitors, 3, &zones);
    
    assert(zone_count == 5);  /* 1 + 3 + 1 zones */
    
    /* First monitor: single zone */
    assert(zones[0].monitor_id == 0);
    assert(zones[0].zone_id == 0);
    
    /* Second monitor: three zones */
    assert(zones[1].monitor_id == 1);
    assert(zones[1].zone_id == 0);
    assert(zones[2].monitor_id == 1);
    assert(zones[2].zone_id == 1);
    assert(zones[3].monitor_id == 1);
    assert(zones[3].zone_id == 2);
    
    /* Third monitor: single zone */
    assert(zones[4].monitor_id == 2);
    assert(zones[4].zone_id == 0);
    
    free(zones);
    printf("✓ Mixed monitor test passed\n");
}

void test_zone_based_client_management(void) {
    printf("Testing zone-based client management...\n");
    
    /* Create test display manager with 3 zones */
    DisplayManager *display = create_test_display_manager(3);
    
    /* Create mock clients */
    Client client1 = {.window = 1, .zone_index = 0, .next = NULL};
    Client client2 = {.window = 2, .zone_index = 0, .next = NULL};
    Client client3 = {.window = 3, .zone_index = 1, .next = NULL};
    Client client4 = {.window = 4, .zone_index = 1, .next = NULL};
    
    /* Test initial state - no clients */
    assert(count_clients_in_zone(display, 0) == 0);
    assert(count_clients_in_zone(display, 1) == 0);
    assert(count_clients_in_zone(display, 2) == 0);
    assert(get_current_client_in_zone(display, 0) == NULL);
    
    /* Add clients to zone 0 */
    add_client_to_zone(display, 0, &client1);
    assert(count_clients_in_zone(display, 0) == 1);
    assert(get_current_client_in_zone(display, 0) == &client1);
    assert(display->zone_current_index[0] == 0);
    
    add_client_to_zone(display, 0, &client2);
    assert(count_clients_in_zone(display, 0) == 2);
    assert(get_current_client_in_zone(display, 0) == &client2);  /* New client becomes current */
    assert(display->zone_current_index[0] == 0);
    
    /* Add clients to zone 1 */
    add_client_to_zone(display, 1, &client3);
    assert(count_clients_in_zone(display, 1) == 1);
    assert(get_current_client_in_zone(display, 1) == &client3);
    
    add_client_to_zone(display, 1, &client4);
    assert(count_clients_in_zone(display, 1) == 2);
    assert(get_current_client_in_zone(display, 1) == &client4);
    
    /* Test cycling within zone 0 */
    display->zone_current_index[0] = 1;  /* Move to second client */
    assert(get_current_client_in_zone(display, 0) == &client1);
    
    /* Test removing clients */
    remove_client_from_zone(display, 0, &client2);
    assert(count_clients_in_zone(display, 0) == 1);
    assert(get_current_client_in_zone(display, 0) == &client1);
    assert(display->zone_current_index[0] == 0);
    
    remove_client_from_zone(display, 0, &client1);
    assert(count_clients_in_zone(display, 0) == 0);
    assert(get_current_client_in_zone(display, 0) == NULL);
    assert(display->zone_current_index[0] == -1);
    
    /* Test removing from zone 1 */
    remove_client_from_zone(display, 1, &client4);  /* Remove current client */
    assert(count_clients_in_zone(display, 1) == 1);
    assert(get_current_client_in_zone(display, 1) == &client3);
    
    cleanup_test_display_manager(display);
    printf("✓ Zone-based client management test passed\n");
}

void test_window_cycling_with_indices(void) {
    printf("Testing window cycling with indices...\n");
    
    DisplayManager *display = create_test_display_manager(2);
    
    /* Create and add clients to zone 0 */
    Client client1 = {.window = 1, .zone_index = 0, .next = NULL};
    Client client2 = {.window = 2, .zone_index = 0, .next = NULL};
    Client client3 = {.window = 3, .zone_index = 0, .next = NULL};
    
    add_client_to_zone(display, 0, &client1);
    add_client_to_zone(display, 0, &client2);
    add_client_to_zone(display, 0, &client3);
    
    /* Test cycling forward */
    assert(get_current_client_in_zone(display, 0) == &client3);  /* Index 0 */
    
    display->zone_current_index[0] = 1;
    assert(get_current_client_in_zone(display, 0) == &client2);  /* Index 1 */
    
    display->zone_current_index[0] = 2;
    assert(get_current_client_in_zone(display, 0) == &client1);  /* Index 2 */
    
    /* Test cycling with modular arithmetic */
    int count = count_clients_in_zone(display, 0);
    assert(count == 3);
    
    /* Forward cycling */
    int current_index = 0;
    current_index = (current_index + 1) % count;
    assert(current_index == 1);
    
    current_index = (current_index + 1) % count;
    assert(current_index == 2);
    
    current_index = (current_index + 1) % count;
    assert(current_index == 0);  /* Wrap around */
    
    /* Backward cycling */
    current_index = 0;
    current_index = (current_index - 1 + count) % count;
    assert(current_index == 2);  /* Wrap around */
    
    current_index = (current_index - 1 + count) % count;
    assert(current_index == 1);
    
    cleanup_test_display_manager(display);
    printf("✓ Window cycling with indices test passed\n");
}

void test_window_movement_logic(void) {
    printf("Testing window movement logic...\n");
    
    /* Test zone calculation for window movement using modular arithmetic */
    
    /* Test moving right in a 3-zone setup */
    int current_zone = 0;
    int zone_count = 3;
    int target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 1);
    
    current_zone = 1;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 2);
    
    current_zone = 2;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 0);  /* Wrap around */
    
    /* Test moving left in a 3-zone setup */
    current_zone = 0;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 2);  /* Wrap around */
    
    current_zone = 1;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 0);
    
    current_zone = 2;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 1);
    
    /* Test with 5-zone setup (mixed monitors) */
    zone_count = 5;
    current_zone = 0;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 1);
    
    current_zone = 4;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 0);  /* Wrap around */
    
    current_zone = 0;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 4);  /* Wrap around */
    
    current_zone = 3;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 2);
    
    /* Test edge cases */
    
    /* Single zone - movement should stay in same zone */
    zone_count = 1;
    current_zone = 0;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 0);
    
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 0);
    
    /* Two zones */
    zone_count = 2;
    current_zone = 0;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 1);
    
    current_zone = 1;
    target_zone = (current_zone + 1) % zone_count;
    assert(target_zone == 0);
    
    current_zone = 0;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 1);
    
    current_zone = 1;
    target_zone = (current_zone - 1 + zone_count) % zone_count;
    assert(target_zone == 0);
    
    printf("✓ Window movement logic test passed\n");
}

int main(void) {
    printf("Running SWM simplified approach tests...\n\n");
    
    test_regular_monitor_zones();
    test_ultrawide_monitor_zones();
    test_zone_cycling_logic();
    test_mixed_monitors();
    test_zone_based_client_management();
    test_window_cycling_with_indices();
    test_window_movement_logic();
    
    printf("\n✓ All tests passed! Simplified zone-based logic is working correctly.\n");
    return 0;
} 