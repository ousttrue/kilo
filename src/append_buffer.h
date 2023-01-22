#pragma once

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
  char *b = nullptr;
  int len = 0;
  void abAppend(const char *s, int len);
  void abFree();
};
