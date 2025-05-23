#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include "config.h"
#include "core.h"

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

void test_zone_cycling(void) {
    printf("Testing zone cycling logic...\n");
    
    /* Test with 3 zones - right direction */
    assert(get_next_zone(0, 3, 1) == 1);
    assert(get_next_zone(1, 3, 1) == 2);
    assert(get_next_zone(2, 3, 1) == 0);  /* Wrap around */
    
    /* Test with 3 zones - left direction */
    assert(get_next_zone(0, 3, -1) == 2);  /* Wrap around */
    assert(get_next_zone(1, 3, -1) == 0);
    assert(get_next_zone(2, 3, -1) == 1);
    
    /* Test with 1 zone */
    assert(get_next_zone(0, 1, 1) == 0);
    assert(get_next_zone(0, 1, -1) == 0);
    
    /* Test with 5 zones - right direction */
    assert(get_next_zone(4, 5, 1) == 0);  /* Wrap around */
    assert(get_next_zone(3, 5, 1) == 4);
    
    /* Test with 5 zones - left direction */
    assert(get_next_zone(0, 5, -1) == 4);  /* Wrap around */
    assert(get_next_zone(1, 5, -1) == 0);
    
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

void test_window_cycling(void) {
    printf("Testing window cycling logic...\n");
    
    /* Create mock clients for testing */
    Client client1 = {.window = 1, .zone_index = 0, .next = NULL};
    Client client2 = {.window = 2, .zone_index = 0, .next = NULL};
    Client client3 = {.window = 3, .zone_index = 0, .next = NULL};
    Client client4 = {.window = 4, .zone_index = 1, .next = NULL};
    
    /* Link them */
    client1.next = &client2;
    client2.next = &client3;
    client3.next = &client4;
    
    /* Test forward cycling in zone 0 */
    Client *next = get_next_window_in_zone(&client1, 0, &client1, 1);
    assert(next == &client2);
    
    next = get_next_window_in_zone(&client1, 0, &client2, 1);
    assert(next == &client3);
    
    next = get_next_window_in_zone(&client1, 0, &client3, 1);
    assert(next == &client1);  /* Should wrap around */
    
    /* Test backward cycling in zone 0 */
    Client *prev = get_next_window_in_zone(&client1, 0, &client1, -1);
    assert(prev == &client3);  /* Should wrap to last */
    
    prev = get_next_window_in_zone(&client1, 0, &client3, -1);
    assert(prev == &client2);
    
    prev = get_next_window_in_zone(&client1, 0, &client2, -1);
    assert(prev == &client1);
    
    /* Test finding window in zone 1 */
    Client *found = get_window_in_zone(&client1, 1);
    assert(found == &client4);
    
    /* Test finding window in non-existent zone */
    found = get_window_in_zone(&client1, 99);
    assert(found == NULL);
    
    /* Test single window cycling */
    next = get_next_window_in_zone(&client4, 1, &client4, 1);
    assert(next == &client4);  /* Should return itself */
    
    prev = get_next_window_in_zone(&client4, 1, &client4, -1);
    assert(prev == &client4);  /* Should return itself */
    
    printf("✓ Window cycling test passed\n");
}

void test_window_movement_logic(void) {
    printf("Testing window movement logic...\n");
    
    /* Test zone calculation for window movement */
    
    /* Test moving right in a 3-zone setup */
    int target_zone = get_next_zone(0, 3, 1);  /* Move right from zone 0 */
    assert(target_zone == 1);
    
    target_zone = get_next_zone(1, 3, 1);  /* Move right from zone 1 */
    assert(target_zone == 2);
    
    target_zone = get_next_zone(2, 3, 1);  /* Move right from zone 2 (wrap) */
    assert(target_zone == 0);
    
    /* Test moving left in a 3-zone setup */
    target_zone = get_next_zone(0, 3, -1);  /* Move left from zone 0 (wrap) */
    assert(target_zone == 2);
    
    target_zone = get_next_zone(1, 3, -1);  /* Move left from zone 1 */
    assert(target_zone == 0);
    
    target_zone = get_next_zone(2, 3, -1);  /* Move left from zone 2 */
    assert(target_zone == 1);
    
    /* Test with 5-zone setup (mixed monitors) */
    target_zone = get_next_zone(0, 5, 1);  /* Move right from zone 0 */
    assert(target_zone == 1);
    
    target_zone = get_next_zone(4, 5, 1);  /* Move right from last zone (wrap) */
    assert(target_zone == 0);
    
    target_zone = get_next_zone(0, 5, -1);  /* Move left from first zone (wrap) */
    assert(target_zone == 4);
    
    target_zone = get_next_zone(3, 5, -1);  /* Move left from zone 3 */
    assert(target_zone == 2);
    
    /* Test edge cases */
    
    /* Single zone - movement should stay in same zone */
    target_zone = get_next_zone(0, 1, 1);
    assert(target_zone == 0);
    
    target_zone = get_next_zone(0, 1, -1);
    assert(target_zone == 0);
    
    /* Two zones */
    target_zone = get_next_zone(0, 2, 1);
    assert(target_zone == 1);
    
    target_zone = get_next_zone(1, 2, 1);
    assert(target_zone == 0);
    
    target_zone = get_next_zone(0, 2, -1);
    assert(target_zone == 1);
    
    target_zone = get_next_zone(1, 2, -1);
    assert(target_zone == 0);
    
    printf("✓ Window movement logic test passed\n");
}

int main(void) {
    printf("Running SWM pure function tests...\n\n");
    
    test_regular_monitor_zones();
    test_ultrawide_monitor_zones();
    test_zone_cycling();
    test_mixed_monitors();
    test_window_cycling();
    test_window_movement_logic();
    
    printf("\n✓ All tests passed! Core logic is working correctly.\n");
    return 0;
} 