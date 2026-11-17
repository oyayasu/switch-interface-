# Switch Interface for Eden

A source-level UI mod for the [Eden emulator](https://github.com/eden-emulator/mirror), with a home screen modeled on the Nintendo Switch 2 interface.

**Status: alpha / source only.** This project injects a custom Qt Widgets home shell into an Eden source tree. It now includes a console-style **Controllers** screen modeled on the supplied Switch 2 reference, plus console-native **System Settings**, **Library**, **User Settings**, and **Power** screens.

**Controllers and System Settings no longer open Eden's native Configure dialog from the home toolbar.** The native dialog is retained internally for emulator maintenance but is not exposed by those buttons. F12 remains an advanced emergency switch back to Eden's original library.

The controller page shows Eden's configured/assigned player count (up to four indicators) and provides familiar console navigation, but it does **not** currently change controller assignments, initiate Bluetooth pairing, or report physical battery status. The Search page updates the existing Eden state; no unimplemented pairing functionality is claimed.

Home navigation uses keyboard/mouse and SDL3 gamepad d-pad/left-stick plus South/East face buttons (confirm/back). The appearance is an ongoing recreation, not a guaranteed pixel-exact reproduction. Advanced console-only features are presented as unavailable inside the custom shell instead of opening an unrelated desktop window.

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

## Switch 2 home visual update (v0.2)

The home UI was redesigned using the user's five supplied screenshot references:

- light gray, 1280 × 720 reference layout with a horizontal row of full-size game covers;
- ten bottom navigation icons in the same positions/order as the Switch 2 reference;
- 12-hour clock, Wi-Fi and battery symbols at upper right;
- two black Joy-Con symbols at bottom left, with four cells reflecting the number of **configured player inputs** (green) in Eden, limited to four;
- black circular **A** button and **OK** label at bottom right;
- upper-left profile photo ring animated with a changing multicolor gradient, which does not rotate the photo;
- click the profile to import a picture using the native file chooser, rename, or clear it. The name and a locally saved PNG copy persist in the user's own application settings directory.

Controls are based on Eden's \`ControllerConsoleState\` when the console-mode assignment engine is active, and \`Settings::values.players\` otherwise. Unavailable Nintendo services show a message instead of pretending to work.

**Visual fidelity note:** the screenshot-faithful browser preview uses image crops supplied by the user. The current in-emulator Qt implementation draws the toolbar glyphs as vectors; their shapes are close, but the current native executable **does not yet bundle exact pixel artwork from those screenshots**. Nintendo artwork, network services, and battery telemetry are not implemented by this project. The native build is not yet verified.

## Windows graphical patcher

The repository includes a small desktop program:

- \`tools/patcher_gui.py\` — run with Python 3.10+ and Tkinter using \`python tools/patcher_gui.py\`.
- **Windows .exe** — GitHub Actions builds \`Eden-UI-Patcher.exe\` from the \`Windows UI Patcher\` workflow. Download the \`Eden-UI-Patcher-Windows\` artifact from the run once the workflow succeeds.

Use the **ZIP** or **Folder** selector for the unmodified Eden source. Select an unused output ZIP or folder, then click **Patch Eden**. Checking **Compile** invokes CMake locally and needs the Eden development toolchain; the patcher executable does **not** contain those compiler dependencies.

The standalone patcher compiles the *patcher itself* on GitHub Actions. It does **not** build a ready-to-play Eden executable in CI. Compiling the modified emulator is a separate operation.

## Controls

- Home: Left/Right to select games or toolbar actions, Up/Down to switch groups
- Inside pages: Up/Down and Enter/A navigate console-style menus
- Escape, Backspace or B: go back one level, then Home
- SDL3 controller: D-pad / left stick, South to confirm, East to go back (physical button labels may differ by controller)
- Mouse: select games, enter Controllers, navigate menu options and use on-screen A / B
- Click the avatar or press P: open a console-style profile page; photo chooser uses Windows Explorer, name editor is inline
- F12: advanced fallback between the home shell and Eden's original game list

## Current boundaries

- Controllers **uses the real Eden assignment/configuration state**, not a simulated count in the native build. The slider in the browser preview is demo-only.
- `Search for Controllers` does not force Bluetooth discovery; it refreshes already recognized devices.
- `Change Grip/Order`, Nintendo network services and many advanced emulator settings are not implemented in the custom interface yet.
- The Wi-Fi and battery icons in the Home bar are visual placeholders. Games still start through Eden's existing boot path.

## Architecture

- `switch_ui/switch_home.{h,cpp}`: native Qt shell reading game names, icons, and file paths directly from Eden's existing game model.
- `tools/patch_eden.py`: deterministic, idempotent, fail-closed source patcher for the currently inspected Eden Qt layout.
- `tests/`: regression tests for source injection and safety.
- `.github/workflows/`: patcher test checks.

This first version deliberately retains Eden's real emulation engine, configuration screens, and game loading path. It does not alter firmware, keys, or saved games. If upstream rewrites patched integration points, the patcher must refuse to patch rather than silently producing a broken build.

Eden is GPL-3.0-or-later; modified Eden distributions must comply with the applicable license conditions. Nintendo marks, assets, and services belong to Nintendo and are not included here. This is an unofficial community project.
