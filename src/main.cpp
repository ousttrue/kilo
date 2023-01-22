#include "editor.h"
#include "erow.h"
#include "find_mode.h"
#include "syntax_highlight.h"
#include "term_util.h"
#include <memory>
#include <optional>
#include <stack>
#include <stdio.h>

struct Mode {
  std::function<bool(int c)> dispatch;
  std::function<void()> prev = []() {};
};
std::stack<Mode> g_stack;

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
#define KILO_QUIT_TIMES 3
bool editorProcessKeypress(editorConfig &E, int c) {
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
  case CTRL_G: {
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
    else if (c == PAGE_DOWN && E.cy != E.screen.rows - 1)
      E.cy = E.screen.rows - 1;
    {
      int times = E.screen.rows;
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
editorConfig *g_E = nullptr;

void editorAtExit(void) { InputEvent::finalize(); }

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: kilo <filename>\n");
    return 1;
  }

  editorConfig E = {};
  g_E = &E;

  if(!InputEvent::initialize())
  {
    return 2;
  }

  if (auto size = getTermSize()) {
    E.setScreenSize(*size);
  } else {
    perror("Unable to query the screen for size (columns / rows)");
    return 3;
  }

  E.syntax = editorSelectSyntaxHighlight(argv[1]);
  E.editorOpen(argv[1]);
  atexit(editorAtExit);

  E.editorSetStatusMessage(
      "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  g_stack.push({[&E](int c) { return editorProcessKeypress(E, c); }});
  while (!g_stack.empty()) {
    // display
    E.editorRefreshScreen();

    // update
    g_stack.top().prev();

    // dipatch input event
    auto event = InputEvent::get();
    switch (event.type) {
    case InputEventType::Stdin:
      // dipatch key code
      if (!g_stack.top().dispatch(event.byte)) {
        if (g_stack.empty()) {
          break;
        } else {
          g_stack.pop();
        }
      }
      break;

    case InputEventType::Action:
      // dipatch key code
      if (!g_stack.top().dispatch(event.action)) {
        if (g_stack.empty()) {
          break;
        } else {
          g_stack.pop();
        }
      }
      break;

    case InputEventType::Resize:
      if (auto size = getTermSize()) {
        E.setScreenSize(*size);
      }
      break;

    case InputEventType::Error:
      break;
    }
  }

  InputEvent::finalize();
  return 0;
}
