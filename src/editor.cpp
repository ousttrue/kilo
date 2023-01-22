#include "editor.h"
#include "append_buffer.h"
#include "erow.h"
#include "term_util.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h> // open
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct editorConfig E;

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
          if (this->cx > this->screen.cols - 1) {
            this->coloff = this->cx - this->screen.cols + 1;
            this->cx = this->screen.cols - 1;
          }
        }
      }
    } else {
      this->cx -= 1;
    }
    break;
  case ARROW_RIGHT:
    if (row && filecol < row->size) {
      if (this->cx == this->screen.cols - 1) {
        this->coloff++;
      } else {
        this->cx += 1;
      }
    } else if (row && filecol == row->size) {
      this->cx = 0;
      this->coloff = 0;
      if (this->cy == this->screen.rows - 1) {
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
      if (this->cy == this->screen.rows - 1) {
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

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editorConfig::editorRefreshScreen(void) {
  int y;
  erow *r;
  char buf[32];
  abuf ab = {};

  ab.abAppend("\x1b[?25l", 6); /* Hide cursor. */
  ab.abAppend("\x1b[H", 3);    /* Go home. */
  for (y = 0; y < E.screen.rows; y++) {
    int filerow = E.rowoff + y;

    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screen.rows / 3) {
        char welcome[80];
        int welcomelen =
            snprintf(welcome, sizeof(welcome),
                     "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
        int padding = (E.screen.cols - welcomelen) / 2;
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
      if (len > E.screen.cols)
        len = E.screen.cols;
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
  if (len > E.screen.cols)
    len = E.screen.cols;
  ab.abAppend(status, len);
  while (len < E.screen.cols) {
    if (E.screen.cols - len == rlen) {
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
    ab.abAppend(E.statusmsg, msglen <= E.screen.cols ? msglen : E.screen.cols);

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

/* Update the rendered version and the syntax highlight of a row. */
void editorConfig::editorUpdateRow(erow *row) {
  unsigned int tabs = 0, nonprint = 0;
  int j, idx;

  /* Create a version of the row we can directly print on the screen,
   * respecting tabs, substituting non printable characters with '?'. */
  free(row->render);
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == TAB)
      tabs++;

  unsigned long long allocsize =
      (unsigned long long)row->size + tabs * 8 + nonprint * 9 + 1;
  if (allocsize > UINT32_MAX) {
    printf("Some line of the edited file is too long for kilo\n");
    exit(1);
  }

  row->render = (char *)malloc(row->size + tabs * 8 + nonprint * 9 + 1);
  idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == TAB) {
      row->render[idx++] = ' ';
      while ((idx + 1) % 8 != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->rsize = idx;
  row->render[idx] = '\0';

  /* Update the syntax highlighting attributes of the row. */
  editorUpdateSyntax(row);
}

static bool is_separator(int c) {
  return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];", c) != NULL;
}

/* Set every byte of row->hl (that corresponds to every character in the line)
 * to the right syntax highlight type (HL_* defines). */
void editorConfig::editorUpdateSyntax(erow *row) {
  row->hl = (HighLightTypes *)realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (this->syntax == NULL)
    return; /* No syntax, everything is HL_NORMAL. */

  const char **keywords = this->syntax->keywords;
  auto scs = this->syntax->singleline_comment_start;
  auto mcs = this->syntax->multiline_comment_start;
  auto mce = this->syntax->multiline_comment_end;

  /* Point to the first non-space char. */
  auto p = row->render;
  int i = 0; /* Current char offset */
  while (*p && isspace(*p)) {
    p++;
    i++;
  }
  int prev_sep = 1;   /* Tell the parser if 'i' points to start of word. */
  int in_string = 0;  /* Are we inside "" or '' ? */
  int in_comment = 0; /* Are we inside multi-line comment? */

  /* If the previous line has an open comment, this line starts
   * with an open comment state. */
  if (row->idx > 0 && this->row[row->idx - 1].editorRowHasOpenComment())
    in_comment = 1;

  while (*p) {
    /* Handle // comments. */
    if (prev_sep && *p == scs[0] && *(p + 1) == scs[1]) {
      /* From here to end is a comment */
      memset(row->hl + i, HL_COMMENT, row->size - i);
      return;
    }

    /* Handle multi line comments. */
    if (in_comment) {
      row->hl[i] = HL_MLCOMMENT;
      if (*p == mce[0] && *(p + 1) == mce[1]) {
        row->hl[i + 1] = HL_MLCOMMENT;
        p += 2;
        i += 2;
        in_comment = 0;
        prev_sep = 1;
        continue;
      } else {
        prev_sep = 0;
        p++;
        i++;
        continue;
      }
    } else if (*p == mcs[0] && *(p + 1) == mcs[1]) {
      row->hl[i] = HL_MLCOMMENT;
      row->hl[i + 1] = HL_MLCOMMENT;
      p += 2;
      i += 2;
      in_comment = 1;
      prev_sep = 0;
      continue;
    }

    /* Handle "" and '' */
    if (in_string) {
      row->hl[i] = HL_STRING;
      if (*p == '\\') {
        row->hl[i + 1] = HL_STRING;
        p += 2;
        i += 2;
        prev_sep = 0;
        continue;
      }
      if (*p == in_string)
        in_string = 0;
      p++;
      i++;
      continue;
    } else {
      if (*p == '"' || *p == '\'') {
        in_string = *p;
        row->hl[i] = HL_STRING;
        p++;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    /* Handle non printable chars. */
    if (!isprint(*p)) {
      row->hl[i] = HL_NONPRINT;
      p++;
      i++;
      prev_sep = 0;
      continue;
    }

    /* Handle numbers */
    if ((isdigit(*p) && (prev_sep || row->hl[i - 1] == HL_NUMBER)) ||
        (*p == '.' && i > 0 && row->hl[i - 1] == HL_NUMBER)) {
      row->hl[i] = HL_NUMBER;
      p++;
      i++;
      prev_sep = 0;
      continue;
    }

    /* Handle keywords and lib calls */
    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;

        if (!memcmp(p, keywords[j], klen) && is_separator(*(p + klen))) {
          /* Keyword */
          memset(row->hl + i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          p += klen;
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue; /* We had a keyword match */
      }
    }

    /* Not special chars */
    prev_sep = is_separator(*p);
    p++;
    i++;
  }

  /* Propagate syntax change to the next row if the open commen
   * state changed. This may recursively affect all the following rows
   * in the file. */
  auto oc = row->editorRowHasOpenComment();
  if (row->hl_oc != oc && row->idx + 1 < this->numrows)
    editorUpdateSyntax(&this->row[row->idx + 1]);
  row->hl_oc = oc;
}

/* Remove the row at the specified position, shifting the remainign on the
 * top. */
void editorConfig::editorDelRow(int at) {
  if (at >= this->numrows)
    return;
  auto row = this->row + at;
  row->editorFreeRow();
  memmove(this->row + at, this->row + at + 1,
          sizeof(this->row[0]) * (this->numrows - at - 1));
  for (int j = at; j < this->numrows - 1; j++)
    this->row[j].idx++;
  this->numrows--;
  this->dirty++;
}

/* Insert a character at the specified position in a row, moving the remaining
 * chars on the right if needed. */
void editorConfig::editorRowInsertChar(erow *row, int at, int c) {
  if (at > row->size) {
    /* Pad the string with spaces if the insert location is outside the
     * current length by more than a single character. */
    int padlen = at - row->size;
    /* In the next line +2 means: new char and null term. */
    row->chars = (char *)realloc(row->chars, row->size + padlen + 2);
    memset(row->chars + row->size, ' ', padlen);
    row->chars[row->size + padlen + 1] = '\0';
    row->size += padlen + 1;
  } else {
    /* If we are in the middle of the string just make space for 1 new
     * char plus the (already existing) null term. */
    row->chars = (char *)realloc(row->chars, row->size + 2);
    memmove(row->chars + at + 1, row->chars + at, row->size - at + 1);
    row->size++;
  }
  row->chars[at] = c;
  editorUpdateRow(row);
  this->dirty++;
}

/* Append the string 's' at the end of a row */
void editorConfig::editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = (char *)realloc(row->chars, row->size + len + 1);
  memcpy(row->chars + row->size, s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  this->dirty++;
}

/* Delete the character at offset 'at' from the specified row. */
void editorConfig::editorRowDelChar(erow *row, int at) {
  if (row->size <= at)
    return;
  memmove(row->chars + at, row->chars + at + 1, row->size - at);
  editorUpdateRow(row);
  row->size--;
  this->dirty++;
}

/* Insert the specified char at the current prompt position. */
void editorConfig::editorInsertChar(int c) {
  int filerow = this->rowoff + this->cy;
  int filecol = this->coloff + this->cx;
  erow *row = (filerow >= this->numrows) ? NULL : &this->row[filerow];

  /* If the row where the cursor is currently located does not exist in our
   * logical representaion of the file, add enough empty rows as needed. */
  if (!row) {
    while (this->numrows <= filerow)
      editorInsertRow(this->numrows, "", 0);
  }
  row = &this->row[filerow];
  editorRowInsertChar(row, filecol, c);
  if (this->cx == this->screen.cols - 1)
    this->coloff++;
  else
    this->cx++;
  this->dirty++;
}

/* Inserting a newline is slightly complex as we have to handle inserting a
 * newline in the middle of a line, splitting the line as needed. */
void editorConfig::editorInsertNewline(void) {
  int filerow = this->rowoff + this->cy;
  int filecol = this->coloff + this->cx;
  erow *row = (filerow >= this->numrows) ? NULL : &this->row[filerow];

  if (!row) {
    if (filerow == this->numrows) {
      editorInsertRow(filerow, "", 0);
      goto fixcursor;
    }
    return;
  }
  /* If the cursor is over the current line size, we want to conceptually
   * think it's just over the last character. */
  if (filecol >= row->size)
    filecol = row->size;
  if (filecol == 0) {
    editorInsertRow(filerow, "", 0);
  } else {
    /* We are in the middle of a line. Split it between two rows. */
    editorInsertRow(filerow + 1, row->chars + filecol, row->size - filecol);
    row = &this->row[filerow];
    row->chars[filecol] = '\0';
    row->size = filecol;
    editorUpdateRow(row);
  }
fixcursor:
  if (this->cy == this->screen.rows - 1) {
    this->rowoff++;
  } else {
    this->cy++;
  }
  this->cx = 0;
  this->coloff = 0;
}

/* Delete the char at the current prompt position. */
void editorConfig::editorDelChar() {
  int filerow = this->rowoff + this->cy;
  int filecol = this->coloff + this->cx;
  erow *row = (filerow >= this->numrows) ? NULL : &this->row[filerow];

  if (!row || (filecol == 0 && filerow == 0))
    return;
  if (filecol == 0) {
    /* Handle the case of column 0, we need to move the current line
     * on the right of the previous one. */
    filecol = this->row[filerow - 1].size;
    editorRowAppendString(&this->row[filerow - 1], row->chars, row->size);
    editorDelRow(filerow);
    row = NULL;
    if (this->cy == 0)
      this->rowoff--;
    else
      this->cy--;
    this->cx = filecol;
    if (this->cx >= this->screen.cols) {
      int shift = (this->screen.cols - this->cx) + 1;
      this->cx -= shift;
      this->coloff += shift;
    }
  } else {
    editorRowDelChar(row, filecol - 1);
    if (this->cx == 0 && this->coloff)
      this->coloff--;
    else
      this->cx--;
  }
  if (row)
    editorUpdateRow(row);
  this->dirty++;
}

/* Turn the editor rows into a single heap-allocated string.
 * Returns the pointer to the heap-allocated string and populate the
 * integer pointed by 'buflen' with the size of the string, escluding
 * the final nulterm. */
char *editorConfig::editorRowsToString(int *buflen) {
  char *buf = NULL, *p;
  int totlen = 0;
  int j;

  /* Compute count of bytes */
  for (j = 0; j < this->numrows; j++)
    totlen += this->row[j].size + 1; /* +1 is for "\n" at end of every row */
  *buflen = totlen;
  totlen++; /* Also make space for nulterm */

  p = buf = (char *)malloc(totlen);
  for (j = 0; j < this->numrows; j++) {
    memcpy(p, this->row[j].chars, this->row[j].size);
    p += this->row[j].size;
    *p = '\n';
    p++;
  }
  *p = '\0';
  return buf;
}

/* Insert a row at the specified position, shifting the other rows on the bottom
 * if required. */
void editorConfig::editorInsertRow(int at, const char *s, size_t len) {
  if (at > this->numrows)
    return;
  this->row = (erow *)realloc(this->row, sizeof(erow) * (this->numrows + 1));
  if (at != this->numrows) {
    memmove(this->row + at + 1, this->row + at,
            sizeof(this->row[0]) * (this->numrows - at));
    for (int j = at + 1; j <= this->numrows; j++)
      this->row[j].idx++;
  }
  this->row[at].size = len;
  this->row[at].chars = (char *)malloc(len + 1);
  memcpy(this->row[at].chars, s, len + 1);
  this->row[at].hl = NULL;
  this->row[at].hl_oc = 0;
  this->row[at].render = NULL;
  this->row[at].rsize = 0;
  this->row[at].idx = at;
  editorUpdateRow(this->row + at);
  this->numrows++;
  this->dirty++;
}

/* Save the current file on disk. Return 0 on success, 1 on error. */
int editorConfig::editorSave(void) {
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(this->filename, O_RDWR | O_CREAT, 0644);
  if (fd == -1)
    goto writeerr;

  /* Use truncate + a single write(2) call in order to make saving
   * a bit safer, under the limits of what we can do in a small editor. */
  if (ftruncate(fd, len) == -1)
    goto writeerr;
  if (write(fd, buf, len) != len)
    goto writeerr;

  close(fd);
  free(buf);
  this->dirty = 0;
  this->editorSetStatusMessage("%d bytes written on disk", len);
  return 0;

writeerr:
  free(buf);
  if (fd != -1)
    close(fd);
  this->editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
  return 1;
}

/* Load the specified program in the editor memory and returns 0 on success
 * or 1 on error. */
int editorConfig::editorOpen(const char *filename) {
  this->dirty = 0;
  free(this->filename);
  size_t fnlen = strlen(filename) + 1;
  this->filename = (char *)malloc(fnlen);
  memcpy(this->filename, filename, fnlen);

  auto fp = fopen(filename, "r");
  if (!fp) {
    if (errno != ENOENT) {
      perror("Opening file");
      exit(1);
    }
    return 1;
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    if (linelen && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      line[--linelen] = '\0';
    editorInsertRow(this->numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  this->dirty = 0;
  return 0;
}
