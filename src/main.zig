const std = @import("std");
const c = @import("c");

var allocator: std.mem.Allocator = undefined;
var running = true;

// Kilo -- A very simple editor in less than 1-kilo lines of code (as counted
//         by "cloc"). Does not depend on libcurses, directly emits VT100
//         escapes on the terminal.
// -----------------------------------------------------------------------
// Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// #define KILO_VERSION "0.0.1"

// #ifdef __linux__
// #define _POSIX_C_SOURCE 200809L
// #endif

// Syntax highlight types
const HL_NORMAL= 0;
const HL_NONPRINT= 1;
const HL_COMMENT= 2; // Single line comment.
const HL_MLCOMMENT= 3; // Multi-line comment.
const HL_KEYWORD1= 4;
const HL_KEYWORD2= 5;
const HL_STRING= 6;
const HL_NUMBER= 7;
const HL_MATCH= 8; // Search match.

const HL_HIGHLIGHT_STRINGS = (1 << 0);
const HL_HIGHLIGHT_NUMBERS = (1 << 1);

const EditorSyntax = struct {
    filematch: []const []const u8,
    keywords: []const []const u8,
    singleline_comment_start: []const u8,
    multiline_comment_start: []const u8,
    multiline_comment_end: []const u8,
    flags: i32,
};

// // This structure represents a single line of the file we are editing.

const Erow = struct {
    /// Row index in the file, zero-based.
    idx: i32,
    /// Size of the row, excluding the null term.
    size: i32,
    /// Size of the rendered row.
    rsize: u32,
    /// Row content.
    chars: std.ArrayList(u8),
    /// Row content "rendered" for screen (for TABs).
    render: std.ArrayList(u8),
    /// Syntax highlight type for each character in render.
    hl: std.ArrayList(u8),
    /// Row had open comment at end in last syntax highlight check.
    hl_oc: i32,
};

// typedef struct hlcolor
// {
//     int r, g, b;
// } hlcolor;

const EditorConfig = struct {
    const Self = @This();

    /// Cursor x and y position in characters
    cx: i32 = 0,
    cy: i32 = 0,
    /// Offset of row displayed.
    rowoff: u32 = 0,
    /// Offset of column displayed.
    coloff: u32 = 0,
    /// Number of rows that we can show
    screenrows: u32 = 0,
    /// Number of cols that we can show
    screencols: u32 = 0,
    /// Is terminal raw mode enabled?
    rawmode: bool = false,
    /// Rows
    row: std.ArrayList(Erow),
    /// File modified but not saved.
    dirty: i32 = 0,
    /// Currently open filename
    filename: ?*u8 = null,
    statusmsg: [80]u8 = undefined,
    statusmsg_time: c.time_t = undefined,
    /// Current syntax highlight, or null.
    syntax: ?*const EditorSyntax = null,

    fn init(a: std.mem.Allocator) Self {
        return Self{
            .row = std.ArrayList(Erow).init(a),
        };
    }

    fn deinit(self: Self) void {
        self.row.deinit();
    }

    fn rows(self: Self) []const Erow {
        const start = self.rowoff;
        const end = std.math.min(start + self.screenrows, self.row.items.len);
        return self.row.items[start..end];
    }
};

var E: EditorConfig = undefined;

