"""Builds for Linux from Windows, with Zig's C++ compiler as the cross-compiler.

    py Tools/build_linux.py [--zig <folder or zig.exe>] [--arch x86_64|aarch64|both] [--out <folder>] [--game] [--check] [--force]

The headless server and the pack tool are plain C++ over the shared code (cmake/Sources.cmake's
lists), so they are compiled file by file for x86_64-linux-gnu / aarch64-linux-gnu (glibc 2.28 and
later: Debian 10, Ubuntu 18.10, RHEL 8 and newer) and linked into

    <out>/LegacySFServer   <out>/lsfpack          (x86_64)
    <out>/arm64/...                               (aarch64)

--game builds the game too, <out>/legacysf (x86_64 only: the Linux PCs that play are x86_64). That
is the client's sources with Client/Source/Engine/Platform/Window_linux.cpp and Client/Desktop/main.cpp,
"This PC"'s server (PF-2), VanGUI compiled from its own source (Projects/VanGUI, as for Android) and
the shaders embedded by Tools/shaders_gen.py. X11 and ALSA are opened at run time; EGL and OpenGL ES
are linked against stubs made here (libEGL.so.1, libGLESv2.so.2: names and symbols only), so the
system's own (Mesa, or the card maker's through libglvnd) are the ones that run.

--check compiles everything and links nothing (a quick "does it still build for Linux"). --force
compiles every file again. A file is also compiled again when any of the project's own headers is
newer than its object (headers are not followed one by one).

Zig is a single folder with no installer (https://ziglang.org/download/): name it with --zig, or
put it on PATH, or set ZIG. Nothing here can run what it builds: that takes a Linux machine
(Docs/UniversalServerDeploy.md, section 17's "server builds"). One build at a time: two Zigs at
once trip over each other's cache.
"""
import concurrent.futures
import hashlib
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VANGUI_SRC = os.path.join(os.path.dirname(os.path.dirname(ROOT)), "Projects", "VanGUI")
VANGUI_CORE = ["src/vangui.cpp", "src/vangui_draw.cpp", "src/vangui_widgets.cpp", "src/vangui_tables.cpp", "src/vangui_docking.cpp"]
VANGUI_SUITE = """anim loading notify command_palette themes node_graph layout dialogs style_sheet views widgets_ext charts
feedback forms panels shortcuts vui theme_engine theme_editor thread actions toolbar dropzone wizard console property_grid
curve_editor datetime markdown hex_editor plot timeline data_grid banners repl richtext perf_hud vector suite_selfcheck raster
svg effects icons gallery extras touch gizmo""".split()
# VanGUI's frozen macro set (Client/Android/CMakeLists.txt's, vendor/VanGUI/cmake's).
VANGUI_DEFS = """ANIM LOADING NOTIFY COMMAND_PALETTE THEMES NODE_GRAPH LAYOUT DIALOGS STYLESHEET SIGNALS VIEWS WIDGETS_EXT CHARTS
FEEDBACK FORMS PANELS SHORTCUTS VUI THEME_ENGINE THEME_EDITOR THREAD ACTIONS TOOLBAR DROPZONE WIZARD CONSOLE PROPERTY_GRID
CURVE_EDITOR DATETIME MARKDOWN HEX_EDITOR PLOT TIMELINE DATA_GRID BANNERS REPL RICHTEXT PERF_HUD VECTOR RASTER SVG EFFECTS
ICONS GALLERY EXTRAS TOUCH GIZMO""".split()


def source_lists():
    text = open(os.path.join(ROOT, "cmake", "Sources.cmake"), encoding="utf-8").read()
    lists = {}
    for name, body in re.findall(r"set\((LSF_[A-Z_]+)\s+([^)]*)\)", text):
        files = []
        for token in body.split():
            if token.startswith("${LSF_ROOT}/"):
                files.append(token[len("${LSF_ROOT}/"):])
            elif token.startswith("${LSF_") and token.endswith("}"):
                files += lists.get(token[2:-1], [])
        lists[name] = files
    return lists


def find_zig(arg):
    for c in [arg, os.environ.get("ZIG"), shutil.which("zig")]:
        if not c:
            continue
        if os.path.isdir(c):
            c = os.path.join(c, "zig.exe" if os.name == "nt" else "zig")
        if os.path.isfile(c):
            return c
    return None


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return p.returncode, (p.stdout + p.stderr).strip()


