#include "editor.h"
#include "kilo.h"
#include "get_termsize.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct editorConfig E;


static void updateWindowSize(void) {
  if (getTermSize(STDIN_FILENO, STDOUT_FILENO, &E.screenrows,
                    &E.screencols) == -1) {
    perror("Unable to query the screen for size (columns / rows)");
    exit(1);
  }
  E.screenrows -= 2; /* Get room for status bar. */
}

static void handleSigWinCh(int unused __attribute__((unused))) {
  updateWindowSize();
  if (E.cy > E.screenrows)
    E.cy = E.screenrows - 1;
  if (E.cx > E.screencols)
    E.cx = E.screencols - 1;
  editorRefreshScreen();
}

void editorConfig::init() {
  this->cx = 0;
  this->cy = 0;
  this->rowoff = 0;
  this->coloff = 0;
  this->numrows = 0;
  this->row = NULL;
  this->dirty = 0;
  this->filename = NULL;
  this->syntax = NULL;
  updateWindowSize();
  signal(SIGWINCH, handleSigWinCh);
}

/* ========================= Editor events handling  ======================== */

/* Handle cursor position change because arrow keys were pressed. */
void editorConfig::editorMoveCursor(int key) {
  int filerow = this->rowoff + this->cy;
  int filecol = this->coloff + this->cx;
  int rowlen;
  erow *row = (filerow >= this->numrows) ? NULL : &this->row[filerow];

  switch (key) {
  case ARROW_LEFT:
    if (this->cx == 0) {
      if (this->coloff) {
        this->coloff--;
      } else {
        if (filerow > 0) {
          this->cy--;
          this->cx = this->row[filerow - 1].size;
          if (this->cx > this->screencols - 1) {
            this->coloff = this->cx - this->screencols + 1;
            this->cx = this->screencols - 1;
          }
        }
      }
    } else {
      this->cx -= 1;
    }
    break;
  case ARROW_RIGHT:
    if (row && filecol < row->size) {
      if (this->cx == this->screencols - 1) {
        this->coloff++;
      } else {
        this->cx += 1;
      }
    } else if (row && filecol == row->size) {
      this->cx = 0;
      this->coloff = 0;
      if (this->cy == this->screenrows - 1) {
        this->rowoff++;
      } else {
        this->cy += 1;
      }
    }
    break;
  case ARROW_UP:
    if (this->cy == 0) {
      if (this->rowoff)
        this->rowoff--;
    } else {
      this->cy -= 1;
    }
    break;
  case ARROW_DOWN:
    if (filerow < this->numrows) {
      if (this->cy == this->screenrows - 1) {
        this->rowoff++;
      } else {
        this->cy += 1;
      }
    }
    break;
  }
  /* Fix cx if the current line has not enough chars. */
  filerow = this->rowoff + this->cy;
  filecol = this->coloff + this->cx;
  row = (filerow >= this->numrows) ? NULL : &this->row[filerow];
  rowlen = row ? row->size : 0;
  if (filecol > rowlen) {
    this->cx -= filecol - rowlen;
    if (this->cx < 0) {
      this->coloff += this->cx;
      this->cx = 0;
    }
  }
}

/* =============================== Find mode ================================ */

#define KILO_QUERY_LEN 256

void editorConfig::editorFind(int fd) {
  char query[KILO_QUERY_LEN + 1] = {0};
  int qlen = 0;
  int last_match = -1;    /* Last line where a match was found. -1 for none. */
  int find_next = 0;      /* if 1 search next, if -1 search prev. */
  int saved_hl_line = -1; /* No saved HL */
  char *saved_hl = NULL;

#define FIND_RESTORE_HL                                                        \
  do {                                                                         \
    if (saved_hl) {                                                            \
      memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);   \
      free(saved_hl);                                                          \
      saved_hl = NULL;                                                         \
    }                                                                          \
  } while (0)

  /* Save the cursor position in order to restore it later. */
  int saved_cx = E.cx, saved_cy = E.cy;
  int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

  while (1) {
    editorSetStatusMessage("Search: %s (Use ESC/Arrows/Enter)", query);
    editorRefreshScreen();

    int c = editorReadKey(fd);
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
      FIND_RESTORE_HL;
      editorSetStatusMessage("");
      return;
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
      FIND_RESTORE_HL;

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
        if (E.cx > E.screencols) {
          int diff = E.cx - E.screencols;
          E.cx -= diff;
          E.coloff += diff;
        }
      }
    }
  }
}
