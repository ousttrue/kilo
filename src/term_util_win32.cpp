#ifdef _WIN32
#include "term_util.h"
#include <Windows.h>
#include <queue>

HANDLE g_stdin = {};
HANDLE g_stdout = {};
DWORD g_fdwSaveOldMode;
EscapeSequence g_seq;

std::optional<TermSize> getTermSize() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(g_stdout, &csbi);
  return TermSize{
      .rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1,
      .cols = csbi.srWindow.Right - csbi.srWindow.Left + 1,
  };
}

bool InputEvent::initialize() {
  g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (g_stdout == INVALID_HANDLE_VALUE) {
    return false;
  }

  g_stdin = GetStdHandle(STD_INPUT_HANDLE);
  if (g_stdin == INVALID_HANDLE_VALUE) {
    return false;
  }

  // Save the current input mode, to be restored on exit.
  if (!GetConsoleMode(g_stdin, &g_fdwSaveOldMode)) {
    return false;
    ;
  }

  // Enable the window and mouse input events.
  auto fdwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
  if (!SetConsoleMode(g_stdin, fdwMode)) {
    return false;
  }

  DWORD dwOriginalOutMode = 0;
  DWORD dwOriginalInMode = 0;
  if (!GetConsoleMode(g_stdout, &dwOriginalOutMode)) {
    return false;
  }
  if (!GetConsoleMode(g_stdin, &dwOriginalInMode)) {
    return false;
  }

  DWORD dwRequestedOutModes =
      ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
  DWORD dwRequestedInModes = ENABLE_VIRTUAL_TERMINAL_INPUT;

  DWORD dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
  if (!SetConsoleMode(g_stdout, dwOutMode)) {
    // we failed to set both modes, try to step down mode gracefully.
    dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
    if (!SetConsoleMode(g_stdout, dwOutMode)) {
      // Failed to set any VT mode, can't do anything here.
      return false;
    }
  }

  DWORD dwInMode = dwOriginalInMode | dwRequestedInModes;
  if (!SetConsoleMode(g_stdin, dwInMode)) {
    // Failed to set VT input mode, can't do anything here.
    return false;
  }

  return true;
}

void InputEvent::finalize() { SetConsoleMode(g_stdin, g_fdwSaveOldMode); }

InputEvent InputEvent::get() {

  static std::queue<INPUT_RECORD> s_queue;

  if (s_queue.empty()) {
    // Wait for the events.
    DWORD cNumRead;
    INPUT_RECORD irInBuf[128];
    if (!ReadConsoleInput(g_stdin,            // input buffer handle
                          irInBuf,            // buffer to read into
                          std::size(irInBuf), // size of read buffer
                          &cNumRead           // number of records read
                          )) {
      return {
          .type = InputEventType::Error,
      };
    }
    for (int i = 0; i < cNumRead; i++) {
      s_queue.push(irInBuf[i]);
    }
  }
  auto value = s_queue.front();
  s_queue.pop();

  // Dispatch the events to the appropriate handler.
  switch (value.EventType) {
  case KEY_EVENT: // keyboard input
  {
    auto key_event = value.Event.KeyEvent;
    if (key_event.bKeyDown) {
      if (auto action = g_seq.push(key_event.uChar.AsciiChar)) {
        g_seq.clear();
        return {
            .type = InputEventType::Action,
            .action = *action,
        };
      } else {
        if (g_seq.pos) {
          return {
              .type = InputEventType::Error,
          };
        } else {
          auto byte = (uint8_t)key_event.uChar.AsciiChar;
          if (byte) {
            return {
                .type = InputEventType::Stdin,
                .byte = (uint8_t)key_event.uChar.AsciiChar,
            };
          } else {
            return {
                .type = InputEventType::Error,
            };
          }
        }
      }
    } else {
      return {
          .type = InputEventType::Error,
      };
    }
  }

  case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing
    return {
        .type = InputEventType::Resize,
    };

    //   case MOUSE_EVENT: // mouse input
    //   case FOCUS_EVENT: // disregard focus events
    //   case MENU_EVENT: // disregard menu events
  default:
    return {
        .type = InputEventType::Error,
    };
  }
}
#endif