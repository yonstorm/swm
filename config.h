#define BORDER_WIDTH 2
#define FOCUSED_BORDER_COLOR   "#E89AFB"
#define UNFOCUSED_BORDER_COLOR "#FFFFFF"

// Window rules
static const struct {
    const char *class;
    int monitor; // -1 for any
} rules[] = {
    /* class      monitor */
    { "discord",      2 },
};
