#include "editor.h"
#include "kilo.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // STDIN_FILENO

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
#define KILO_QUIT_TIMES 3
void editorProcessKeypress(int fd) {
  /* When the file is modified, requires Ctrl-q to be pressed N times
   * before actually quitting. */
  static int quit_times = KILO_QUIT_TIMES;

  int c = editorReadKey(fd);
  switch (c) {
  case ENTER: /* Enter */
    E.editorInsertNewline();
    break;
  case CTRL_C: /* Ctrl-c */
    /* We ignore ctrl-c, it can't be so simple to lose the changes
     * to the edited file. */
    break;
  case CTRL_Q: /* Ctrl-q */
    /* Quit if the file was already saved. */
    if (E.dirty && quit_times) {
      E.editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                             "Press Ctrl-Q %d more times to quit.",
                             quit_times);
      quit_times--;
      return;
    }
    exit(0);
    break;
  case CTRL_S: /* Ctrl-s */
    E.editorSave();
    break;
  case CTRL_F:
    E.editorFind(fd);
    break;
  case BACKSPACE: /* Backspace */
  case CTRL_H:    /* Ctrl-h */
  case DEL_KEY:
    E.editorDelChar();
    break;
  case PAGE_UP:
  case PAGE_DOWN:
    if (c == PAGE_UP && E.cy != 0)
      E.cy = 0;
    else if (c == PAGE_DOWN && E.cy != E.screenrows - 1)
      E.cy = E.screenrows - 1;
    {
      int times = E.screenrows;
      while (times--)
        E.editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    E.editorMoveCursor(c);
    break;
  case CTRL_L: /* ctrl+l, clear screen */
    /* Just refresht the line as side effect. */
    break;
  case ESC:
    /* Nothing to do for ESC in this mode. */
    break;
  default:
    E.editorInsertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES; /* Reset it to the original value. */
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: kilo <filename>\n");
    return 1;
  }

  E.init();
  E.editorSelectSyntaxHighlight(argv[1]);
  E.editorOpen(argv[1]);
  enableRawMode(STDIN_FILENO);
  E.editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
  while (1) {
    E.editorRefreshScreen();
    editorProcessKeypress(STDIN_FILENO);
  }
  return 0;
}
