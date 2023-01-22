#pragma once
#include "term_util.h"
#include <functional>
#include <string>
#include <time.h>

#define KILO_VERSION "0.0.1"

enum KEY_ACTION {
  KEY_NULL = 0,    /* NULL */
  CTRL_C = 3,      /* Ctrl-c */
  CTRL_D = 4,      /* Ctrl-d */
  CTRL_F = 6,      /* Ctrl-f */
  CTRL_G = 7,      /* Ctrl-f */
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

struct editorConfig {
  // Cursor x and y position in characters
  int cx = 0;
  int cy = 0;
  // Offset of row displayed.
  int rowoff = 0;
  // Offset of column displayed.
  int coloff = 0;
  TermSize screen;
  // Number of rows
  int numrows = 0;
  // Is terminal raw mode enabled?
  bool rawmode = false;
  // Rows
  struct erow *row = 0;
  // File modified but not saved.
  int dirty = 0;
  // Currently open filename
  std::string filename;
  char statusmsg[80] = {0};
  time_t statusmsg_time = {};
  // Current syntax highlight, or NULL.
  const struct editorSyntax *syntax = nullptr;

  void setScreenSize(const TermSize &size) {
    screen = size;
    screen.rows -= 2; /* Get room for status bar. */
    if (cy > screen.rows)
      cy = screen.rows - 1;
    if (cx > screen.cols)
      cx = screen.cols - 1;
  }

  void editorUpdateRow(erow *row);
  void editorInsertRow(int at, const char *s, size_t len);
  std::string editorRowsToString() const;
  void editorUpdateSyntax(erow *row);
  void editorDelRow(int at);
  void editorRowInsertChar(erow *row, int at, int c);
  void editorRowAppendString(erow *row, char *s, size_t len);
  void editorRowDelChar(erow *row, int at);
  void editorInsertChar(int c);
  void editorInsertNewline();
  void editorDelChar();

  void editorMoveCursor(int key);
  void editorSetStatusMessage(const char *fmt, ...);
  void editorRefreshScreen();
  void editorFind(int c);
  bool editorSave();
  bool editorOpen(const char *filename);
};