def gl_stubs(zig, target, objdir):
    """libEGL.so.1 and libGLESv2.so.2 holding every function the Khronos headers declare, empty:
    linked against, never run (the system's own libraries answer at run time)."""
    khronos = os.path.join(ROOT, "vendor", "Khronos", "include")
    out = {}
    for lib, soname, headers, pattern in (
        ("EGL", "libEGL.so.1", ["EGL/egl.h"], r"EGLAPI\s+[\w\s\*]+?EGLAPIENTRY\s+(egl\w+)\s*\("),
        ("GLESv2", "libGLESv2.so.2", ["GLES2/gl2.h", "GLES3/gl3.h", "GLES3/gl31.h", "GLES3/gl32.h"],
         r"GL_APICALL\s+[\w\s\*]+?GL_APIENTRY\s+(gl\w+)\s*\("),
    ):
        names = set()
        for h in headers:
            path = os.path.join(khronos, h)
            if os.path.exists(path):
                names.update(re.findall(pattern, open(path, encoding="utf-8", errors="replace").read()))
        src = os.path.join(objdir, f"stub_{lib}.c")
        with open(src, "w", encoding="utf-8") as f:
            f.write("/* A link stub: names only (the system's library runs). */\n")
            for n in sorted(names):
                f.write(f"void {n}(void) {{}}\n")
        so = os.path.join(objdir, soname)
        code, text = run([zig, "cc", "-target", target, "-shared", "-fPIC", f"-Wl,-soname,{soname}", "-o", so, src])
        if code != 0:
            print(text[:2000])
            raise SystemExit(f"the {soname} stub did not build")
        out[lib] = so
    return out


