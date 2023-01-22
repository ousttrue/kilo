#pragma once
#include <optional>
#include <stdint.h>
#include "escape_sequence.h"

struct TermSize {
  int rows;
  int cols;
};
std::optional<TermSize> getTermSize();

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

  static bool initialize();
  static InputEvent get();
  static void finalize();
};
