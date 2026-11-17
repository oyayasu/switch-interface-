# Switch Interface for Eden

A source-level UI mod for the [Eden emulator](https://github.com/eden-emulator/mirror), with a home screen modeled on the Nintendo Switch 2 interface.

**Status: alpha / source only.** This is a native Qt Widgets integration into an Eden fork, not a replacement emulator and not a downloadable precompiled emulator. The home screen is functional for browsing titles from Eden's game model, launching games, adding a game directory, opening settings, and returning to the native UI. Keyboard/mouse navigation is supported. The visual layout is a first approximation, **not yet a pixel-exact reproduction** of Switch 2. Controller input, pixel-accurate visuals, profiles, all submenus, and complete automation of cross-version compatibility remain future work.

## Quick start

Requires Python 3.10+ and an Eden source checkout with submodules.

```bash
git clone --recursive https://github.com/eden-emulator/mirror.git eden
python tools/patch_eden.py --source eden --output eden-custom
```

This **copies** the source and injects `switch_home.cpp/.h` into `src/yuzu`. It never modifies the input directory. Run the resulting tree through [Eden's build guide](https://github.com/eden-emulator/mirror/blob/master/docs/Build.md), or add `--build` if your CMake toolchain and dependencies are installed.

ZIP input is supported:

```bash
python tools/patch_eden.py --source eden-source.zip --output eden-custom.zip
```

**Important:** GitHub-generated source ZIP files generally omit Git submodules. These are **not sufficient for a full Eden compilation**. For reliable builds, use `git clone --recursive`; do not download or run arbitrary third-party archives.

## Windows graphical patcher

The repository includes a small desktop program:

- \`tools/patcher_gui.py\` — run with Python 3.10+ and Tkinter using \`python tools/patcher_gui.py\`.
- **Windows .exe** — GitHub Actions builds \`Eden-UI-Patcher.exe\` from the \`Windows UI Patcher\` workflow. Download the \`Eden-UI-Patcher-Windows\` artifact from the run once the workflow succeeds.

Use the **ZIP** or **Folder** selector for the unmodified Eden source. Select an unused output ZIP or folder, then click **Patch Eden**. Checking **Compile** invokes CMake locally and needs the Eden development toolchain; the patcher executable does **not** contain those compiler dependencies.

The standalone patcher compiles the *patcher itself* on GitHub Actions. It does **not** build a ready-to-play Eden executable in CI. Compiling the modified emulator is a separate operation.

## Controls

- Left/Right: choose game or toolbar item
- Down/Up: switch between games and toolbar
- Enter/Space: launch or activate
- Mouse: click games or buttons
- F12: toggle the original Eden library and the custom home screen
- Escape: go back to games from toolbar

## Architecture

- `switch_ui/switch_home.{h,cpp}`: native Qt shell reading game names, icons, and file paths directly from Eden's existing game model.
- `tools/patch_eden.py`: deterministic, idempotent, fail-closed source patcher for the currently inspected Eden Qt layout.
- `tests/`: regression tests for source injection and safety.
- `.github/workflows/`: patcher test checks.

This first version deliberately retains Eden's real emulation engine, configuration screens, and game loading path. It does not alter firmware, keys, or saved games. If upstream rewrites patched integration points, the patcher must refuse to patch rather than silently producing a broken build.

Eden is GPL-3.0-or-later; modified Eden distributions must comply with the applicable license conditions. Nintendo marks, assets, and services belong to Nintendo and are not included here. This is an unofficial community project.
