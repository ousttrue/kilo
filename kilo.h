#pragma once

void editorSelectSyntaxHighlight(char *filename);
int editorOpen(char *filename);
int enableRawMode(int fd);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
void editorProcessKeypress(int fd);
int getWindowSize(int ifd, int ofd, int *rows, int *cols);