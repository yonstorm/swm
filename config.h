#ifndef CONFIG_H
#define CONFIG_H

/* Appearance */
#define BORDER_WIDTH        2
#define FOCUS_COLOR         0x005577  /* Blue border for focused window */
#define UNFOCUS_COLOR       0x444444  /* Gray border for unfocused windows */

/* Focus behavior */
#define FOCUS_FOLLOWS_MOUSE 1         /* 1 to enable, 0 to disable */

/* Monitor configuration */
#define ULTRAWIDE_THRESHOLD 5000      /* Pixels width to consider ultrawide */
#define ZONE_LEFT_RATIO     0.25      /* Left zone: 1/4 of ultrawide */
#define ZONE_CENTER_RATIO   0.50      /* Center zone: 1/2 of ultrawide */
#define ZONE_RIGHT_RATIO    0.25      /* Right zone: 1/4 of ultrawide */

/* External command interface */
#define COMMAND_PROPERTY    "_SWM_COMMAND"

/* Commands that can be sent via root window property */
enum swm_command {
    CMD_CYCLE_WINDOW = 1,
    CMD_CYCLE_MONITOR,
    CMD_CYCLE_WINDOW_NEXT,
    CMD_CYCLE_WINDOW_PREV,
    CMD_CYCLE_MONITOR_LEFT,
    CMD_CYCLE_MONITOR_RIGHT,
    CMD_KILL_WINDOW,
    CMD_QUIT,
    CMD_MOVE_WINDOW_LEFT,
    CMD_MOVE_WINDOW_RIGHT
};

#endif /* CONFIG_H */ 