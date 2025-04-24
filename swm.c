#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

#define MAX_CLIENTS 256

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    char msg[256];
    XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
    fprintf(stderr,
        "X11 error: %s (req %d.%d resource 0x%lx)\n",
        msg, e->request_code, e->minor_code, e->resourceid);
    // returning 0 supresses default X11 error output
    return 0;
}

typedef struct {
  Window stack[MAX_CLIENTS];
  int   count;
  int   current;
} Monitor;

Monitor monitors[8]; // assume max 8 monitors
int num_monitors = 0;
XineramaScreenInfo *screens;
int focused_monitor = -1;

Display *dpy;
Window root;
Atom cmd_atom;

Window clients[MAX_CLIENTS];
int  nclients = 0;
int  current = -1;

unsigned long focused_border_color;
unsigned long unfocused_border_color;
int border_width = BORDER_WIDTH;

void SetFocusedMonitor(int mon) {
    if (mon < 0 || mon >= num_monitors) return;

    focused_monitor = mon;
    fprintf(stderr, "Focus moved to monitor %d\n", mon);
    // Should propably window focus here aswell to the top window
}

void set_active_borders(Window w) {
    // reset other windows borders to unfocused
    // this is a bit inefficient but works
    for (int i = 0; i < num_monitors; i++) {
        Monitor *m = &monitors[i];
        for (int j = 0; j < m->count; j++) {
            if (m->stack[j] != w) {
                XSetWindowBorder(dpy, m->stack[j], unfocused_border_color);
            }
        }
    }

    XSetWindowBorder(dpy, w, focused_border_color);
}

void kill_focused_window() {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    Monitor *m = &monitors[focused_monitor];
    if (m->count == 0 || m->current < 0 || m->current >= m->count)
        return;

    Window w = m->stack[m->current];

    // Send WM_DELETE_WINDOW message if supported
    Atom *protocols;
    int n;
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      for (int i = 0; i < n; i++) {
          if (protocols[i] == wm_delete) {
              XEvent ev;
              memset(&ev, 0, sizeof(ev));
              ev.xclient.type = ClientMessage;
              ev.xclient.window = w;
              ev.xclient.message_type = wm_protocols;
              ev.xclient.format = 32;
              ev.xclient.data.l[0] = wm_delete;
              ev.xclient.data.l[1] = CurrentTime;
              XSendEvent(dpy, w, False, NoEventMask, &ev);
              XFree(protocols);
              return;
          }
      }
      XFree(protocols);
    }

    XKillClient(dpy, w);
}

void move_window_relative(int dir) {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    Monitor *src = &monitors[focused_monitor];
    if (src->count == 0 || src->current < 0) return;

    Window w = src->stack[src->current];

    // Determine target monitor
    int best = -1;
    int current_x = screens[focused_monitor].x_org;

    for (int i = 0; i < num_monitors; i++) {
        if (i == focused_monitor) continue;
        int dx = screens[i].x_org - current_x;
        if ((dir < 0 && dx < 0) || (dir > 0 && dx > 0)) {
            if (best == -1 || abs(dx) < abs(screens[best].x_org - current_x))
                best = i;
        }
    }

    if (best == -1) return;

    Monitor *dst = &monitors[best];

    // Remove from src stack
    for (int i = src->current; i < src->count - 1; i++)
        src->stack[i] = src->stack[i + 1];
    src->count--;
    if (src->current >= src->count) src->current = src->count - 1;

    // Add to dst stack
    if (dst->count >= MAX_CLIENTS) return;

    dst->stack[dst->count++] = w;
    dst->current = dst->count - 1;

    // Move and resize to new monitor
    XMoveResizeWindow(dpy, w,
        screens[best].x_org,
        screens[best].y_org,
        screens[best].width,
        screens[best].height
    );

    // Focus and raise
    XRaiseWindow(dpy, w);
    
    Window active_win = src->stack[src->current];
    XSetInputFocus(dpy, active_win, RevertToPointerRoot, CurrentTime);

    // Update borders
    set_active_borders(active_win);
}

