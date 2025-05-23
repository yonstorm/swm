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

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    char msg[256];
    XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
    fprintf(stderr,
        "X11 error: %s (req %d.%d resource 0x%lx)\n",
        msg, e->request_code, e->minor_code, e->resourceid);
    // returning 0 supresses default X11 error output
    return 0;
}


static void cycle(int dir);
static void focus_monitor_relative(int dir);
static void kill_focused_window(void);
static void move_window_relative(int dir);


static void cmd_next(void)               { cycle(+1); }
static void cmd_prev(void)               { cycle(-1); }
static void cmd_focus_mon_left(void)     { focus_monitor_relative(-1); }
static void cmd_focus_mon_right(void)    { focus_monitor_relative(+1); }
static void cmd_kill_focused(void)       { kill_focused_window(); }
static void cmd_move_window_left(void)   { move_window_relative(-1); }
static void cmd_move_window_right(void)  { move_window_relative(+1); }
static void cmd_quit(void) {
    XCloseDisplay(dpy);
    exit(0);
}

typedef struct {
    const char *cmd;
    void (*fn)(void);
} CmdHandler;

static const CmdHandler handlers[] = {
    { "next",             cmd_next            },
    { "prev",             cmd_prev            },
    { "focus_mon_left",   cmd_focus_mon_left  },
    { "focus_mon_right",  cmd_focus_mon_right },
    { "move_window_left", cmd_move_window_left },
    { "move_window_right",cmd_move_window_right},
    { "kill_focused",     cmd_kill_focused    },
    { "quit",             cmd_quit            },
};
static const size_t n_handlers = sizeof(handlers) / sizeof(handlers[0]);


static void set_focused_monitor(int mon) {
    if (mon < 0 || mon >= num_monitors) {
      fprintf(stderr, "set_focused_monitor: invalid monitor %d\n", mon);
      return;
    }

    if (focused_monitor == mon) {
      fprintf(stderr, "Monitor %d already focused\n", mon);
      return;
    }

    focused_monitor = mon;
    fprintf(stderr, "Focus moved to monitor %d\n", mon);
    // Should propably window focus here aswell to the top window
}

static void set_active_borders(Window w) {
    static Window last_focused = None;
    
    if (last_focused && last_focused != w) {
        XSetWindowBorder(dpy, last_focused, unfocused_border_color);
    }

    XSetWindowBorder(dpy, w, focused_border_color);
    last_focused = w;
}

static void kill_focused_window() {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    Monitor *m = &monitors[focused_monitor];
    if (m->count == 0 || m->current < 0 || m->current >= m->count)
        return;

    Window w = m->stack[m->current];
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

    // Try polite shutdown first via WM_DELETE_WINDOW
    Atom *protocols = NULL;
    int n = 0;
    int sent_delete = False;

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
              if (!XSendEvent(dpy, w, False, NoEventMask, &ev)) {
                  fprintf(stderr, "kill_focused_window: WM_DELETE send failed for 0x%lx\n", w);
              }
              sent_delete = True;
              break;
          }
      }
    }

    XFree(protocols);

    if(!sent_delete) {
      XKillClient(dpy, w);
    }
    
    XSync(dpy, False);
}

static int find_neighbor_monitor(int focused, int dir) {
    int found = -1;
    int current_x = screens[focused_monitor].x_org;

    for (int i = 0; i < num_monitors; i++) {
        if (i == focused_monitor) continue;
        int dx = screens[i].x_org - current_x;
        if ((dir < 0 && dx < 0) || (dir > 0 && dx > 0)) {
            if (found == -1 || abs(dx) < abs(screens[found].x_org - current_x))
                found = i;
        }
    }
    return found;
}

static void move_window_relative(int dir) {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    Monitor *src = &monitors[focused_monitor];
    if (src->count == 0 || src->current < 0) return;

    Window w = src->stack[src->current];

    // Determine target monitor
    int best = find_neighbor_monitor(focused_monitor, dir);
    if (best < 0){
      fprintf(stderr, "move_window_relative: no neighbor in dir %d\n", dir);
      return;
    }
    Monitor *dst = &monitors[best];

    // Remove winodw from src stack
    for (int i = src->current; i < src->count - 1; i++)
        src->stack[i] = src->stack[i + 1];
    src->count--;
    src->current = (src->count > 0) ? src->current % src->count : -1;

    // Add to dst stack
    if (dst->count >= MAX_CLIENTS) {
      fprintf(stderr, "move_window_relative: monitor %d full\n", best);
      return;
    }

    dst->stack[dst->count] = w;
    dst->current = dst->count++;

    // Move and resize to new monitor
    XMoveResizeWindow(dpy, w,
        screens[best].x_org, screens[best].y_org,
        screens[best].width, screens[best].height
    );

    XRaiseWindow(dpy, w);
    
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    // invalid dereference?
    //Window active_win = src->stack[src->current];
    // Update borders
    set_active_borders(w);
    XSync(dpy, False);
}