def build(zig, arch, out, check, lists, game):
    target = f"{arch}-linux-gnu.2.28"
    objdir = os.path.join(ROOT, "build-linux", arch)
    os.makedirs(objdir, exist_ok=True)
    cxx = ["-target", target, "-std=c++23", "-O2", "-DNDEBUG", "-pthread", "-fno-sanitize=undefined", "-Wno-deprecated-declarations",
           "-Wno-date-time"]
    server_inc = ["-I", os.path.join(ROOT, "Shared"), "-I", os.path.join(ROOT, "vendor", "SFSDK", "include"),
                  "-I", os.path.join(ROOT, "vendor", "stb"), "-I", os.path.join(ROOT, "Server", "Source")]

    shared = lists["LSF_SHARED_SOURCES"]
    core = lists["LSF_SERVER_CORE_SOURCES"]
    server = lists["LSF_SERVER_SOURCES"]
    units = {}   # source (absolute) -> its flags

    def add(files, flags):
        for f in files:
            units.setdefault(f, flags)

    absolute = lambda files: [os.path.join(ROOT, f) for f in files]
    add(absolute(shared + core + server + ["Tools/lsfpack/main.cpp"]), cxx + server_inc)
    programs = {
        "LegacySFServer": absolute(shared + core + server),
        "lsfpack": absolute(shared + ["Tools/lsfpack/main.cpp"]),
    }
    if game and arch == "x86_64":
        if not os.path.isdir(VANGUI_SRC):
            raise SystemExit(f"VanGUI's source is not at {VANGUI_SRC}")
        gen = os.path.join(objdir, "generated")
        os.makedirs(gen, exist_ok=True)
        shaders = os.path.join(gen, "Shaders.cpp")
        code, text = run([sys.executable, os.path.join(ROOT, "Tools", "shaders_gen.py"), shaders,
                          os.path.join(ROOT, "Client", "Source", "Shaders"), "world,post", os.path.join(ROOT, "vendor", "ShaderTools")])
        if code != 0:
            print(text[-3000:])
            raise SystemExit("the shaders did not embed")
        compat = ["-include", os.path.join(ROOT, "Client", "Android", "cmake", "vangui_compat.h")]
        vg_inc = ["-I", os.path.join(VANGUI_SRC, "include"), "-I", os.path.join(VANGUI_SRC, "misc"),
                  "-I", os.path.join(VANGUI_SRC, "include", "vangui"), "-I", os.path.join(VANGUI_SRC, "src")]
        defs = [f"-DVANGUI_ENABLE_{d}" for d in VANGUI_DEFS]
        vg_core = [os.path.join(VANGUI_SRC, f) for f in VANGUI_CORE]
        vg_suite = [os.path.join(VANGUI_SRC, "misc", f"vangui_{m}.cpp") for m in VANGUI_SUITE]
        # The core with the animation hook only (VanGUI's CMake); the modules with the whole set.
        add(vg_core, cxx + compat + ["-DVANGUI_ENABLE_ANIM", "-I", VANGUI_SRC] + vg_inc)
        add(vg_suite, cxx + compat + defs + vg_inc)
        vg = os.path.join(ROOT, "vendor", "VanGUI")
        game_flags = cxx + server_inc + defs + ["-DLSF_WITH_SERVER=1", "-I", os.path.join(ROOT, "Client", "Source"),
                                               "-I", os.path.join(ROOT, "vendor", "minimp3"), "-I", os.path.join(vg, "include"),
                                               "-I", os.path.join(vg, "include", "vangui"), "-I", os.path.join(vg, "backends"),
                                               "-I", os.path.join(ROOT, "vendor", "Khronos", "include")]
        backend = [os.path.join(vg, "backends", "vangui_impl_opengl3.cpp")]
        add(backend, game_flags + ["-DVANGUI_IMPL_OPENGL_ES3"])
        game_files = absolute(lists["LSF_ENGINE_CLIENT_SOURCES"] + lists["LSF_CLIENT_SOURCES"] +
                              ["Client/Source/Engine/Platform/Window_linux.cpp", "Client/Desktop/main.cpp"]) + [shaders]
        add(game_files, game_flags)
        programs["legacysf"] = absolute(shared + core) + game_files + backend + vg_core + vg_suite

    # Headers are not followed file by file: an object older than any of the project's own headers
    # is compiled again (a changed declaration once linked a stale object against a symbol that no
    # longer existed).
    newest_header = 0.0
    for folder in ("Shared", os.path.join("Server", "Source"), os.path.join("Client", "Source"), os.path.join("Client", "Desktop"), "Tools"):
        for dirpath, _, names in os.walk(os.path.join(ROOT, folder)):
            for n in names:
                if n.endswith((".hpp", ".h", ".inl")):
                    newest_header = max(newest_header, os.path.getmtime(os.path.join(dirpath, n)))

    def compile_one(src):
        rel = os.path.relpath(src, ROOT)
        obj = os.path.join(objdir, hashlib.sha1(rel.replace("\\", "/").encode()).hexdigest()[:10] + "-" + os.path.basename(src) + ".o")
        # Up to date: the object is newer than its source and than every header of ours.
        if os.path.exists(obj) and os.path.getmtime(obj) > max(os.path.getmtime(src), newest_header) and "--force" not in sys.argv:
            return src, obj, 0, ""
        code, text = run([zig, "c++"] + units[src] + ["-c", src, "-o", obj])
        return src, obj, code, text

    objects = {}
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(2, (os.cpu_count() or 4) - 1)) as pool:
        for src, obj, code, text in pool.map(compile_one, list(units)):
            objects[src] = obj
            if code != 0:
                failed += 1
                errors = [l for l in text.splitlines() if " error: " in l or " fatal error: " in l]
                print(f"  FAILED {os.path.relpath(src, ROOT)}")
                for l in (errors or text.splitlines())[:12]:
                    print("      " + l[:300])
    print(f"{arch}: {len(units) - failed} of {len(units)} files compile for Linux")
    if failed or check:
        return failed
    os.makedirs(out, exist_ok=True)
    stubs = gl_stubs(zig, target, objdir) if "legacysf" in programs else {}
    for name, files in programs.items():
        exe = os.path.join(out, name)
        libs = ["-ldl"]
        if name == "legacysf":
            # The stubs by path: the program asks for their sonames, which the system provides.
            libs = [stubs["EGL"], stubs["GLESv2"], "-ldl", "-lm"]
        # -s: no symbols in what ships (the crash handler names the signal, not the function: the
        # objects in build-linux/ keep everything for a closer look).
        code, text = run([zig, "c++", "-target", target, "-O2", "-pthread", "-s", "-o", exe] + [objects[f] for f in files] + libs)
        if code != 0:
            failed += 1
            print(f"  FAILED linking {name}")
            for l in text.splitlines()[:40]:
                print("      " + l[:300])
        else:
            print(f"  {exe}  ({os.path.getsize(exe) / 1048576.0:.1f} MB)")
    return failed


def main():
    args = sys.argv[1:]
    zig_arg, arch, out, check, game = None, "x86_64", os.path.join(ROOT, "bin", "linux"), False, False
    while args:
        a = args.pop(0)
        if a == "--zig":
            zig_arg = args.pop(0)
        elif a == "--arch":
            arch = args.pop(0)
        elif a == "--out":
            out = args.pop(0)
        elif a == "--check":
            check = True
        elif a == "--game":
            game = True
    zig = find_zig(zig_arg)
    if not zig:
        print("Zig was not found: name its folder with --zig, set ZIG, or put it on PATH (https://ziglang.org/download/).")
        return 2
    lists = source_lists()
    failed = 0
    for a in (["x86_64", "aarch64"] if arch == "both" else [arch]):
        failed += build(zig, a, out if a == "x86_64" else os.path.join(out, "arm64"), check, lists, game)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