void focus_monitor_relative(int dir) {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    int target = -1;
    int current_x = screens[focused_monitor].x_org;

    // Find neighbor monitor in the desired direction
    for (int i = 0; i < num_monitors; i++) {
        if (i == focused_monitor) continue;

        int dx = screens[i].x_org - current_x;
        if ((dir < 0 && dx < 0) || (dir > 0 && dx > 0)) {
            if (target == -1 || abs(dx) < abs(screens[target].x_org - current_x)) {
                target = i;
            }
        }
    }

    if (target == -1)
        return;

    Monitor *m = &monitors[target];
    SetFocusedMonitor(target);

    if (m->count == 0 || m->current < 0) return;

    Window w = m->stack[m->current];
    fprintf(stderr, "Focusing window %lu on monitor %d\n", w, target);
    set_active_borders(w);
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);

    fprintf(stderr, "Focus moved to monitor %d\n", target);
}

int monitor_under_pointer() {
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;

    XQueryPointer(dpy, root, &root_return, &child_return,
                  &root_x, &root_y, &win_x, &win_y, &mask_return);

    for (int i = 0; i < num_monitors; i++) {
        if (root_x >= screens[i].x_org &&
            root_x < screens[i].x_org + screens[i].width &&
            root_y >= screens[i].y_org &&
            root_y < screens[i].y_org + screens[i].height) {
            return i;
        }
    }

    return 0; // fallback
}

void cycle(int dir) {
    int mon = focused_monitor; //monitor_under_pointer();
    Monitor *m = &monitors[mon];
    if (m->count <= 1) return;

    m->current = (m->current + dir + m->count) % m->count;

    Window w = m->stack[m->current];
    fprintf(stderr, "Cycling to window %lu\n", w);

    set_active_borders(w);
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);

    SetFocusedMonitor(mon);
}

void handle_map(XEvent *e) {
    Window w = e->xmaprequest.window;
    // Get attributes before managing
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, w, &wa)) return;
    // Skip override_redirect windows (tooltips, menus, etc.)
    if (wa.override_redirect) return;
    // Don't manage already-mapped or withdrawn windows
    if (wa.map_state != IsUnmapped) return;

    // Check if we already manage it
    for (int m = 0; m < num_monitors; m++) {
        Monitor *mon = &monitors[m];
        for (int i = 0; i < mon->count; i++) {
            if (mon->stack[i] == w) {
                XMapWindow(dpy, w);
                return;
            }
        }
    }
    // Pick monitor based on pointer location or rules
    int mon = focused_monitor >= 0 ? focused_monitor : monitor_under_pointer();

    XClassHint ch;
    XGetClassHint(dpy, w, &ch);
    int rule_mon = -1;

    for (int i = 0; i < sizeof(rules)/sizeof(rules[0]); i++) {
      if (ch.res_class && strcmp(ch.res_class, rules[i].class) == 0) {
          if (rules[i].monitor >= 0)
              mon = rules[i].monitor;
      }
    }

    XFree(ch.res_name);
    XFree(ch.res_class);

    mon = rule_mon >= 0 ? rule_mon : mon;

    Monitor *m = &monitors[mon];

    if (m->count >= MAX_CLIENTS) {
        fprintf(stderr, "Max clients reached on monitor %d\n", mon);
        return;
    }

    XSelectInput(dpy, w, EnterWindowMask | StructureNotifyMask);
    // Add to stack
    m->stack[m->count++] = w;
    m->current = m->count - 1;
    // Move and resize to monitor
    int bw = border_width;
    XMoveResizeWindow(dpy, w, 
        screens[mon].x_org,
        screens[mon].y_org,
        screens[mon].width,
        screens[mon].height
    );
    // Show and raise the window
    XMapWindow(dpy, w);
    XRaiseWindow(dpy, w);
    // borders
    XSetWindowBorderWidth(dpy, w, border_width);
    set_active_borders(w);

    // initial focus
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime); 
    XSync(dpy, False);
}

