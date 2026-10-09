# Repository Guidelines

## Project Structure & Module Organization

Core plugin code lives in `src/`; `chkl.l` and `chkl.y` are the Flex/Bison grammar sources. The bundled X-Plane SDK is under `src/SDK/`, so change it only for deliberate SDK upgrades. `checker/` contains the standalone checklist validator and regression fixtures. `simon/` builds the speech helper. Manuals and screenshots belong in `docs/`, while `Xchecklist/` is the distributable plugin layout. CI and packaging are defined in `.github/workflows/`.

## Build, Test, and Development Commands

Use an out-of-tree build directory. Linux requires CMake, a C/C++ toolchain, Flex, Bison, OpenGL/GLUT, OpenAL, and Speech Dispatcher development packages.

```sh
cmake -S src -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

This produces `build/lin.xpl` (platform-specific names are used on Windows and macOS). Build auxiliary tools separately because each directory is its own CMake project:

```sh
cmake -S checker -B build-checker -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-checker
./build-checker/lin_checker_64 checker/clist.txt
cmake -S simon -B build-simon
cmake --build build-simon
```

Run `ctest --test-dir build --output-on-failure` for any CTest cases added to a build tree.

## Coding Style & Naming Conventions

Match the surrounding legacy style: two-space indentation, same-line opening braces, and descriptive `snake_case` identifiers. Use `.h` for headers and `.c`, `.cpp`, or `.m` for implementations. Keep platform branches guarded with `APL`, `IBM`, and `LIN`. CMake enables strict warnings (`-Wall`, `-Wpedantic`, `-Wshadow`, `-Wfloat-equal`, and `-Wextra`); introduce no new warnings. No repository-wide formatter is configured, so preserve local formatting.

## Testing Guidelines

There is no dedicated unit-test framework or coverage threshold. Validate parser and checklist changes with the checker, using `checker/clist.txt` for normal parsing and `checker/regres_test1.txt` for regression walkthroughs. The optional checker bit-field argument enables diagnostics; value `4` runs walkthrough mode. Build all affected targets and, for GUI or plugin changes, test in a supported X-Plane version. Add focused fixtures under `checker/` when fixing parser regressions.

## Commit & Pull Request Guidelines

Recent commits use concise, imperative summaries such as `Fixed typo` and `fix windows instructions in README.md`. Keep each commit scoped to one logical change and mention the affected component when useful. Pull requests should explain behavior changes, list platforms and commands tested, link relevant issues, and include screenshots for GUI changes. Update documentation and change logs when user-visible behavior changes; do not commit generated build directories or packaged binaries.