static void focus_monitor_relative(int dir) {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    int target = find_neighbor_monitor(focused_monitor, dir);
    if (target < 0) {
        fprintf(stderr, "focus_monitor_relative: no neighbor in dir %d\n", dir);
        return;
    }

    set_focused_monitor(target);

    Monitor *m = &monitors[target];
    if (m->count <= 0 || m->current < 0) {
      return;
    }

    Window w = m->stack[m->current];
    fprintf(stderr, "Focusing window %lu on monitor %d\n", w, target);
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    set_active_borders(w);
    XSync(dpy, False);
}

static int get_monitor_at_pointer() {
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;

    if(!XQueryPointer(dpy, root, &root_return, &child_return,
                  &root_x, &root_y, &win_x, &win_y, &mask_return)) {
          fprintf(stderr, "get_monitor_at_pointer: XQueryPointer failed\n");
          return focused_monitor >= 0 ? focused_monitor : 0;
    };

    // TODO: use cached value that is set on EnterNotify 
    for (int i = 0; i < num_monitors; ++i) {
        int x0 = screens[i].x_org;
        int y0 = screens[i].y_org;
        int x1 = x0 + screens[i].width;
        int y1 = y0 + screens[i].height;

        if (root_x >= x0 && root_x < x1 &&
            root_y >= y0 && root_y < y1) {
            return i;
        }
    }

    return (focused_monitor >= 0) ? focused_monitor : 0; // fallback
}

static void cycle(int dir) {
    if (focused_monitor < 0 || focused_monitor >= num_monitors)
        return;

    Monitor *m = &monitors[focused_monitor];
    if (m->count <= 1) return;

    m->current = (m->current + dir + m->count) % m->count;

    Window w = m->stack[m->current];
    fprintf(stderr, "cycle: monitor %d → window 0x%lx\n", focused_monitor, w);

    set_active_borders(w);
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    XSync(dpy, False);
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
    int mon_idx = focused_monitor >= 0 ? focused_monitor : get_monitor_at_pointer();

    XClassHint ch;
    if(XGetClassHint(dpy, w, &ch)) {
        size_t nrules = sizeof(rules) / sizeof(rules[0]);
        if(ch.res_class) {
            for (size_t i = 0; i < nrules; ++i) {
                if (strcmp(ch.res_class, rules[i].class) == 0 &&
                    rules[i].monitor >= 0) {
                    mon_idx = rules[i].monitor;
                    break;
                }
            }
        }

        XFree(ch.res_name);
        XFree(ch.res_class);
    } else {
        fprintf(stderr, "handle_map: XGetClassHint failed for 0x%lx\n", w);
    }

    Monitor *m = &monitors[mon_idx];
    if (m->count >= MAX_CLIENTS) {
        fprintf(stderr, "handle_map: max clients reached on monitor %d\n", mon_idx);
        return;
    }

    XSelectInput(dpy, w, EnterWindowMask | StructureNotifyMask);
    m->stack[m->count] = w;
    m->current = m->count++;
    XMoveResizeWindow(dpy, w, 
        screens[mon_idx].x_org,
        screens[mon_idx].y_org,
        screens[mon_idx].width,
        screens[mon_idx].height
    );
    XMapWindow(dpy, w);
    XRaiseWindow(dpy, w);
    XSetWindowBorderWidth(dpy, w, border_width);
    set_active_borders(w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime); 
    XSync(dpy, False);
}

static void handle_prop(XEvent *e) {
    if (e->xproperty.atom != cmd_atom)
        return;

    Atom actual;
    int format;
    unsigned long n, after;
    unsigned char *data = NULL;

    if (XGetWindowProperty(dpy, root, cmd_atom, 0, 32, False,
                           XA_STRING, &actual, &format,
                           &n, &after, &data) != Success || !data)
        return;

    // Ensure C-string
    if (format != 8 || n == 0) {
        XFree(data);
        return;
    }

    int handled = False;
    for (size_t i = 0; i < n_handlers; ++i) {
        if (strcmp((char*)data, handlers[i].cmd) == 0) {
            handlers[i].fn();
            handled = True;
            break;
        }
    }

    if (!handled) {
        fprintf(stderr, "handle_prop: unknown command '%.*s'\n", (int)n, data);
    }

    XFree(data);
}

/*
 * remove_client: locate and remove window w from monitors[]
 * @w: the window to remove
 * @out_mon: if non-NULL, set to the monitor index it was on
 * @out_idx: if non-NULL, set to the index in that monitor’s stack
 * Returns true if the window was found & removed, false otherwise.
 */
static int remove_client(Window w, int *out_mon, int *out_idx)
{
    for (int mi = 0; mi < num_monitors; ++mi) {
        Monitor *mon = &monitors[mi];
        for (int ci = 0; ci < mon->count; ++ci) {
            if (mon->stack[ci] == w) {
                /* shift left */
                for (int j = ci; j + 1 < mon->count; ++j) {
                    mon->stack[j] = mon->stack[j+1];
                }
                mon->count--;
                /* adjust current */
                if (mon->current >= mon->count)
                    mon->current = mon->count - 1;
                /* report back */
                if (out_mon) *out_mon = mi;
                if (out_idx) *out_idx = ci;
                return True;
            }
        }
    }
    return False;
}

