#include "append_buffer.h"
#include <stdlib.h>
#include <string.h>

void abuf::abAppend(const char *s, int len) {
  char *new_ = (char *)realloc(this->b, this->len + len);

  if (new_ == NULL)
    return;
  memcpy(new_ + this->len, s, len);
  this->b = new_;
  this->len += len;
}

void abuf::abFree() { free(this->b); }
