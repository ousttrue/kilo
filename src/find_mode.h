#pragma once
const int KILO_QUERY_LEN = 256;
class FindMode {
  struct editorConfig &E;
  char query[KILO_QUERY_LEN + 1] = {0};
  int qlen = 0;
  int last_match = -1;    /* Last line where a match was found. -1 for none. */
  int find_next = 0;      /* if 1 search next, if -1 search prev. */
  int saved_hl_line = -1; /* No saved HL */
  char *saved_hl = nullptr;
  int saved_cx;
  int saved_cy;
  int saved_coloff;
  int saved_rowoff;

public:
  FindMode(editorConfig &E);
  void prev();
  bool dispatch(int c);

private:
  void FIND_RESTORE_HL();
};
