#include "editor.h"
#include "kilo.h"
#include "get_termsize.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

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
