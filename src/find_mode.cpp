#include "find_mode.h"
#include "editor.h"
#include "erow.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

FindMode::FindMode(editorConfig &E) : E(E) {
  /* Save the cursor position in order to restore it later. */
  saved_cx = E.cx;
  saved_cy = E.cy;
  saved_coloff = E.coloff;
  saved_rowoff = E.rowoff;
}

void FindMode::prev() {
  E.editorSetStatusMessage("Search: %s (Use ESC/Arrows/Enter)", query);
  E.editorRefreshScreen();
}

bool FindMode::dispatch(int c) {
  // int c = readKey(fd);
  if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
    if (qlen != 0)
      query[--qlen] = '\0';
    last_match = -1;
  } else if (c == ESC || c == ENTER) {
    if (c == ESC) {
      E.cx = saved_cx;
      E.cy = saved_cy;
      E.coloff = saved_coloff;
      E.rowoff = saved_rowoff;
    }
    FIND_RESTORE_HL();
    E.editorSetStatusMessage("");
    return false;
    // g_stack.pop();
    // return;
  } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
    find_next = 1;
  } else if (c == ARROW_LEFT || c == ARROW_UP) {
    find_next = -1;
  } else if (isprint(c)) {
    if (qlen < KILO_QUERY_LEN) {
      query[qlen++] = c;
      query[qlen] = '\0';
      last_match = -1;
    }
  }

  /* Search occurrence. */
  if (last_match == -1)
    find_next = 1;
  if (find_next) {
    char *match = NULL;
    int match_offset = 0;
    int i, current = last_match;

    for (i = 0; i < E.numrows; i++) {
      current += find_next;
      if (current == -1)
        current = E.numrows - 1;
      else if (current == E.numrows)
        current = 0;
      match = strstr(E.row[current].render, query);
      if (match) {
        match_offset = match - E.row[current].render;
        break;
      }
    }
    find_next = 0;

    /* Highlight */
    FIND_RESTORE_HL();

    if (match) {
      erow *row = &E.row[current];
      last_match = current;
      if (row->hl) {
        saved_hl_line = current;
        saved_hl = (char *)malloc(row->rsize);
        memcpy(saved_hl, row->hl, row->rsize);
        memset(row->hl + match_offset, HL_MATCH, qlen);
      }
      E.cy = 0;
      E.cx = match_offset;
      E.rowoff = current;
      E.coloff = 0;
      /* Scroll horizontally as needed. */
      if (E.cx > E.screen.cols) {
        int diff = E.cx - E.screen.cols;
        E.cx -= diff;
        E.coloff += diff;
      }
    }
  }
  return true;
}

void FindMode::FIND_RESTORE_HL() {
  do {
    if (saved_hl) {
      memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
      free(saved_hl);
      saved_hl = NULL;
    }
  } while (0);
}
