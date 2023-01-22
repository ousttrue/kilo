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

struct editorSyntax {
  const char **filematch;
  const char **keywords;
  char singleline_comment_start[2];
  char multiline_comment_start[3];
  char multiline_comment_end[3];
  int flags;
};

const editorSyntax *editorSelectSyntaxHighlight(const char *filename);
