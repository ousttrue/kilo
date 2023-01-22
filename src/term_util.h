#pragma once
#include <optional>

struct TermSize {
  int rows;
  int cols;
};

std::optional<TermSize> getTermSize(int ifd, int ofd);

int enableRawMode(int fd);
void disableRawMode(int fd);
