#pragma once
#include <functional>
#include <time.h>

#define KILO_VERSION "0.0.1"

enum KEY_ACTION {
  KEY_NULL = 0,    /* NULL */
  CTRL_C = 3,      /* Ctrl-c */
  CTRL_D = 4,      /* Ctrl-d */
  CTRL_F = 6,      /* Ctrl-f */
  CTRL_H = 8,      /* Ctrl-h */
  TAB = 9,         /* Tab */
  CTRL_L = 12,     /* Ctrl+l */
  ENTER = 13,      /* Enter */
  CTRL_Q = 17,     /* Ctrl-q */
  CTRL_S = 19,     /* Ctrl-s */
  CTRL_U = 21,     /* Ctrl-u */
  CTRL_X = 24,     /* Ctrl-q */
  ESC = 27,        /* Escape */
  BACKSPACE = 127, /* Backspace */
  /* The following are just soft codes, not really reported by the
   * terminal directly. */
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

struct editorSyntax {
  const char **filematch;
  const char **keywords;
  char singleline_comment_start[2];
  char multiline_comment_start[3];
  char multiline_comment_end[3];
  int flags;
};

struct editorConfig {
  int cx, cy;       /* Cursor x and y position in characters */
  int rowoff;       /* Offset of row displayed. */
  int coloff;       /* Offset of column displayed. */
  int screenrows;   /* Number of rows that we can show */
  int screencols;   /* Number of cols that we can show */
  int numrows;      /* Number of rows */
  int rawmode;      /* Is terminal raw mode enabled? */
  struct erow *row; /* Rows */
  int dirty = 0;    /* File modified but not saved. */
  char *filename;   /* Currently open filename */
  char statusmsg[80];
  time_t statusmsg_time;
  editorSyntax *syntax; /* Current syntax highlight, or NULL. */

  void init();

  void editorUpdateRow(erow *row);
  void editorInsertRow(int at, const char *s, size_t len);
  char *editorRowsToString(int *buflen);
  void editorUpdateSyntax(erow *row);
  void editorDelRow(int at);
  void editorRowInsertChar(erow *row, int at, int c);
  void editorRowAppendString(erow *row, char *s, size_t len);
  void editorRowDelChar(erow *row, int at);
  void editorInsertChar(int c);
  void editorInsertNewline(void);
  void editorDelChar();

  void editorSelectSyntaxHighlight(const char *filename);
  void editorMoveCursor(int key);
  void editorSetStatusMessage(const char *fmt, ...);
  void editorRefreshScreen(void);
  void editorFind(int fd, const std::function<int(int fd)> &readKey);
  int editorSave(void);
  int editorOpen(const char *filename);
};

extern struct editorConfig E;
