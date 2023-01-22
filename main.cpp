#include "kilo.h"
#include <stdio.h>
#include <unistd.h> // STDIN_FILENO

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: kilo <filename>\n");
    return 1;
  }

  initEditor();
  editorSelectSyntaxHighlight(argv[1]);
  editorOpen(argv[1]);
  enableRawMode(STDIN_FILENO);
  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress(STDIN_FILENO);
  }
  return 0;
}
