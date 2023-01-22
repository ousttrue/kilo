#pragma once
#include <optional>
#include <stdint.h>

struct TermSize {
  int rows;
  int cols;
};

std::optional<TermSize> getTermSize(int ifd, int ofd);

int enableRawMode(int fd);
void disableRawMode(int fd);

enum KEY_ACTION {
  KEY_NULL = 0,    /* NULL */
  CTRL_C = 3,      /* Ctrl-c */
  CTRL_D = 4,      /* Ctrl-d */
  CTRL_F = 6,      /* Ctrl-f */
  CTRL_G = 7,      /* Ctrl-f */
  CTRL_H = 8,      /* Ctrl-h */
  TAB = 9,         /* Tab */
  CTRL_L = 12,     /* Ctrl+l */
  ENTER = 13,      /* Enter */
  CTRL_Q = 17,     /* Ctrl-q */
  CTRL_S = 19,     /* Ctrl-s */
  CTRL_U = 21,     /* Ctrl-u */
  CTRL_X = 24,     /* Ctrl-q */
  ESC = 27,        /* Escape */
  BACKSPACE = 127, /* Backspace */
  /* The following are just soft codes, not really reported by the
   * terminal directly. */
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum InputEventType {
  Error,
  Stdin,
  Action,
  Resize,
};

struct InputEvent {
  InputEventType type;
  union {
    uint8_t byte;
    KEY_ACTION action;
    TermSize size;
  };

  static void initialize();
  static InputEvent get();
};
