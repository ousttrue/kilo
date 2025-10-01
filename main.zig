const std = @import("std");
const c = @cImport({
    @cInclude("kilo.h");
    @cInclude("platform.h");
});

pub fn main() void {
    _ = c.enableRawMode(0);

    var E = c.editorConfig{};

    c.initEditor(&E);
    if (std.os.argv.len >= 2) {
        c.editorOpen(&E, std.os.argv[1]);
    }

    c.editorSetStatusMessage(&E, "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (true) {
        c.editorRefreshScreen(&E);
        if (E.mode == c.MODE_NORMAL) {
            c.editorNormalProcessKeypress(&E);
        } else {
            c.editorProcessKeypress(&E);
        }
    }
}
