const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .target = target,
        .optimize = optimize,
        .name = "kilo",
        // .root_source_file = b.path("src/main.zig"),
    });

    const flags: []const []const u8 = if (target.result.os.tag == .windows)
        &.{
            // for getline
            // "-std=gnu89",
            // "-D_GNU_SOURCE",
        }
    else
        &.{};

    const srcs = [_][]const u8{
        "repos/taidanh/kilo.c",
    };
    const platform = if (target.result.os.tag == .windows)
        [_][]const u8{
            "repos/_mod/platform_win32.c",
        }
    else
        [_][]const u8{
            "repos/_mod/platform_linux.c",
        };

    exe.addCSourceFiles(.{
        // .root = b.path("kilo"),
        .files = &(srcs ++ platform),
        .flags = flags,
    });
    exe.linkLibC();
    exe.addIncludePath(b.path("repos/_mod"));
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const exe_unit_tests = b.addTest(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    const run_exe_unit_tests = b.addRunArtifact(exe_unit_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_exe_unit_tests.step);
}
