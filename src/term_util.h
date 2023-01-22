#pragma once

int getTermSize(int ifd, int ofd, int *rows, int *cols);

int enableRawMode(int fd);
void disableRawMode(int fd);
