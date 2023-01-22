#include "erow.h"
#include <stdlib.h>

/* Free row's heap allocated stuff. */
void erow::editorFreeRow() {
  free(this->render);
  free(this->chars);
  free(this->hl);
}
/* Return true if the specified row last char is part of a multi line comment
 * that starts at this row or at one before, and does not end at the end
 * of the row but spawns to the next row. */
int erow::editorRowHasOpenComment() const {
  if (this->hl && this->rsize && this->hl[this->rsize - 1] == HL_MLCOMMENT &&
      (this->rsize < 2 || (this->render[this->rsize - 2] != '*' ||
                           this->render[this->rsize - 1] != '/')))
    return 1;
  return 0;
}
