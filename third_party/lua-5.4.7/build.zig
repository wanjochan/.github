const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Lua core library (shared)
    const lua_lib = b.addSharedLibrary(.{
        .name = "lua5.4",
        .target = target,
        .optimize = optimize,
    });

    // Add all Lua core source files (excluding lua.c and luac.c)
    const lua_core_sources = [_][]const u8{
        "src/lapi.c",
        "src/lauxlib.c",
        "src/lbaselib.c",
        "src/lcode.c",
        "src/lcorolib.c",
        "src/lctype.c",
        "src/ldblib.c",
        "src/ldebug.c",
        "src/ldo.c",
        "src/ldump.c",
        "src/lfunc.c",
        "src/lgc.c",
        "src/linit.c",
        "src/liolib.c",
        "src/llex.c",
        "src/lmathlib.c",
        "src/lmem.c",
        "src/loadlib.c",
        "src/lobject.c",
        "src/lopcodes.c",
        "src/loslib.c",
        "src/lparser.c",
        "src/lstate.c",
        "src/lstring.c",
        "src/lstrlib.c",
        "src/ltable.c",
        "src/ltablib.c",
        "src/ltm.c",
        "src/lundump.c",
        "src/lutf8lib.c",
        "src/lvm.c",
        "src/lzio.c",
    };

    for (lua_core_sources) |src| {
        lua_lib.addCSourceFile(.{
            .file = b.path(src),
            .flags = &[_][]const u8{
                "-std=c99",
                "-O2",
                "-Wall",
                "-DLUA_USE_POSIX",
                "-DLUA_USE_DLOPEN",
            },
        });
    }

    lua_lib.linkLibC();
    if (target.result.os.tag != .windows) {
        lua_lib.linkSystemLibrary("m"); // Math library
        lua_lib.linkSystemLibrary("dl"); // Dynamic loading
    }

    b.installArtifact(lua_lib);

    // Also create a static library version
    const lua_static = b.addStaticLibrary(.{
        .name = "lua5.4",
        .target = target,
        .optimize = optimize,
    });

    for (lua_core_sources) |src| {
        lua_static.addCSourceFile(.{
            .file = b.path(src),
            .flags = &[_][]const u8{
                "-std=c99",
                "-O2",
                "-Wall",
                "-DLUA_USE_POSIX",
            },
        });
    }

    lua_static.linkLibC();
    b.installArtifact(lua_static);
}
