# Windowsビルド

## raw input

```c
DWORD g_fdwSaveOldMode = 0;
HANDLE g_hStdin;

int enableRawMode(int _) {
  g_hStdin = GetStdHandle(STD_INPUT_HANDLE);

  if (!GetConsoleMode(g_hStdin, &g_fdwSaveOldMode)) {
    // ErrorExit("GetConsoleMode");
    return -1;
  }

  DWORD fdwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
  if (!SetConsoleMode(g_hStdin, fdwMode)) {
    // ErrorExit("SetConsoleMode");
    return -1;
  }

  return 1;
}

void disableRawMode(int _) { SetConsoleMode(g_hStdin, g_fdwSaveOldMode); }
```
## get window size

```c
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0) {
    return -1;
  }

  *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return 0;
}
```
## SIGWINCH

`SIGWINC` シグナルは存在しない。 

ReadConsoleInput で Size 変更の検知はできるので、
構造を修正する必要がある。

## ReadConsoleInput

rawmode fd を read する代わりにReadConsoleInput 関数を呼び出す。
block される。

```c
int getInput(int _) {
  if (g_current >= g_cNumRead) {
    g_current = 0;
    if (!ReadConsoleInput(g_hStdin,       // input buffer handle
                          g_irInBuf,      // buffer to read into
                          128,            // size of read buffer
                          &g_cNumRead)) { // number of records read
      return 0;
      // ErrorExit("ReadConsoleInput");
    }
  }

  // Dispatch the events to the appropriate handler.
  for (; g_current < g_cNumRead;) {
    INPUT_RECORD record = g_irInBuf[g_current++];
    switch (record.EventType) {
    case KEY_EVENT: // keyboard input
      // KeyEventProc(irInBuf[i].Event.KeyEvent);
      {
        KEY_EVENT_RECORD e = record.Event.KeyEvent;
        if (e.bKeyDown) {
          return e.uChar.AsciiChar;
        }
      }
      break;

    case MOUSE_EVENT: // mouse input
      // MouseEventProc(irInBuf[i].Event.MouseEvent);
      break;

    case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing
      // ResizeEventProc(irInBuf[i].Event.WindowBufferSizeEvent);
      break;

    case FOCUS_EVENT: // disregard focus events

    case MENU_EVENT: // disregard menu events
      break;

    default:
      // ErrorExit("Unknown event type");
      break;
    }
  }

  return 0;
}
```

## getline to fgets

windows 環境に getline が無かったので、
fgets で置き換えるように改造。

