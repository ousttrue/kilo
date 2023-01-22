#pragma once

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

/* Syntax highlight types */
enum HighLightTypes {
  HL_NORMAL = 0,
  HL_NONPRINT = 1,
  HL_COMMENT = 2,   /* Single line comment. */
  HL_MLCOMMENT = 3, /* Multi-line comment. */
  HL_KEYWORD1 = 4,
  HL_KEYWORD2 = 5,
  HL_STRING = 6,
  HL_NUMBER = 7,
  HL_MATCH = 8, /* Search match. */
};

void editorSelectSyntaxHighlight(char *filename);
int editorOpen(char *filename);
int enableRawMode(int fd);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
void editorProcessKeypress(int fd);
int editorReadKey(int fd);
void editorInsertNewline(void);
int editorSave(void);
void editorDelChar();
void editorInsertChar(int c);
