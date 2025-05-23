#define _DEFAULT_SOURCE  /* For usleep() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Simple X11 test client for SWM testing */

int main(int argc, char *argv[]) {
    Display *display;
    Window window;
    XEvent event;
    int screen;
    int duration = 5; /* Default duration in seconds */
    
    /* Parse command line arguments */
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) duration = 5;
    }
    
    /* Open display */
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    screen = DefaultScreen(display);
    
    /* Create window */
    window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                100, 100, 300, 200, 1,
                                BlackPixel(display, screen),
                                WhitePixel(display, screen));
    
    /* Set window properties */
    XStoreName(display, window, "SWM Test Client");
    
    /* Select events */
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    
    /* Map window */
    XMapWindow(display, window);
    XFlush(display);
    
    printf("Test client window created (duration: %d seconds)\n", duration);
    
    /* Simple event loop with timeout */
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        while (XPending(display)) {
            XNextEvent(display, &event);
            
            switch (event.type) {
                case Expose:
                    /* Draw something simple */
                    XDrawString(display, window, DefaultGC(display, screen),
                               50, 100, "Test Window", 11);
                    break;
                case KeyPress:
                    /* Exit on any key press */
                    printf("Key pressed, exiting...\n");
                    goto cleanup;
                case DestroyNotify:
                    printf("Window destroyed, exiting...\n");
                    goto cleanup;
            }
        }
        
        /* Sleep briefly to avoid busy waiting */
        usleep(100000); /* 100ms */
    }
    
    printf("Test client duration expired, exiting...\n");
    
cleanup:
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
} 