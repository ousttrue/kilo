#ifndef _WIN32
#include "term_util.h"
#include <cstdint>
#include <errno.h>
#include <optional>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Is terminal raw mode enabled?
bool g_rawmode = false;

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
static int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  /* Report cursor location */
  if (write(ofd, "\x1b[6n", 4) != 4)
    return -1;

  /* Read the response: ESC [ rows ; cols R */
  while (i < sizeof(buf) - 1) {
    if (read(ifd, buf + i, 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  /* Parse it. */
  if (buf[0] != 27 || buf[1] != '[')
    return -1;
  if (sscanf(buf + 2, "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
std::optional<TermSize> getTermSize() {
  int ifd = STDIN_FILENO;
  int ofd = STDOUT_FILENO;
  struct winsize ws;

  TermSize size{};
  if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    /* ioctl() failed. Try to query the terminal itself. */
    int orig_row, orig_col, retval;

    /* Get the initial position so we can restore it later. */
    retval = getCursorPosition(ifd, ofd, &orig_row, &orig_col);
    if (retval == -1)
      goto failed;

    /* Go to right/bottom margin and get position. */
    if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12)
      goto failed;
    retval = getCursorPosition(ifd, ofd, &size.rows, &size.cols);
    if (retval == -1)
      goto failed;

    /* Restore position. */
    char seq[32];
    snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
    if (write(ofd, seq, strlen(seq)) == -1) {
      /* Can't recover... */
    }
    return size;
  } else {
    size.cols = ws.ws_col;
    size.rows = ws.ws_row;
    return size;
  }

failed:
  return {};
}

static struct termios orig_termios; /* In order to restore at exit.*/

void disableRawMode() {
  if (g_rawmode) {
    /* Don't even check the return value as it's too late. */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    g_rawmode = false;
  }
}

/* Raw mode: 1960 magic shit. */
bool enableRawMode() {
  struct termios raw;

  if (!isatty(STDIN_FILENO))
    goto fatal;
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
    goto fatal;
  g_rawmode = true;
  return true;

fatal:
  errno = ENOTTY;
  return false;
}

static bool g_sigwinch = false;

static void handleSigWinCh(int) { g_sigwinch = true; }

void InputEvent::initialize() { signal(SIGWINCH, handleSigWinCh); }

EscapeSequence g_seq;

InputEvent InputEvent::get() {
  if (g_sigwinch) {
    g_sigwinch = false;
    if (auto size = getTermSize()) {
      return {
          .type = InputEventType::Resize,
          .size = *size,
      };
    } else {
      return {
          .type = InputEventType::Error,
      };
    }
  }

  uint8_t b;
  auto read_size = read(STDIN_FILENO, &b, 1);
  if (read_size < 0) {
    return {
        .type = InputEventType::Error,
    };
  }

  if (read_size == 0) {
    g_seq.clear();
    return {
        .type = InputEventType::Error,
    };
  }

  assert(read_size == 1);
  if (auto action = g_seq.push(b)) {
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
      return {
          .type = InputEventType::Stdin,
          .byte = b,
      };
    }
  }
}
#endif