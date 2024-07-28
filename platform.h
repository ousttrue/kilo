#ifndef PLATFORM_H
#define PLATFORM_H

void initResizeSignal();

void disableRawMode(int fd);

/* Raw mode: 1960 magic shit. */
int enableRawMode(int fd);

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int getWindowSize(int ifd, int ofd, int *rows, int *cols);

#ifdef _WIN32
#include <sys/types.h>
#include <stdio.h>
ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream);
#endif

#endif