void handle_prop(XEvent *e) {
    if (e->xproperty.atom != cmd_atom) return;

    Atom actual;
    int format;
    unsigned long n, after;
    unsigned char *data = NULL;
    
    if (XGetWindowProperty(dpy, root, cmd_atom, 0, 32, False,
                           XA_STRING, &actual, &format,
                           &n, &after, &data) != Success || !data)
        return;

        
    if (strcmp((char*)data, "next") == 0) {
      cycle(+1);
    }      
    else if (strcmp((char*)data, "prev") == 0){
      cycle(-1);
    }
    else if (strcmp((char*)data, "focus_mon_left") == 0){
      focus_monitor_relative(-1);
    }
    else if (strcmp((char*)data, "focus_mon_right") == 0){
      focus_monitor_relative(+1);
    }
    else if (strcmp((char*)data, "kill_focused") == 0){
      kill_focused_window();
    }
    else if (strcmp((char*)data, "move_window_left") == 0){
      move_window_relative(-1);
    }
    else if (strcmp((char*)data, "move_window_right") == 0){
      move_window_relative(+1);
    }

    else if (strcmp((char*)data, "quit") == 0){
      XFree(data);
      XCloseDisplay(dpy);
      exit(0);
    }

    XFree(data);
}

void handle_destroy(XEvent *e) {
    Window w = e->xdestroywindow.window;
    fprintf(stderr, "Destroying window %lu\n", w);

    for (int m = 0; m < num_monitors; m++) {
        Monitor *mon = &monitors[m];
        for (int i = 0; i < mon->count; i++) {
            if (mon->stack[i] == w) {
                // Shift stack left to remove window
                for (int j = i; j < mon->count - 1; j++) {
                    mon->stack[j] = mon->stack[j+1];
                }
                mon->count--;

                // Update current index
                if (mon->current >= mon->count)
                    mon->current = mon->count - 1;

                // re-focus top window if any left
                if (mon->count > 0 && mon->current >= 0) {
                    Window nw = mon->stack[mon->current];
                    XRaiseWindow(dpy, nw);
                    XSetInputFocus(dpy, nw, RevertToPointerRoot, CurrentTime);
                    SetFocusedMonitor(m);
                }

                return;
            }
        }
    }
}


void handle_enter(XEvent *e) {
    Window w = e->xcrossing.window;

    fprintf(stderr, "Pointer entered window %lu\n", w);

    // Only trigger for real pointer motion (not fake events)
    if (e->xcrossing.mode != NotifyNormal || e->xcrossing.detail == NotifyInferior)
        return;

    // Only set focus if it's not already focused
    XWindowAttributes wa;
    if (XGetWindowAttributes(dpy, w, &wa) && wa.map_state == IsViewable) {
        XRaiseWindow(dpy, w);
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    }
    SetFocusedMonitor(focused_monitor);
    set_active_borders(w);
}

void load_colors(void) {
    XColor col;
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));

    if (XAllocNamedColor(dpy, cmap, FOCUSED_BORDER_COLOR, &col, &col))
        focused_border_color = col.pixel;
    else
        focused_border_color = WhitePixel(dpy, DefaultScreen(dpy));

    if (XAllocNamedColor(dpy, cmap, UNFOCUSED_BORDER_COLOR, &col, &col))
        unfocused_border_color = col.pixel;
    else
        unfocused_border_color = BlackPixel(dpy, DefaultScreen(dpy));
}

int main(void) {
    fprintf(stderr, "Starting Simple Window Manager\n");

    XEvent ev;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    load_colors();

    XSetErrorHandler(xerror_handler);

    root     = DefaultRootWindow(dpy);
    cmd_atom = XInternAtom(dpy, "_MYWM_CMD", False);

    screens = XineramaQueryScreens(dpy, &num_monitors);
    if (screens == NULL) {
        fprintf(stderr, "Xinerama not supported\n");
        return 1;
    }

    fprintf(stderr, "Found %d monitors\n", num_monitors);
    // Initialize monitors
    for (int i = 0; i < num_monitors; i++) {
        monitors[i].count = 0;
        monitors[i].current = -1;
    }

    int screen = DefaultScreen(dpy);

    SetFocusedMonitor(monitor_under_pointer());

    // listen for new windows and our property
    XSelectInput(dpy, root,
        SubstructureRedirectMask |
        SubstructureNotifyMask  |
        PropertyChangeMask
    );

    while (!XNextEvent(dpy, &ev)) {
        switch (ev.type) {
          case MapRequest:   handle_map(&ev);    break;
          case PropertyNotify: handle_prop(&ev); break;
          case DestroyNotify: handle_destroy(&ev); break;
          case EnterNotify: handle_enter(&ev); break;
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
