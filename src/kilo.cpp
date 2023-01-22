/* Kilo -- A very simple editor in less than 1-kilo lines of code (as counted
 *         by "cloc"). Does not depend on libcurses, directly emits VT100
 *         escapes on the terminal.
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#include "kilo.h"
#include "editor.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

typedef struct hlcolor {
  int r, g, b;
} hlcolor;

/* =========================== Syntax highlights DB =========================
 *
 * In order to add a new syntax, define two arrays with a list of file name
 * matches and keywords. The file name matches are used in order to match
 * a given syntax with a given file name: if a match pattern starts with a
 * dot, it is matched as the last past of the filename, for example ".c".
 * Otherwise the pattern is just searched inside the filenme, like "Makefile").
 *
 * The list of keywords to highlight is just a list of words, however if they
 * a trailing '|' character is added at the end, they are highlighted in
 * a different color, so that you can have two different sets of keywords.
 *
 * Finally add a stanza in the HLDB global variable with two two arrays
 * of strings, and a set of flags in order to enable highlighting of
 * comments and numbers.
 *
 * The characters for single and multi line comments must be exactly two
 * and must be provided as well (see the C language example).
 *
 * There is no support to highlight patterns currently. */



/* ======================= Low level terminal handling ====================== */

static struct termios orig_termios; /* In order to restore at exit.*/

void disableRawMode(int fd) {
  /* Don't even check the return value as it's too late. */
  if (E.rawmode) {
    tcsetattr(fd, TCSAFLUSH, &orig_termios);
    E.rawmode = 0;
  }
}

/* Called at exit to avoid remaining in raw mode. */
void editorAtExit(void) { disableRawMode(STDIN_FILENO); }

/* Raw mode: 1960 magic shit. */
int enableRawMode(int fd) {
  struct termios raw;

  if (E.rawmode)
    return 0; /* Already enabled. */
  if (!isatty(STDIN_FILENO))
    goto fatal;
  atexit(editorAtExit);
  if (tcgetattr(fd, &orig_termios) == -1)
    goto fatal;

  raw = orig_termios; /* modify the original mode */
  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes - disable post processing */
  raw.c_oflag &= ~(OPOST);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - choing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer. */
  raw.c_cc[VMIN] = 0;  /* Return each byte, or zero for timeout. */
  raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

  /* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
    goto fatal;
  E.rawmode = 1;
  return 0;

fatal:
  errno = ENOTTY;
  return -1;
}

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int editorReadKey(int fd) {
  int nread;
  char c, seq[3];
  while ((nread = read(fd, &c, 1)) == 0)
    ;
  if (nread == -1)
    exit(1);

  while (1) {
    switch (c) {
    case ESC: /* escape sequence */
      /* If this is just an ESC, we'll timeout here. */
      if (read(fd, seq, 1) == 0)
        return ESC;
      if (read(fd, seq + 1, 1) == 0)
        return ESC;

      /* ESC [ sequences. */
      if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
          /* Extended escape, read additional byte. */
          if (read(fd, seq + 2, 1) == 0)
            return ESC;
          if (seq[2] == '~') {
            switch (seq[1]) {
            case '3':
              return DEL_KEY;
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
            }
          }
        } else {
          switch (seq[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
          }
        }
      }

      /* ESC O sequences. */
      else if (seq[0] == 'O') {
        switch (seq[1]) {
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
      break;
    default:
      return c;
    }
  }
}