static void handle_destroy(XEvent *e) {
    Window w = e->xdestroywindow.window;
    fprintf(stderr, "handle_destroy: destroying window 0x%lx\n", w);

    int mon_idx, idx;
    if (!remove_client(w, &mon_idx, &idx)) {
        fprintf(stderr, "handle_destroy: window 0x%lx not managed\n", w);
        return;
    }

    Monitor *mon = &monitors[mon_idx];
    // If there is still a window on that monitor, focus it
    if (mon->count > 0) {
        Window nw = mon->stack[mon->current];
        fprintf(stderr,
                "handle_destroy: new focus 0x%lx on monitor %d\n",
                nw, mon_idx);
        XRaiseWindow(dpy, nw);
        XSetInputFocus(dpy, nw, RevertToPointerRoot, CurrentTime);
        set_active_borders(nw);
        set_focused_monitor(mon_idx);
    }

    XSync(dpy, False);
}


static void handle_enter(XEvent *e) {
    /* Validate pointer focus state */
    if (focused_monitor < 0 || focused_monitor >= num_monitors) return;

    /* Only real enter events */
    if (e->xcrossing.mode != NotifyNormal ||
        e->xcrossing.detail == NotifyInferior)
        return;

    Window w = e->xcrossing.window;

    /* Ignore windows we don’t manage */
    //if (!is_managed_window(w)) return;

    /* Avoid re-focusing the same window repeatedly */
    static Window last_entered = 0;
    if (last_entered == w) return;
    last_entered = w;

    /* Check it’s viewable before focusing */
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, w, &wa) ||
        wa.map_state != IsViewable) {
        fprintf(stderr, "handle_enter: cannot focus 0x%lx (not viewable)\n", w);
        return;
    }

    /* Do the focus/raise/borders in one block */
    fprintf(stderr, "handle_enter: focusing window 0x%lx\n", w);
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    set_active_borders(w);
    set_focused_monitor(focused_monitor);

    XSync(dpy, False);
}

static void load_colors(void) {
  int scr = DefaultScreen(dpy);
    Colormap cmap = DefaultColormap(dpy, scr);

    // Helper to allocate a named color or fallback 
    unsigned long alloc_or_fallback(const char *name, unsigned long fb) {
        XColor col;
        if (XAllocNamedColor(dpy, cmap, name, &col, &col)) {
            return col.pixel;
        }
        fprintf(stderr, "load_colors: could not alloc '%s', using fallback\n", name);
        return fb;
    }

    focused_border_color   = alloc_or_fallback(FOCUSED_BORDER_COLOR,   WhitePixel(dpy, scr));
    unfocused_border_color = alloc_or_fallback(UNFOCUSED_BORDER_COLOR, BlackPixel(dpy, scr));
}

int main(void) {
      fprintf(stderr, "Starting Simple Window Manager\n");

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    load_colors();
    XSetErrorHandler(xerror_handler);
    root     = DefaultRootWindow(dpy);
    cmd_atom = XInternAtom(dpy, "_MYWM_CMD", False);

    /* Grab substructure events; error means another WM is running */
    if (XSelectInput(dpy, root,
                     SubstructureRedirectMask |
                     SubstructureNotifyMask  |
                     PropertyChangeMask)
        == BadAccess)
    {
        fprintf(stderr, "Error: another window manager is running\n");
        return 1;
    }

    /* Adopt already‐mapped windows */
    {
        Window dummy1, *children;
        unsigned int n;
        if (XQueryTree(dpy, root, &dummy1, &dummy1, &children, &n)) {
            for (unsigned int i = 0; i < n; ++i) {
                XEvent ev = { .type = MapRequest, .xmaprequest.window = children[i] };
                handle_map(&ev);
            }
            if (children) XFree(children);
        }
    }

    /* Discover monitors */
    screens = XineramaQueryScreens(dpy, &num_monitors);
    if (!screens) {
        fprintf(stderr, "Xinerama not supported\n");
        return 1;
    }
    fprintf(stderr, "Found %d monitors\n", num_monitors);
    for (int i = 0; i < num_monitors; ++i) {
        monitors[i].count   = 0;
        monitors[i].current = -1;
    }

    /* Set initial focus */
    set_focused_monitor(get_monitor_at_pointer());
    XSync(dpy, False);

    /* Main event loop */
    XEvent ev;
    while (XNextEvent(dpy, &ev) == 0) {
        switch (ev.type) {
          case MapRequest:      handle_map(&ev);     break;
          case UnmapNotify:     handle_destroy(&ev); break;
          case DestroyNotify:   handle_destroy(&ev); break;
          //case ConfigureRequest:handle_configure(&ev); break;
          case PropertyNotify:  handle_prop(&ev);    break;
          case EnterNotify:     handle_enter(&ev);   break;
          /* add more as needed */
        }
    }

    XFree(screens);
    XCloseDisplay(dpy);
    return 0;
}
