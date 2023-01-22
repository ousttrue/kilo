#pragma once
#include <stdint.h>
#include "syntax_highlight.h"

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