const KEY_ACTION = enum(i32) {
    KEY_NULL = 0, // null
    CTRL_C = 3, // Ctrl-c
    CTRL_D = 4, // Ctrl-d
    CTRL_F = 6, // Ctrl-f
    CTRL_H = 8, // Ctrl-h
    TAB = 9, // Tab
    CTRL_L = 12, // Ctrl+l
    ENTER = 13, // Enter
    CTRL_Q = 17, // Ctrl-q
    CTRL_S = 19, // Ctrl-s
    CTRL_U = 21, // Ctrl-u
    ESC = 27, // Escape
    BACKSPACE = 127, // Backspace
    // The following are just soft codes, not really reported by the
    // terminal directly.
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

// =========================== Syntax highlights DB =========================
// In order to add a new syntax, define two arrays with a list of file name
// matches and keywords. The file name matches are used in order to match
// a given syntax with a given file name: if a match pattern starts with a
// dot, it is matched as the last past of the filename, for example ".c".
// Otherwise the pattern is just searched inside the filenme, like "Makefile").
// The list of keywords to highlight is just a list of words, however if they
// a trailing '|' character is added at the end, they are highlighted in
// a different color, so that you can have two different sets of keywords.
// Finally add a stanza in the HLDB global variable with two two arrays
// of strings, and a set of flags in order to enable highlighting of
// comments and numbers.
// The characters for single and multi line comments must be exactly two
// and must be provided as well (see the C language example).
// There is no support to highlight patterns currently.

// C / C++
const C_HL_extensions = [_][]const u8{ ".c", ".h", ".cpp", ".hpp", ".cc" };
const C_HL_keywords = [_][]const u8{
    // C Keywords
    "auto",          "break",       "case",      "continue",  "default",      "do",        "else",   "enum",
    "extern",        "for",         "goto",      "if",        "register",     "return",    "sizeof", "static",
    "struct",        "switch",      "typedef",   "union",     "volatile",     "while",     "null",

    // C++ Keywords
      "alignas",
    "alignof",       "and",         "and_eq",    "asm",       "bitand",       "bitor",     "class",  "compl",
    "constexpr",     "const_cast",  "deltype",   "delete",    "dynamic_cast", "explicit",  "export", "false",
    "friend",        "inline",      "mutable",   "namespace", "new",          "noexcept",  "not",    "not_eq",
    "nullptr",       "operator",    "or",        "or_eq",     "private",      "protected", "public", "reinterpret_cast",
    "static_assert", "static_cast", "template",  "this",      "thread_local", "throw",     "true",   "try",
    "typeid",        "typename",    "virtual",   "xor",       "xor_eq",

    // C types
          "int|",      "long|",  "double|",
    "float|",        "char|",       "unsigned|", "signed|",   "void|",        "short|",    "auto|",  "const|",
    "bool|",
};

/// Here we define an array of syntax highlights by extensions, keywords,
/// comments delimiters and flags.
const HLDB = [_]EditorSyntax{
    // C / C++
    .{
        .filematch = &C_HL_extensions,
        .keywords = &C_HL_keywords,
        .singleline_comment_start = "//",
        .multiline_comment_start = "/*",
        .multiline_comment_end = "*/",
        .flags = HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
    },
};

// #define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

// ======================= Low level terminal handling ======================

var orig_termios: c.termios = undefined;
/// In order to restore at exit.
fn disableRawMode(fd: i32) void {
    // Don't even check the return value as it's too late.
    if (E.rawmode) {
        _ = c.tcsetattr(fd, c.TCSAFLUSH, &orig_termios);
        E.rawmode = false;
    }
}

/// Called at exit to avoid remaining in raw mode.
fn editorAtExit() callconv(.C) void {
    disableRawMode(c.STDIN_FILENO);
}

/// Raw mode: 1960 magic shit.
fn enableRawMode(fd: i32) !void {
    if (E.rawmode)
        return; // Already enabled.

    if (c.isatty(c.STDIN_FILENO) == 0) {
        // c.errno = c.ENOTTY;
        return error.isatty;
    }
    _ = c.atexit(editorAtExit);
    if (c.tcgetattr(fd, &orig_termios) == -1) {
        // c.errno = c.ENOTTY;
        return error.isatty;
    }

    // modify the original mode
    var raw = orig_termios;

    // input modes: no break, no CR to NL, no parity check, no strip char,
    // no start/stop output control.

    raw.c_iflag &= @bitCast(c_uint, ~(c.BRKINT | c.ICRNL | c.INPCK | c.ISTRIP | c.IXON));
    // output modes - disable post processing

    raw.c_oflag &= @bitCast(c_uint, ~(c.OPOST));
    // control modes - set 8 bit chars

    raw.c_cflag |= (c.CS8);
    // local modes - choing off, canonical off, no extended functions,
    // no signal chars (^Z,^C)

    raw.c_lflag &= @bitCast(c_uint, ~(c.ECHO | c.ICANON | c.IEXTEN | c.ISIG));
    // control chars - set return condition: min number of bytes and timer.

    raw.c_cc[c.VMIN] = 0; // Return each byte, or zero for timeout.

    raw.c_cc[c.VTIME] = 1; // 100 ms timeout (unit is tens of second).

    // put terminal in raw mode after flushing

    if (c.tcsetattr(fd, c.TCSAFLUSH, &raw) < 0) {
        // c.errno = c.ENOTTY;
        return error.isatty;
    }
    E.rawmode = true;
}

/// Read a key from the terminal put in raw mode, trying to handle
/// escape sequences.
fn editorReadKey(fd: i32) KEY_ACTION {
    var nread: isize = undefined;
    var ch: u8 = undefined;
    while (true) {
        nread = c.read(fd, &ch, 1);
        if (nread != 0) {
            break;
        }
    }
    if (nread == -1)
        c.exit(1);

    while (true) {
        switch (@intToEnum(KEY_ACTION, ch)) {
            // escape sequence
            KEY_ACTION.ESC => {
                // If this is just an ESC, we'll timeout here.
                var seq: [3]u8 = undefined;
                if (c.read(fd, &seq[0], 1) == 0)
                    return KEY_ACTION.ESC;
                if (c.read(fd, &seq[1], 1) == 0)
                    return KEY_ACTION.ESC;

                // ESC [ sequences.

                if (seq[0] == '[') {
                    if (seq[1] >= '0' and seq[1] <= '9') {
                        // Extended escape, read additional byte.

                        if (c.read(fd, &seq[2], 1) == 0)
                            return KEY_ACTION.ESC;
                        if (seq[2] == '~') {
                            switch (seq[1]) {
                                '3' => return KEY_ACTION.DEL_KEY,
                                '5' => return KEY_ACTION.PAGE_UP,
                                '6' => return KEY_ACTION.PAGE_DOWN,
                                else => unreachable,
                            }
                        }
                    } else {
                        switch (seq[1]) {
                            'A' => return KEY_ACTION.ARROW_UP,
                            'B' => return KEY_ACTION.ARROW_DOWN,
                            'C' => return KEY_ACTION.ARROW_RIGHT,
                            'D' => return KEY_ACTION.ARROW_LEFT,
                            'H' => return KEY_ACTION.HOME_KEY,
                            'F' => return KEY_ACTION.END_KEY,
                            else => unreachable,
                        }
                    }
                }
                // ESC O sequences.
                else if (seq[0] == 'O') {
                    switch (seq[1]) {
                        'H' => return KEY_ACTION.HOME_KEY,
                        'F' => return KEY_ACTION.END_KEY,
                        else => unreachable,
                    }
                }
            },
            else => return @intToEnum(KEY_ACTION, ch),
        }
    }
}

/// Use the ESC [6n escape sequence to query the horizontal cursor position
/// and return it. On error -1 is returned, on success the position of the
/// cursor is stored at *rows and *cols and 0 is returned.
fn getCursorPosition(ifd: i32, ofd: i32, rows: *u32, cols: *u32) !void {

    // Report cursor location
    if (c.write(ofd, "\x1b[6n", 4) != 4)
        return error.getCursorPosition;

    // Read the response: ESC [ rows ; cols R
    var buf: [32]u8 = undefined;
    var i: u32 = 0;
    while (i < @sizeOf(@TypeOf(buf)) - 1) : (i += 1) {
        if (c.read(ifd, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
    }
    buf[i] = 0;

    // Parse it.
    if (buf[0] != @enumToInt(KEY_ACTION.ESC) or buf[1] != '[')
        return error.getCursorPosition;
    if (c.sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return error.getCursorPosition;
}

/// Try to get the number of columns in the current terminal. If the ioctl()
/// call fails the function will try to query the terminal itself.
/// Returns 0 on success, -1 on error.
fn getWindowSize(ifd: i32, ofd: i32, rows: *u32, cols: *u32) !void {
    var ws: c.winsize = undefined;

    if (c.ioctl(1, c.TIOCGWINSZ, &ws) == -1 or ws.ws_col == 0) {
        // ioctl() failed. Try to query the terminal itself.

        var orig_row: u32 = undefined;
        var orig_col: u32 = undefined;
        // Get the initial position so we can restore it later.
        try getCursorPosition(ifd, ofd, &orig_row, &orig_col);

        // Go to right/bottom margin and get position.
        if (c.write(ofd, "\x1b[999C\x1b[999B", 12) != 12)
            return error.gotoRightBottom;
        try getCursorPosition(ifd, ofd, rows, cols);

        // Restore position.
        var seq: [32]u8 = undefined;
        _ = c.snprintf(&seq[0], 32, "\x1b[%d;%dH", orig_row, orig_col);
        if (c.write(ofd, &seq[0], c.strlen(&seq[0])) == -1) {
            // Can't recover...
        }
    } else {
        cols.* = ws.ws_col;
        rows.* = ws.ws_row;
    }
}

// // ====================== Syntax highlight color scheme  ====================

// int is_separator(int c)
// {
//     return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];", c) != null;
// }

// // Return true if the specified row last char is part of a multi line comment
// // that starts at this row or at one before, and does not end at the end
// // of the row but spawns to the next row.

// int editorRowHasOpenComment(erow *row)
// {
//     if (row.hl && row.rsize && row.hl[row.rsize - 1] == HL_MLCOMMENT &&
//         (row.rsize < 2 || (row.render[row.rsize - 2] != '*' ||
//                             row.render[row.rsize - 1] != '/')))
//         return 1;
//     return 0;
// }

// // Set every byte of row.hl (that corresponds to every character in the line)
// // to the right syntax highlight type (HL_* defines).

// void editorUpdateSyntax(erow *row)
// {
//     row.hl = realloc(row.hl, row.rsize);
//     memset(row.hl, HL_NORMAL, row.rsize);

//     if (E.syntax == null)
//         return; // No syntax, everything is HL_NORMAL.

//     int i, prev_sep, in_string, in_comment;
//     char *p;
//     char **keywords = E.syntax.keywords;
//     char *scs = E.syntax.singleline_comment_start;
//     char *mcs = E.syntax.multiline_comment_start;
//     char *mce = E.syntax.multiline_comment_end;

//     // Point to the first non-space char.

//     p = row.render;
//     i = 0; // Current char offset

//     while (*p && isspace(*p))
//     {
//         p++;
//         i++;
//     }
//     prev_sep = 1; // Tell the parser if 'i' points to start of word.

//     in_string = 0; // Are we inside "" or '' ?

//     in_comment = 0; // Are we inside multi-line comment?

//     // If the previous line has an open comment, this line starts
//     // with an open comment state.

//     if (row.idx > 0 && editorRowHasOpenComment(&E.row[row.idx - 1]))
//         in_comment = 1;

//     while (*p)
//     {
//         // Handle // comments.

//         if (prev_sep && *p == scs[0] && *(p + 1) == scs[1])
//         {
//             // From here to end is a comment

//             memset(row.hl + i, HL_COMMENT, row.size - i);
//             return;
//         }

//         // Handle multi line comments.

//         if (in_comment)
//         {
//             row.hl[i] = HL_MLCOMMENT;
//             if (*p == mce[0] && *(p + 1) == mce[1])
//             {
//                 row.hl[i + 1] = HL_MLCOMMENT;
//                 p += 2;
//                 i += 2;
//                 in_comment = 0;
//                 prev_sep = 1;
//                 continue;
//             }
//             else
//             {
//                 prev_sep = 0;
//                 p++;
//                 i++;
//                 continue;
//             }
//         }
//         else if (*p == mcs[0] && *(p + 1) == mcs[1])
//         {
//             row.hl[i] = HL_MLCOMMENT;
//             row.hl[i + 1] = HL_MLCOMMENT;
//             p += 2;
//             i += 2;
//             in_comment = 1;
//             prev_sep = 0;
//             continue;
//         }

//         // Handle "" and ''

//         if (in_string)
//         {
//             row.hl[i] = HL_STRING;
//             if (*p == '\\')
//             {
//                 row.hl[i + 1] = HL_STRING;
//                 p += 2;
//                 i += 2;
//                 prev_sep = 0;
//                 continue;
//             }
//             if (*p == in_string)
//                 in_string = 0;
//             p++;
//             i++;
//             continue;
//         }
//         else
//         {
//             if (*p == '"' || *p == '\'')
//             {
//                 in_string = *p;
//                 row.hl[i] = HL_STRING;
//                 p++;
//                 i++;
//                 prev_sep = 0;
//                 continue;
//             }
//         }

//         // Handle non printable chars.

//         if (!isprint(*p))
//         {
//             row.hl[i] = HL_NONPRINT;
//             p++;
//             i++;
//             prev_sep = 0;
//             continue;
//         }

//         // Handle numbers

//         if ((isdigit(*p) && (prev_sep || row.hl[i - 1] == HL_NUMBER)) ||
//             (*p == '.' && i > 0 && row.hl[i - 1] == HL_NUMBER))
//         {
//             row.hl[i] = HL_NUMBER;
//             p++;
//             i++;
//             prev_sep = 0;
//             continue;
//         }

//         // Handle keywords and lib calls

//         if (prev_sep)
//         {
//             int j;
//             for (j = 0; keywords[j]; j++)
//             {
//                 int klen = strlen(keywords[j]);
//                 int kw2 = keywords[j][klen - 1] == '|';
//                 if (kw2)
//                     klen--;

//                 if (!memcmp(p, keywords[j], klen) &&
//                     is_separator(*(p + klen)))
//                 {
//                     // Keyword

//                     memset(row.hl + i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
//                     p += klen;
//                     i += klen;
//                     break;
//                 }
//             }
//             if (keywords[j] != null)
//             {
//                 prev_sep = 0;
//                 continue; // We had a keyword match
//             }
//         }

//         // Not special chars

//         prev_sep = is_separator(*p);
//         p++;
//         i++;
//     }

//     // Propagate syntax change to the next row if the open commen
//     // state changed. This may recursively affect all the following rows
//     // in the file.

//     int oc = editorRowHasOpenComment(row);
//     if (row.hl_oc != oc && row.idx + 1 < E.numrows)
//         editorUpdateSyntax(&E.row[row.idx + 1]);
//     row.hl_oc = oc;
// }

/// Maps syntax highlight token types to terminal colors.
fn editorSyntaxToColor(hl: i32) i32
{
    return switch (hl)
    {
        // cyan
        HL_COMMENT, HL_MLCOMMENT => 36,

        // yellow
    HL_KEYWORD1 =>  33,

     // green
    HL_KEYWORD2 => 32,

     // magenta
    HL_STRING =>35,

     // red
    HL_NUMBER =>31,

     // blu
    HL_MATCH=>34,

     // white
    else =>37,
    };
}

/// Select the syntax highlight scheme depending on the filename,
/// setting it in the global state E.syntax.
fn editorSelectSyntaxHighlight(filename: []const u8) void {
    _ = filename;
    for (HLDB) |*s| {
        for (s.filematch) |match| {
            if (std.mem.endsWith(u8, filename, match)) {
                E.syntax = s;
                return;
            }
        }
    }
}

// // ======================= Editor rows implementation =======================

// // Update the rendered version and the syntax highlight of a row.

// void editorUpdateRow(erow *row)
// {
//     unsigned int tabs = 0, nonprint = 0;
//     int j, idx;

//     // Create a version of the row we can directly print on the screen,
//     // respecting tabs, substituting non printable characters with '?'.

//     free(row.render);
//     for (j = 0; j < row.size; j++)
//         if (row.chars[j] == TAB)
//             tabs++;

//     unsigned long long allocsize =
//         (unsigned long long)row.size + tabs * 8 + nonprint * 9 + 1;
//     if (allocsize > UINT32_MAX)
//     {
//         printf("Some line of the edited file is too long for kilo\n");
//         exit(1);
//     }

//     row.render = malloc(row.size + tabs * 8 + nonprint * 9 + 1);
//     idx = 0;
//     for (j = 0; j < row.size; j++)
//     {
//         if (row.chars[j] == TAB)
//         {
//             row.render[idx++] = ' ';
//             while ((idx + 1) % 8 != 0)
//                 row.render[idx++] = ' ';
//         }
//         else
//         {
//             row.render[idx++] = row.chars[j];
//         }
//     }
//     row.rsize = idx;
//     row.render[idx] = '\0';

//     // Update the syntax highlighting attributes of the row.

//     editorUpdateSyntax(row);
// }

// // Insert a row at the specified position, shifting the other rows on the bottom
// // if required.

// void editorInsertRow(int at, char *s, size_t len)
// {
//     if (at > E.numrows)
//         return;
//     E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
//     if (at != E.numrows)
//     {
//         memmove(E.row + at + 1, E.row + at, sizeof(E.row[0]) * (E.numrows - at));
//         for (int j = at + 1; j <= E.numrows; j++)
//             E.row[j].idx++;
//     }
//     E.row[at].size = len;
//     E.row[at].chars = malloc(len + 1);
//     memcpy(E.row[at].chars, s, len + 1);
//     E.row[at].hl = null;
//     E.row[at].hl_oc = 0;
//     E.row[at].render = null;
//     E.row[at].rsize = 0;
//     E.row[at].idx = at;
//     editorUpdateRow(E.row + at);
//     E.numrows++;
//     E.dirty++;
// }

// // Free row's heap allocated stuff.

// void editorFreeRow(erow *row)
// {
//     free(row.render);
//     free(row.chars);
//     free(row.hl);
// }

// // Remove the row at the specified position, shifting the remainign on the
// // top.

// void editorDelRow(int at)
// {
//     erow *row;

//     if (at >= E.numrows)
//         return;
//     row = E.row + at;
//     editorFreeRow(row);
//     memmove(E.row + at, E.row + at + 1, sizeof(E.row[0]) * (E.numrows - at - 1));
//     for (int j = at; j < E.numrows - 1; j++)
//         E.row[j].idx++;
//     E.numrows--;
//     E.dirty++;
// }

// // Turn the editor rows into a single heap-allocated string.
// // Returns the pointer to the heap-allocated string and populate the
// // integer pointed by 'buflen' with the size of the string, escluding
// // the final nulterm.

// char *editorRowsToString(int *buflen)
// {
//     char *buf = null, *p;
//     int totlen = 0;
//     int j;

//     // Compute count of bytes

//     for (j = 0; j < E.numrows; j++)
//         totlen += E.row[j].size + 1; // +1 is for "\n" at end of every row

//     *buflen = totlen;
//     totlen++; // Also make space for nulterm

//     p = buf = malloc(totlen);
//     for (j = 0; j < E.numrows; j++)
//     {
//         memcpy(p, E.row[j].chars, E.row[j].size);
//         p += E.row[j].size;
//         *p = '\n';
//         p++;
//     }
//     *p = '\0';
//     return buf;
// }

// // Insert a character at the specified position in a row, moving the remaining
// // chars on the right if needed.

// void editorRowInsertChar(erow *row, int at, int c)
// {
//     if (at > row.size)
//     {
//         // Pad the string with spaces if the insert location is outside the
//         // current length by more than a single character.

//         int padlen = at - row.size;
//         // In the next line +2 means: new char and null term.

//         row.chars = realloc(row.chars, row.size + padlen + 2);
//         memset(row.chars + row.size, ' ', padlen);
//         row.chars[row.size + padlen + 1] = '\0';
//         row.size += padlen + 1;
//     }
//     else
//     {
//         // If we are in the middle of the string just make space for 1 new
//         // char plus the (already existing) null term.

//         row.chars = realloc(row.chars, row.size + 2);
//         memmove(row.chars + at + 1, row.chars + at, row.size - at + 1);
//         row.size++;
//     }
//     row.chars[at] = c;
//     editorUpdateRow(row);
//     E.dirty++;
// }

// // Append the string 's' at the end of a row

// void editorRowAppendString(erow *row, char *s, size_t len)
// {
//     row.chars = realloc(row.chars, row.size + len + 1);
//     memcpy(row.chars + row.size, s, len);
//     row.size += len;
//     row.chars[row.size] = '\0';
//     editorUpdateRow(row);
//     E.dirty++;
// }

// // Delete the character at offset 'at' from the specified row.

// void editorRowDelChar(erow *row, int at)
// {
//     if (row.size <= at)
//         return;
//     memmove(row.chars + at, row.chars + at + 1, row.size - at);
//     editorUpdateRow(row);
//     row.size--;
//     E.dirty++;
// }

// // Insert the specified char at the current prompt position.

// void editorInsertChar(int c)
// {
//     int filerow = E.rowoff + E.cy;
//     int filecol = E.coloff + E.cx;
//     erow *row = (filerow >= E.numrows) ? null : &E.row[filerow];

//     // If the row where the cursor is currently located does not exist in our
//     // logical representaion of the file, add enough empty rows as needed.

//     if (!row)
//     {
//         while (E.numrows <= filerow)
//             editorInsertRow(E.numrows, "", 0);
//     }
//     row = &E.row[filerow];
//     editorRowInsertChar(row, filecol, c);
//     if (E.cx == E.screencols - 1)
//         E.coloff++;
//     else
//         E.cx++;
//     E.dirty++;
// }

/// Inserting a newline is slightly complex as we have to handle inserting a
/// newline in the middle of a line, splitting the line as needed.
fn editorInsertNewline() void {
    //     int filerow = E.rowoff + E.cy;
    //     int filecol = E.coloff + E.cx;
    //     erow *row = (filerow >= E.numrows) ? null : &E.row[filerow];

    //     if (!row)
    //     {
    //         if (filerow == E.numrows)
    //         {
    //             editorInsertRow(filerow, "", 0);
    //             goto fixcursor;
    //         }
    //         return;
    //     }
    //     // If the cursor is over the current line size, we want to conceptually
    //     // think it's just over the last character.

    //     if (filecol >= row.size)
    //         filecol = row.size;
    //     if (filecol == 0)
    //     {
    //         editorInsertRow(filerow, "", 0);
    //     }
    //     else
    //     {
    //         // We are in the middle of a line. Split it between two rows.

    //         editorInsertRow(filerow + 1, row.chars + filecol, row.size - filecol);
    //         row = &E.row[filerow];
    //         row.chars[filecol] = '\0';
    //         row.size = filecol;
    //         editorUpdateRow(row);
    //     }
    // fixcursor:
    //     if (E.cy == E.screenrows - 1)
    //     {
    //         E.rowoff++;
    //     }
    //     else
    //     {
    //         E.cy++;
    //     }
    //     E.cx = 0;
    //     E.coloff = 0;
}

// // Delete the char at the current prompt position.

// void editorDelChar()
// {
//     int filerow = E.rowoff + E.cy;
//     int filecol = E.coloff + E.cx;
//     erow *row = (filerow >= E.numrows) ? null : &E.row[filerow];

//     if (!row || (filecol == 0 && filerow == 0))
//         return;
//     if (filecol == 0)
//     {
//         // Handle the case of column 0, we need to move the current line
//         // on the right of the previous one.

//         filecol = E.row[filerow - 1].size;
//         editorRowAppendString(&E.row[filerow - 1], row.chars, row.size);
//         editorDelRow(filerow);
//         row = null;
//         if (E.cy == 0)
//             E.rowoff--;
//         else
//             E.cy--;
//         E.cx = filecol;
//         if (E.cx >= E.screencols)
//         {
//             int shift = (E.screencols - E.cx) + 1;
//             E.cx -= shift;
//             E.coloff += shift;
//         }
//     }
//     else
//     {
//         editorRowDelChar(row, filecol - 1);
//         if (E.cx == 0 && E.coloff)
//             E.coloff--;
//         else
//             E.cx--;
//     }
//     if (row)
//         editorUpdateRow(row);
//     E.dirty++;
// }

// // Load the specified program in the editor memory and returns 0 on success
// // or 1 on error.

// int editorOpen(char *filename)
// {
//     FILE *fp;

//     E.dirty = 0;
//     free(E.filename);
//     size_t fnlen = strlen(filename) + 1;
//     E.filename = malloc(fnlen);
//     memcpy(E.filename, filename, fnlen);

//     fp = fopen(filename, "r");
//     if (!fp)
//     {
//         if (errno != ENOENT)
//         {
//             perror("Opening file");
//             exit(1);
//         }
//         return 1;
//     }

//     char *line = null;
//     size_t linecap = 0;
//     ssize_t linelen;
//     while ((linelen = getline(&line, &linecap, fp)) != -1)
//     {
//         if (linelen && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
//             line[--linelen] = '\0';
//         editorInsertRow(E.numrows, line, linelen);
//     }
//     free(line);
//     fclose(fp);
//     E.dirty = 0;
//     return 0;
// }

// // Save the current file on disk. Return 0 on success, 1 on error.

// int editorSave(void)
// {
//     int len;
//     char *buf = editorRowsToString(&len);
//     int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
//     if (fd == -1)
//         goto writeerr;

//     // Use truncate + a single write(2) call in order to make saving
//     // a bit safer, under the limits of what we can do in a small editor.

//     if (ftruncate(fd, len) == -1)
//         goto writeerr;
//     if (write(fd, buf, len) != len)
//         goto writeerr;

//     close(fd);
//     free(buf);
//     E.dirty = 0;
//     editorSetStatusMessage("%d bytes written on disk", len);
//     return 0;

// writeerr:
//     free(buf);
//     if (fd != -1)
//         close(fd);
//     editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
//     return 1;
// }

// // ============================= Terminal update ============================

// // We define a very simple "append buffer" structure, that is an heap
// // allocated string where we can append to. This is useful in order to
// // write all the escape sequences in a buffer and flush them to the standard
// // output in a single call, to avoid flickering effects.

const Abuf = std.ArrayList(u8);

/// This function writes the whole screen using VT100 escape characters
/// starting from the logical state of the editor in the global state 'E'.
fn editorRefreshScreen() void {
    //     erow *r;
    //     char buf[32];
    var ab = Abuf.init(allocator);
    defer {
        _ = c.write(c.STDOUT_FILENO, &ab.items[0], ab.items.len);
        ab.deinit();
    }
    errdefer {
        ab.deinit();
    }

    // Hide cursor.
    ab.appendSlice("\x1b[?25l") catch unreachable;
    // Show cursor.
    defer ab.appendSlice("\x1b[?25h") catch unreachable;

    // Go home.
    ab.appendSlice("\x1b[H") catch unreachable;

    for (E.rows()) |r| {

        //     int y;
        //     for (y = 0; y < E.screenrows; y++)
        //     {
        //         int filerow = E.rowoff + y;

        //         if (filerow >= E.numrows)
        //         {
        //             if (E.numrows == 0 && y == E.screenrows / 3)
        //             {
        //                 char welcome[80];
        //                 int welcomelen = snprintf(welcome, sizeof(welcome),
        //                                           "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
        //                 int padding = (E.screencols - welcomelen) / 2;
        //                 if (padding)
        //                 {
        //                     abAppend(&ab, "~", 1);
        //                     padding--;
        //                 }
        //                 while (padding--)
        //                     abAppend(&ab, " ", 1);
        //                 abAppend(&ab, welcome, welcomelen);
        //             }
        //             else
        //             {
        //                 abAppend(&ab, "~\x1b[0K\r\n", 7);
        //             }
        //             continue;
        //         }

        //         r = &E.row[filerow];

        var len: u32 = r.rsize - E.coloff;
        var current_color: i32 = -1;
        if (len > 0)
        {
            if (len > E.screencols){
                len = E.screencols;
            }
            var p = r.render.items[E.coloff..];
            var hl = r.hl.items[E.coloff..];
            var j: u32 = 0;
            while(j<len):(j+=1)
            {
                if (hl[j] == HL_NONPRINT)
                {
                    ab.appendSlice("\x1b[7m") catch unreachable;
                    const sym = if (p[j] <= 26)
                        '@' + p[j]                    
                    else
                        '?';
                    
                    ab.append(sym) catch unreachable;
                    ab.appendSlice("\x1b[0m") catch unreachable;
                }
                else if (hl[j] == HL_NORMAL)
                {
                    if (current_color != -1)
                    {
                        ab.appendSlice("\x1b[39m") catch unreachable;
                        current_color = -1;
                    }
                    ab.append(p[j]) catch unreachable;
                }
                else
                {
                    const color = editorSyntaxToColor(hl[j]);
                    if (color != current_color)
                    {
                        var buf:[16]u8 = undefined;
                        const clen = @intCast(u32, c.snprintf(&buf[0], buf.len, "\x1b[%dm", color));
                        current_color = color;
                        ab.appendSlice(buf[0..clen]) catch unreachable;
                    }
                    ab.append(p[j]) catch unreachable;
                }
            }
        }
        ab.appendSlice("\x1b[39m") catch unreachable;
        ab.appendSlice("\x1b[0K") catch unreachable;
        ab.appendSlice("\r\n") catch unreachable;
    }

    //     // Create a two rows status. First row:

    //     abAppend(&ab, "\x1b[0K", 4);
    //     abAppend(&ab, "\x1b[7m", 4);
    //     char status[80], rstatus[80];
    //     int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    //                        E.filename, E.numrows, E.dirty ? "(modified)" : "");
    //     int rlen = snprintf(rstatus, sizeof(rstatus),
    //                         "%d/%d", E.rowoff + E.cy + 1, E.numrows);
    //     if (len > E.screencols)
    //         len = E.screencols;
    //     abAppend(&ab, status, len);
    //     while (len < E.screencols)
    //     {
    //         if (E.screencols - len == rlen)
    //         {
    //             abAppend(&ab, rstatus, rlen);
    //             break;
    //         }
    //         else
    //         {
    //             abAppend(&ab, " ", 1);
    //             len++;
    //         }
    //     }
    //     abAppend(&ab, "\x1b[0m\r\n", 6);

    //     // Second row depends on E.statusmsg and the status message update time.

    //     abAppend(&ab, "\x1b[0K", 4);
    //     int msglen = strlen(E.statusmsg);
    //     if (msglen && time(null) - E.statusmsg_time < 5)
    //         abAppend(&ab, E.statusmsg, msglen <= E.screencols ? msglen : E.screencols);

    //     // Put cursor at its current position. Note that the horizontal position
    //     // at which the cursor is displayed may be different compared to 'E.cx'
    //     // because of TABs.

    //     int j;
    //     int cx = 1;
    //     int filerow = E.rowoff + E.cy;
    //     erow *row = (filerow >= E.numrows) ? null : &E.row[filerow];
    //     if (row)
    //     {
    //         for (j = E.coloff; j < (E.cx + E.coloff); j++)
    //         {
    //             if (j < row.size && row.chars[j] == TAB)
    //                 cx += 7 - ((cx) % 8);
    //             cx++;
    //         }
    //     }
    //     snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, cx);
    //     abAppend(&ab, buf, strlen(buf));

}

/// Set an editor status message for the second line of the status, at the
/// end of the screen.
fn editorSetStatusMessage(fmt: [:0]const u8, args: anytype) void {
    // va_list ap;
    // va_start(ap, fmt);
    _ = @call(.{}, c.snprintf, .{ &E.statusmsg[0], E.statusmsg.len, &fmt[0] } ++ args);
    // va_end(ap);
    E.statusmsg_time = c.time(null);
}

// =============================== Find mode ================================

// #define KILO_QUERY_LEN 256

// void editorFind(int fd)
// {
//     char query[KILO_QUERY_LEN + 1] = {0};
//     int qlen = 0;
//     int last_match = -1; // Last line where a match was found. -1 for none.

//     int find_next = 0; // if 1 search next, if -1 search prev.

//     int saved_hl_line = -1; // No saved HL

//     char *saved_hl = null;

// #define FIND_RESTORE_HL                                                            \
//     do                                                                             \
//     {                                                                              \
//         if (saved_hl)                                                              \
//         {                                                                          \
//             memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize); \
//             free(saved_hl);                                                        \
//             saved_hl = null;                                                       \
//         }                                                                          \
//     } while (0)

//     // Save the cursor position in order to restore it later.

//     int saved_cx = E.cx, saved_cy = E.cy;
//     int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

//     while (1)
//     {
//         editorSetStatusMessage(
//             "Search: %s (Use ESC/Arrows/Enter)", query);
//         editorRefreshScreen();

//         int c = editorReadKey(fd);
//         if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE)
//         {
//             if (qlen != 0)
//                 query[--qlen] = '\0';
//             last_match = -1;
//         }
//         else if (c == ESC || c == ENTER)
//         {
//             if (c == ESC)
//             {
//                 E.cx = saved_cx;
//                 E.cy = saved_cy;
//                 E.coloff = saved_coloff;
//                 E.rowoff = saved_rowoff;
//             }
//             FIND_RESTORE_HL;
//             editorSetStatusMessage("");
//             return;
//         }
//         else if (c == ARROW_RIGHT || c == ARROW_DOWN)
//         {
//             find_next = 1;
//         }
//         else if (c == ARROW_LEFT || c == ARROW_UP)
//         {
//             find_next = -1;
//         }
//         else if (isprint(c))
//         {
//             if (qlen < KILO_QUERY_LEN)
//             {
//                 query[qlen++] = c;
//                 query[qlen] = '\0';
//                 last_match = -1;
//             }
//         }

//         // Search occurrence.

//         if (last_match == -1)
//             find_next = 1;
//         if (find_next)
//         {
//             char *match = null;
//             int match_offset = 0;
//             int i, current = last_match;

//             for (i = 0; i < E.numrows; i++)
//             {
//                 current += find_next;
//                 if (current == -1)
//                     current = E.numrows - 1;
//                 else if (current == E.numrows)
//                     current = 0;
//                 match = strstr(E.row[current].render, query);
//                 if (match)
//                 {
//                     match_offset = match - E.row[current].render;
//                     break;
//                 }
//             }
//             find_next = 0;

//             // Highlight

//             FIND_RESTORE_HL;

//             if (match)
//             {
//                 erow *row = &E.row[current];
//                 last_match = current;
//                 if (row.hl)
//                 {
//                     saved_hl_line = current;
//                     saved_hl = malloc(row.rsize);
//                     memcpy(saved_hl, row.hl, row.rsize);
//                     memset(row.hl + match_offset, HL_MATCH, qlen);
//                 }
//                 E.cy = 0;
//                 E.cx = match_offset;
//                 E.rowoff = current;
//                 E.coloff = 0;
//                 // Scroll horizontally as needed.

//                 if (E.cx > E.screencols)
//                 {
//                     int diff = E.cx - E.screencols;
//                     E.cx -= diff;
//                     E.coloff += diff;
//                 }
//             }
//         }
//     }
// }

// ========================= Editor events handling  ========================

/// Handle cursor position change because arrow keys were pressed.
// void editorMoveCursor(int key)
// {
//     int filerow = E.rowoff + E.cy;
//     int filecol = E.coloff + E.cx;
//     int rowlen;
//     erow *row = (filerow >= E.numrows) ? null : &E.row[filerow];

//     switch (key)
//     {
//     case ARROW_LEFT:
//         if (E.cx == 0)
//         {
//             if (E.coloff)
//             {
//                 E.coloff--;
//             }
//             else
//             {
//                 if (filerow > 0)
//                 {
//                     E.cy--;
//                     E.cx = E.row[filerow - 1].size;
//                     if (E.cx > E.screencols - 1)
//                     {
//                         E.coloff = E.cx - E.screencols + 1;
//                         E.cx = E.screencols - 1;
//                     }
//                 }
//             }
//         }
//         else
//         {
//             E.cx -= 1;
//         }
//         break;
//     case ARROW_RIGHT:
//         if (row && filecol < row.size)
//         {
//             if (E.cx == E.screencols - 1)
//             {
//                 E.coloff++;
//             }
//             else
//             {
//                 E.cx += 1;
//             }
//         }
//         else if (row && filecol == row.size)
//         {
//             E.cx = 0;
//             E.coloff = 0;
//             if (E.cy == E.screenrows - 1)
//             {
//                 E.rowoff++;
//             }
//             else
//             {
//                 E.cy += 1;
//             }
//         }
//         break;
//     case ARROW_UP:
//         if (E.cy == 0)
//         {
//             if (E.rowoff)
//                 E.rowoff--;
//         }
//         else
//         {
//             E.cy -= 1;
//         }
//         break;
//     case ARROW_DOWN:
//         if (filerow < E.numrows)
//         {
//             if (E.cy == E.screenrows - 1)
//             {
//                 E.rowoff++;
//             }
//             else
//             {
//                 E.cy += 1;
//             }
//         }
//         break;
//     }
//     // Fix cx if the current line has not enough chars.

//     filerow = E.rowoff + E.cy;
//     filecol = E.coloff + E.cx;
//     row = (filerow >= E.numrows) ? null : &E.row[filerow];
//     rowlen = row ? row.size : 0;
//     if (filecol > rowlen)
//     {
//         E.cx -= filecol - rowlen;
//         if (E.cx < 0)
//         {
//             E.coloff += E.cx;
//             E.cx = 0;
//         }
//     }
// }

const KILO_QUIT_TIMES = 3;
/// Process events arriving from the standard input, which is, the user
/// is typing stuff on the terminal.
fn editorProcessKeypress(fd: i32) void {
    // When the file is modified, requires Ctrl-q to be pressed N times
    // before actually quitting.

    const S = struct {
        var quit_times: i32 = KILO_QUIT_TIMES;
    };

    const ch = editorReadKey(fd);
    switch (ch) {
        // Enter
        KEY_ACTION.ENTER => {
            editorInsertNewline();
        },
        // Ctrl-c
        KEY_ACTION.CTRL_C => {
            // We ignore ctrl-c, it can't be so simple to lose the changes
            // to the edited file.
            running = false;
        },
        //     case CTRL_Q: // Ctrl-q

        //         // Quit if the file was already saved.

        //         if (E.dirty && quit_times)
        //         {
        //             editorSetStatusMessage("WARNING!!! File has unsaved changes. "
        //                                    "Press Ctrl-Q %d more times to quit.",
        //                                    quit_times);
        //             quit_times--;
        //             return;
        //         }
        //         exit(0);
        //         break;
        //     case CTRL_S: // Ctrl-s

        //         editorSave();
        //         break;
        //     case CTRL_F:
        //         editorFind(fd);
        //         break;
        //     case BACKSPACE: // Backspace

        //     case CTRL_H: // Ctrl-h

        //     case DEL_KEY:
        //         editorDelChar();
        //         break;
        //     case PAGE_UP:
        //     case PAGE_DOWN:
        //         if (c == PAGE_UP && E.cy != 0)
        //             E.cy = 0;
        //         else if (c == PAGE_DOWN && E.cy != E.screenrows - 1)
        //             E.cy = E.screenrows - 1;
        //         {
        //             int times = E.screenrows;
        //             while (times--)
        //                 editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        //         }
        //         break;

        //     case ARROW_UP:
        //     case ARROW_DOWN:
        //     case ARROW_LEFT:
        //     case ARROW_RIGHT:
        //         editorMoveCursor(c);
        //         break;
        //     case CTRL_L: // ctrl+l, clear screen

        //         // Just refresht the line as side effect.

        //         break;
        //     case ESC:
        //         // Nothing to do for ESC in this mode.

        //         break;
        //     default:
        //         editorInsertChar(c);
        //         break;
        else => unreachable,
    }

    S.quit_times = KILO_QUIT_TIMES; // Reset it to the original value.
}

// int editorFileWasModified(void)
// {
//     return E.dirty;
// }

fn updateWindowSize() void {
    getWindowSize(c.STDIN_FILENO, c.STDOUT_FILENO, &E.screenrows, &E.screencols) catch {
        c.perror("Unable to query the screen for size (columns / rows)");
        c.exit(1);
    };
    E.screenrows -= 2; // Get room for status bar.
}

fn handleSigWinCh(_: c_int) callconv(.C) void {
    updateWindowSize();
    if (E.cy > E.screenrows)
        E.cy = @intCast(i32, E.screenrows) - 1;
    if (E.cx > E.screencols)
        E.cx = @intCast(i32, E.screencols) - 1;
    editorRefreshScreen();
}

pub fn main() anyerror!void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    allocator = gpa.allocator();
    defer std.debug.assert(!gpa.deinit());

    E = EditorConfig.init(allocator);
    defer E.deinit();

    var it = std.process.ArgIterator.init();
    // skip arg0
    _ = it.next();
    const arg = it.next() orelse {
        _ = c.fprintf(c.stderr, "Usage: kilo <filename>\n");
        c.exit(1);
    };

    updateWindowSize();
    _ = c.signal(c.SIGWINCH, handleSigWinCh);
    editorSelectSyntaxHighlight(arg);
    //     editorOpen(argv[1]);
    try enableRawMode(c.STDIN_FILENO);
    defer disableRawMode(c.STDIN_FILENO);

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find", .{});
    while (running) {
        editorRefreshScreen();
        editorProcessKeypress(c.STDIN_FILENO);
    }
}
