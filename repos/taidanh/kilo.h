#pragma once
#include <time.h>
#include "../_mod/platform.h"

enum editorMode {
    MODE_INSERT = 0,
    MODE_NORMAL
};

struct editorConfig {
    int cx, cy; /* Cursor x and y position in characters */
    int rowoff; /* Offset of row displayed. */
    int coloff; /* Offset of column displayed. */
    int screenrows; /* Number of rows that we can show */
    int screencols; /* Number of cols that we can show */
    int numrows; /* Number of rows */
    int rawmode; /* Is terminal raw mode enabled? */
    erow *row; /* Rows */
    int dirty; /* File modified but not saved. */
    int mode;
    int rx;
    char *filename; /* Currently open filename */
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax; /* Current syntax highlight, or NULL. */
};

extern struct editorConfig E;

void initEditor();
void editorOpen(char *filename);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
void editorNormalProcessKeypress();
void editorProcessKeypress();
