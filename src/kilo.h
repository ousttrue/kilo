#pragma once
#define KILO_VERSION "0.0.1"
#include <stdint.h>

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
enum HighLightTypes: uint8_t {
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
/* Maps syntax highlight token types to terminal colors. */
inline int editorSyntaxToColor(HighLightTypes hl) {
  switch (hl) {
  case HL_COMMENT:
  case HL_MLCOMMENT:
    return 36; /* cyan */
  case HL_KEYWORD1:
    return 33; /* yellow */
  case HL_KEYWORD2:
    return 32; /* green */
  case HL_STRING:
    return 35; /* magenta */
  case HL_NUMBER:
    return 31; /* red */
  case HL_MATCH:
    return 34; /* blu */
  default:
    return 37; /* white */
  }
}

void editorSelectSyntaxHighlight(char *filename);
int editorOpen(char *filename);
int enableRawMode(int fd);
void editorProcessKeypress(int fd);
int editorReadKey(int fd);
void editorInsertNewline(void);
void editorDelChar();
void editorInsertChar(int c);
