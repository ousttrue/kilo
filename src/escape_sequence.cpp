#include "escape_sequence.h"
#include <assert.h>
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include <format>

std::optional<KEY_ACTION> EscapeSequence::push(uint8_t b) {
#ifdef _MSC_VER
  OutputDebugStringA(std::format("EscapeSequence::push({}, {:#x})\n", (char)b, b).c_str());
#endif

  assert(pos <= 3);
  if (pos == 0) {
    if (b == 27) {
      seq[pos++] = b;
    }
    return {};
  }
  seq[pos++] = b;

  if (pos == 3) {
    if (seq[1] == '[' || seq[1] == 'O') {
      switch (seq[2]) {
      case 'A':
        // ESC [ A
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
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        break;

      default:
        // unknown
        assert(false);
        break;
      }
    }
  } else if (pos == 4) {
    if (seq[3] == '~') {
      switch (seq[2]) {
      case '3':
        // ESC [ 3 ~
        return DEL_KEY;
      case '5':
        // ESC [ 5 ~
        return PAGE_UP;
      case '6':
        // ESC [ 6 ~
        return PAGE_DOWN;

      default:
        clear();
      }
    }
  }

  return {};
}
