pub usingnamespace @cImport({
    @cInclude("time.h"); // time_t
    @cInclude("sys/ioctl.h");
    @cInclude("unistd.h");
    @cInclude("stdio.h"); // sscanf
    @cInclude("string.h"); // strlen
    @cInclude("stdlib.h"); // exit
    @cInclude("signal.h");
});
