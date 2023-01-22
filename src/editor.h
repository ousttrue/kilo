#pragma once
#include "kilo.h"
#include <time.h>

struct editorSyntax {
  const char **filematch;
  const char **keywords;
  char singleline_comment_start[2];
  char multiline_comment_start[3];
  char multiline_comment_end[3];
  int flags;
};

struct editorConfig {
  int cx, cy;     /* Cursor x and y position in characters */
  int rowoff;     /* Offset of row displayed. */
  int coloff;     /* Offset of column displayed. */
  int screenrows; /* Number of rows that we can show */
  int screencols; /* Number of cols that we can show */
  int numrows;    /* Number of rows */
  int rawmode;    /* Is terminal raw mode enabled? */
  struct erow *row;      /* Rows */
  int dirty = 0;  /* File modified but not saved. */
  char *filename; /* Currently open filename */
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
  void editorFind(int fd);
  int editorSave(void);
  int editorOpen(const char *filename);
};

extern struct editorConfig E;
