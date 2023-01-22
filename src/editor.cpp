#include "editor.h"
#include "append_buffer.h"
#include "get_termsize.h"
#include "kilo.h"
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct editorConfig E;

static void updateWindowSize(void) {
  if (getTermSize(STDIN_FILENO, STDOUT_FILENO, &E.screenrows, &E.screencols) ==
      -1) {
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
  E.editorRefreshScreen();
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

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editorConfig::editorRefreshScreen(void) {
  int y;
  erow *r;
  char buf[32];
  abuf ab = {};

  ab.abAppend("\x1b[?25l", 6); /* Hide cursor. */
  ab.abAppend("\x1b[H", 3);    /* Go home. */
  for (y = 0; y < E.screenrows; y++) {
    int filerow = E.rowoff + y;

    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen =
            snprintf(welcome, sizeof(welcome),
                     "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          ab.abAppend("~", 1);
          padding--;
        }
        while (padding--)
          ab.abAppend(" ", 1);
        ab.abAppend(welcome, welcomelen);
      } else {
        ab.abAppend("~\x1b[0K\r\n", 7);
      }
      continue;
    }

    r = &E.row[filerow];

    int len = r->rsize - E.coloff;
    int current_color = -1;
    if (len > 0) {
      if (len > E.screencols)
        len = E.screencols;
      char *c = r->render + E.coloff;
      auto hl = r->hl + E.coloff;
      int j;
      for (j = 0; j < len; j++) {
        if (hl[j] == HL_NONPRINT) {
          char sym;
          ab.abAppend("\x1b[7m", 4);
          if (c[j] <= 26)
            sym = '@' + c[j];
          else
            sym = '?';
          ab.abAppend(&sym, 1);
          ab.abAppend("\x1b[0m", 4);
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            ab.abAppend("\x1b[39m", 5);
            current_color = -1;
          }
          ab.abAppend(c + j, 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            current_color = color;
            ab.abAppend(buf, clen);
          }
          ab.abAppend(c + j, 1);
        }
      }
    }
    ab.abAppend("\x1b[39m", 5);
    ab.abAppend("\x1b[0K", 4);
    ab.abAppend("\r\n", 2);
  }

  /* Create a two rows status. First row: */
  ab.abAppend("\x1b[0K", 4);
  ab.abAppend("\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename,
                     E.numrows, E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.rowoff + E.cy + 1,
                      E.numrows);
  if (len > E.screencols)
    len = E.screencols;
  ab.abAppend(status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      ab.abAppend(rstatus, rlen);
      break;
    } else {
      ab.abAppend(" ", 1);
      len++;
    }
  }
  ab.abAppend("\x1b[0m\r\n", 6);

  /* Second row depends on E.statusmsg and the status message update time. */
  ab.abAppend("\x1b[0K", 4);
  int msglen = strlen(E.statusmsg);
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    ab.abAppend(E.statusmsg, msglen <= E.screencols ? msglen : E.screencols);

  /* Put cursor at its current position. Note that the horizontal position
   * at which the cursor is displayed may be different compared to 'E.cx'
   * because of TABs. */
  int j;
  int cx = 1;
  int filerow = E.rowoff + E.cy;
  erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
  if (row) {
    for (j = E.coloff; j < (E.cx + E.coloff); j++) {
      if (j < row->size && row->chars[j] == TAB)
        cx += 7 - ((cx) % 8);
      cx++;
    }
  }
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, cx);
  ab.abAppend(buf, strlen(buf));
  ab.abAppend("\x1b[?25h", 6); /* Show cursor. */
  write(STDOUT_FILENO, ab.b, ab.len);
  ab.abFree();
}

/* Set an editor status message for the second line of the status, at the
 * end of the screen. */
void editorConfig::editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(this->statusmsg, sizeof(this->statusmsg), fmt, ap);
  va_end(ap);
  this->statusmsg_time = time(NULL);
}
