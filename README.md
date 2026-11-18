# Switch Interface Patcher — Eden / Eglantine / Lichii

A source-level Nintendo Switch 2-style UI modification for [Eden](https://github.com/eden-emulator/mirror), [Eglantine](https://github.com/Eglantine-Community/Eglantine) and [Lichii](https://github.com/zortos293/Lichii).

**Status: alpha / source only.** This project injects a custom Qt Widgets home shell into a compatible source tree, without altering the original checkout. It now includes a console-style **Controllers** screen modeled on the supplied Switch 2 reference, plus console-native **System Settings**, **Library**, **User Settings**, and **Power** screens.

**Controllers and System Settings no longer open Eden's native Configure dialog from the home toolbar.** The native dialog is retained internally for emulator maintenance but is not exposed by those buttons. F12 remains an advanced emergency switch back to Eden's original library.

The controller page shows Eden's configured/assigned player count (up to four indicators) and provides familiar console navigation, but it does **not** currently change controller assignments, initiate Bluetooth pairing, or report physical battery status. The Search page updates the existing Eden state; no unimplemented pairing functionality is claimed.

Home navigation uses keyboard/mouse and SDL3 gamepad d-pad/left-stick with A/B confirmation, using physical face-button labels when SDL recognizes a Nintendo layout. The appearance is an ongoing recreation, not a guaranteed pixel-exact reproduction. Advanced console-only features are presented as unavailable inside the custom shell instead of opening an unrelated desktop window.

## Windows installer (v0.4.0)

**[Windows workflow and downloadable installer artifact](https://github.com/oyayasu/switch-interface-/actions/workflows/windows-patcher.yml)**

The GitHub Actions workflow creates two Windows 64-bit artifacts:
- `Switch-Interface-Setup.exe`: standard Windows installer, built with PyInstaller and Inno Setup.
- `Switch-Interface-Patcher.exe`: portable patcher, without installation.

The installer installs **the source-code patching application**, not a modified emulator. It does not patch already compiled emulator EXEs or replace the system's installed emulators.

After installation: open the Patcher, choose the **ZIP** or **Folder** containing the unmodified source for Eden/Eglantine/Lichii, choose a new output ZIP or folder, and click **Apply Switch Interface**. Automatic detection uses:

| Source project | Default source branch | Frontend target |
| --- | --- | --- |
| Eden | `master` | `src/yuzu` · `yuzu` |
| Eglantine | `nx2` | `src/yuzu` · `yuzu` |
| Lichii | `nx2` | `src/lichii` · `lichii` |

For command-line use:

```bash
# Using a ZIP of source code
python tools/patch_eden.py --source emulator-source.zip --output custom-source.zip

# Or use a Git clone with submodules if you intend to compile the result
git clone --recursive -b nx2 https://github.com/zortos293/Lichii.git lichii
python tools/patch_eden.py --source lichii --output lichii-custom
```

**Compilation note:** ZIP downloads typically omit required Git submodules. To compile an emulator, clone with submodules and install that emulator's Qt/CMake/compiler dependencies. Checking **Compile** delegates to local CMake; the installer does not bundle Visual Studio, Qt, Vulkan SDK or the emulators themselves.

CI includes source-level smoke tests against the current default branches of all three projects. They validate injection points, not native runtime behavior: **a complete patched-emulator build and launch has not yet been verified for each fork**.

## Switch 2 home visual update (v0.2)

The home UI was redesigned using the user's five supplied screenshot references:

- light gray, 1280 × 720 reference layout with a horizontal row of full-size game covers;
- ten bottom navigation icons in the same positions/order as the Switch 2 reference;
- 12-hour clock, Wi-Fi and battery symbols at upper right;
- two black Joy-Con symbols at bottom left, with four cells reflecting the number of **configured player inputs** (green) in Eden, limited to four;
- black circular **A** button and **OK** label at bottom right;
- upper-left profile photo ring animated with a changing multicolor gradient, which does not rotate the photo;
- click the profile to import a picture using the native file chooser, rename, or clear it. The name and a locally saved PNG copy persist in the user's own application settings directory.

Controls use Eden's `ControllerConsoleState` when that API is available, with a fallback to configured emulated player slots for Eglantine and Lichii. Unavailable Nintendo services show a message instead of pretending to work.

**Visual fidelity note:** the screenshot-faithful browser preview uses image crops supplied by the user. The current in-emulator Qt implementation draws the toolbar glyphs as vectors; their shapes are close, but the current native executable **does not yet bundle exact pixel artwork from those screenshots**. Nintendo artwork, network services, and battery telemetry are not implemented by this project. The native build is not yet verified.

## Controls

- Home: Left/Right to select games or toolbar actions, Up/Down to switch groups
- Inside pages: Up/Down and Enter/A navigate console-style menus
- Escape, Backspace or B: go back one level, then Home
- SDL3 controller: D-pad / left stick; A to confirm and B to go back, adapting the layout when SDL identifies Nintendo-style face-button labels
- Mouse: select games, enter Controllers, navigate menu options and use on-screen A / B
- Click the avatar or press P: open a console-style profile page; photo chooser uses Windows Explorer, name editor is inline
- F12: advanced fallback between the home shell and Eden's original game list

## Current boundaries

- Controllers **uses existing emulator assignments/configuration state**, not a simulated count in the native build. The slider in the browser preview is demo-only.
- `Search for Controllers` does not force Bluetooth discovery; it refreshes already recognized devices.
- `Change Grip/Order`, Nintendo network services and many advanced emulator settings are not implemented in the custom interface yet.
- The Wi-Fi and battery icons in the Home bar are visual placeholders. Games still start through Eden's existing boot path.

## Architecture

- `switch_ui/switch_home.{h,cpp}`: native Qt shell reading game names, icons, and file paths directly from Eden's existing game model.
- `tools/patch_eden.py`: deterministic, idempotent, fail-closed source patcher with dedicated frontend/CMake handling for all three projects.
- `tests/`: regression tests for source injection and safety.
- `.github/workflows/`: patcher test checks.

This first version deliberately retains Eden's real emulation engine, configuration screens, and game loading path. It does not alter firmware, keys, or saved games. If upstream rewrites patched integration points, the patcher must refuse to patch rather than silently producing a broken build.

Eden is GPL-3.0-or-later; modified Eden distributions must comply with the applicable license conditions. Nintendo marks, assets, and services belong to Nintendo and are not included here. This is an unofficial community project.
