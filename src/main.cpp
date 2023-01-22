#include "editor.h"
#include "erow.h"
#include "find_mode.h"
#include "syntax_highlight.h"
#include "term_util.h"
#include <memory>
#include <optional>
#include <stack>
#include <stdio.h>
#include <unistd.h> // STDIN_FILENO

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
std::optional<int> editorReadKey(int fd) {
  int nread;
  char c, seq[3];
  while ((nread = read(fd, &c, 1)) == 0)
    ;
  if (nread == -1) {
    return {};
  }

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

struct Mode {
  std::function<bool(int c)> dispatch;
  std::function<void()> prev = []() {};
};
std::stack<Mode> g_stack;

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
#define KILO_QUIT_TIMES 3
bool editorProcessKeypress(int c) {
  /* When the file is modified, requires Ctrl-q to be pressed N times
   * before actually quitting. */
  static int quit_times = KILO_QUIT_TIMES;

  switch (c) {
  case ENTER: /* Enter */
    E.editorInsertNewline();
    break;
  case CTRL_C: /* Ctrl-c */
    /* We ignore ctrl-c, it can't be so simple to lose the changes
     * to the edited file. */
    break;
  case CTRL_Q: /* Ctrl-q */
  case CTRL_X: /* Ctrl-q */
    /* Quit if the file was already saved. */
    if (E.dirty && quit_times) {
      E.editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                               "Press Ctrl-Q %d more times to quit.",
                               quit_times);
      quit_times--;
    } else {
      g_stack.pop();
    }
    break;
  case CTRL_S: /* Ctrl-s */
    E.editorSave();
    break;
  case CTRL_F:
  case CTRL_G:
   {
    auto find = std::make_shared<FindMode>(E);
    g_stack.push(Mode{
        .dispatch = [find](int c) { return find->dispatch(c); },
        .prev = [find]() { find->prev(); },
    });
  } break;
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
  return true;
}

/* Called at exit to avoid remaining in raw mode. */
void editorAtExit(void) {
  if (E.rawmode) {
    disableRawMode(STDIN_FILENO);
    E.rawmode = 0;
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: kilo <filename>\n");
    return 1;
  }

  E.init();
  E.syntax = editorSelectSyntaxHighlight(argv[1]);
  E.editorOpen(argv[1]);
  enableRawMode(STDIN_FILENO);
  E.rawmode = 1;
  // atexit(editorAtExit);

  E.editorSetStatusMessage(
      "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  g_stack.push({&editorProcessKeypress});
  while (1) {
    E.editorRefreshScreen();

    g_stack.top().prev();
    if (auto c = editorReadKey(STDIN_FILENO)) {
      if (!g_stack.top().dispatch(*c)) {
        if (g_stack.empty()) {
          break;
        } else {
          g_stack.pop();
        }
      }
    } else {
      break;
    }
  }

  disableRawMode(STDIN_FILENO);
  return 0;
}
