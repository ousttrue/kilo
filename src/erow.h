#pragma once
#include <stdint.h>

/* Syntax highlight types */
enum HighLightTypes : uint8_t {
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

/* This structure represents a single line of the file we are editing. */
struct erow {
  int idx;            /* Row index in the file, zero-based. */
  int size;           /* Size of the row, excluding the null term. */
  int rsize;          /* Size of the rendered row. */
  char *chars;        /* Row content. */
  char *render;       /* Row content "rendered" for screen (for TABs). */
  HighLightTypes *hl; /* Syntax highlight type for each character in render.*/
  int hl_oc;          /* Row had open comment at end in last syntax highlight
                         check. */

  void editorFreeRow();
  int editorRowHasOpenComment() const;
};
