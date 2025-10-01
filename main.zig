const std = @import("std");
const c = @cImport({
    @cInclude("kilo.h");
    @cInclude("platform.h");
});

pub fn main() void {
    _ = c.enableRawMode(0);
    c.initEditor();
    if (std.os.argv.len >= 2) {
        c.editorOpen(std.os.argv[1]);
    }

    c.editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (true) {
        c.editorRefreshScreen();
        if (c.E.mode == c.MODE_NORMAL) {
            c.editorNormalProcessKeypress();
        } else {
            c.editorProcessKeypress();
        }
    }
}
