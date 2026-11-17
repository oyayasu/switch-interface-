#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reapply Switch Interface to an Eden source tree, without editing the original."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1]))
MARKER = "SWITCH_INTERFACE_PATCH_v1"
SOURCE_FILES = (
    Path("src/yuzu/CMakeLists.txt"),
    Path("src/yuzu/main_window.h"),
    Path("src/yuzu/main_window.cpp"),
)


class PatchError(RuntimeError):
    pass


def replace_exact(contents: str, anchor: str, inserted: str, filename: str) -> str:
    """Fail on a changed upstream anchor, rather than apply a dubious patch."""
    count = contents.count(anchor)
    if count != 1:
        raise PatchError(
            f"{filename}: expected precisely one occurrence of {anchor!r}; found {count}. "
            "This Eden revision needs an integration update."
        )
    return contents.replace(anchor, inserted, 1)


def patch_tree(source: Path, ui_root: Path = ROOT) -> bool:
    """Patch an Eden source tree in place. Caller must work on a COPY.

    Returns False if this exact integration is already present.
    """
    paths = [source / part for part in SOURCE_FILES]
    if not all(path.is_file() for path in paths):
        missing = [str(path) for path in paths if not path.is_file()]
        raise PatchError("Not a supported Eden source checkout. Missing: " + ", ".join(missing))

    texts = [path.read_text(encoding="utf-8") for path in paths]
    flags = [MARKER in content for content in texts]
    if all(flags):
        if not all((source / "src/yuzu" / name).is_file()
                   for name in ("switch_home.cpp", "switch_home.h")):
            raise PatchError("Patch marker is present but switch_home files are missing.")
        return False
    if any(flags):
        raise PatchError("Partial or inconsistent integration found; refusing to continue.")

    cmake, header, cpp = texts
    cmake = replace_exact(
        cmake,
        "add_executable(yuzu\n",
        f"add_executable(yuzu\n    # {MARKER}\n    switch_home.cpp\n    switch_home.h\n",
        "src/yuzu/CMakeLists.txt",
    )
    header = replace_exact(
        header, "class GameList;\n",
        f"class GameList;\nclass SwitchHomeWidget; // {MARKER}\n",
        "src/yuzu/main_window.h",
    )
    header = replace_exact(
        header, "    GameList* game_list = nullptr;\n",
        "    GameList* game_list = nullptr;\n"
        "    SwitchHomeWidget* switch_home = nullptr;\n",
        "src/yuzu/main_window.h",
    )
    cpp = replace_exact(
        cpp, '#include "main_window.h"\n',
        '#include "main_window.h"\n'
        f'#include "switch_home.h" // {MARKER}\n',
        "src/yuzu/main_window.cpp",
    )

    bridge = """    // SWITCH_INTERFACE_PATCH_v1: use Eden's existing game metadata and boot flow.
    switch_home = new SwitchHomeWidget(game_list->GetModel(), ui->centralwidget);
    ui->horizontalLayout->addWidget(switch_home);
    switch_home->onLaunch = [this](const QString& game_path) {
        BootGameFromList(game_path, StartGameType::Normal);
        if (emulation_running) switch_home->hide();
    };
    switch_home->onNativeLibrary = [this] {
        switch_home->hide();
        game_list->show();
        ui->menubar->show();
        statusBar()->show();
        game_list->setFocus();
    };
    switch_home->onAddDirectory = [this] { game_list->AddDirectory(); };
    // Controllers and Settings are now rendered by SwitchHomeWidget itself.
    // Do not bind them to Eden's native Configure dialog.
    switch_home->onClose = [this] { close(); };
    connect(game_list, &GameList::PopulatingCompleted, this, [this] {
        if (switch_home && switch_home->isVisible() && !emulation_running) {
            game_list->hide();
            game_list_placeholder->hide();
            switch_home->raise();
            switch_home->setFocus();
        }
    });
    auto* switch_home_shortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
    connect(switch_home_shortcut, &QShortcut::activated, this, [this] {
        if (emulation_running) return;
        if (switch_home->isVisible()) {
            switch_home->onNativeLibrary();
        } else {
            game_list->hide();
            game_list_placeholder->hide();
            ui->menubar->hide();
            statusBar()->hide();
            switch_home->show();
            switch_home->raise();
            switch_home->setFocus();
        }
    });
    game_list->hide();
    game_list_placeholder->hide();
    ui->menubar->hide();
    statusBar()->hide();
    switch_home->show();
    switch_home->setFocus();
"""
    cpp = replace_exact(
        cpp,
        "    game_list->PopulateAsync(UISettings::values.game_dirs);\n\n"
        "    // Set up game list mode checkboxes.",
        "    // Keep 256px metadata icons for the large Switch-style home tiles.\n"
        "    UISettings::values.game_icon_size.SetValue(256);\n"
        "    game_list->PopulateAsync(UISettings::values.game_dirs);\n\n" + bridge +
        "\n    // Set up game list mode checkboxes.",
        "src/yuzu/main_window.cpp",
    )

    # The game list is restored by Eden when a running game exits. Raise the home
    # screen afterwards; the original emulation/shutdown routines are unchanged.
    cpp = replace_exact(
        cpp,
        "    game_list->SetFilterFocus();\n    tas_label->clear();",
        "    game_list->SetFilterFocus();\n"
        "    // SWITCH_INTERFACE_PATCH_v1: return to the home screen after a game ends.\n"
        "    if (switch_home) {\n"
        "        game_list->hide();\n"
        "        game_list_placeholder->hide();\n"
        "        ui->menubar->hide();\n"
        "        statusBar()->hide();\n"
        "        switch_home->show();\n"
        "        switch_home->raise();\n"
        "        switch_home->setFocus();\n"
        "    }\n"
        "    tas_label->clear();",
        "src/yuzu/main_window.cpp",
    )

    # Only write once every upstream compatibility anchor has been validated.
    additions = ["switch_home.cpp", "switch_home.h"]
    for filename in additions:
        if (source / "src/yuzu" / filename).exists():
            raise PatchError(f"Unexpected existing Eden file: {filename}")
        if not (ui_root / "switch_ui" / filename).is_file():
            raise PatchError(f"Missing local integration source: {filename}")

    for path, contents in zip(paths, (cmake, header, cpp)):
        path.write_text(contents, encoding="utf-8")
    for filename in additions:
        shutil.copy2(ui_root / "switch_ui" / filename, source / "src/yuzu" / filename)
    (source / ".switch-interface.json").write_text(
        json.dumps(
            {
                "name": "Switch Interface for Eden",
                "version": 2,
                "source": "https://github.com/eden-emulator/mirror",
                "note": "A source-code UI patch. This does not verify runtime compatibility.",
            },
            indent=2,
        ) + "\n",
        encoding="utf-8",
    )
    return True


def extract_zip_safely(archive: Path, into: Path) -> Path:
    with zipfile.ZipFile(archive) as zf:
        entries = zf.infolist()
        if len(entries) > 100_000:
            raise PatchError("Archive has too many entries.")
        total_size = sum(entry.file_size for entry in entries)
        if total_size > 12 * 1024**3:
            raise PatchError("Archive exceeds the 12 GiB uncompressed safety limit.")
        for entry in entries:
            name = PurePosixPath(entry.filename.replace("\\", "/"))
            if (name.is_absolute() or ".." in name.parts or
                    not name.parts or name.parts[0] in ("", ".")):
                raise PatchError(f"Unsafe archive path: {entry.filename!r}")
            permissions = (entry.external_attr >> 16) & 0xFFFF
            if stat.S_ISLNK(permissions):
                raise PatchError(f"Symlinks are not allowed in ZIP input: {entry.filename!r}")
            destination = into.joinpath(*name.parts)
            if not destination.resolve().is_relative_to(into.resolve()):
                raise PatchError(f"Archive entry escapes destination: {entry.filename!r}")
            if entry.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(entry) as src, destination.open("wb") as dest:
                shutil.copyfileobj(src, dest)
    if (into / "CMakeLists.txt").is_file():
        return into
    candidates = [child for child in into.iterdir() if child.is_dir()]
    if len(candidates) == 1 and (candidates[0] / "CMakeLists.txt").is_file():
        return candidates[0]
    raise PatchError("Cannot find Eden's root CMakeLists.txt in the archive.")


def pack_zip(root: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for file in sorted(root.rglob("*")):
            if file.is_file():
                zf.write(file, arcname=file.relative_to(root).as_posix())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path,
                        help="Unmodified Eden source directory or source ZIP")
    parser.add_argument("--output", required=True, type=Path,
                        help="New patched directory or .zip (never overwritten)")
    parser.add_argument("--build", action="store_true",
                        help="Run CMake configure/build (requires a prepared toolchain)")
    parser.add_argument("--cmake-arg", action="append", default=[],
                        help="Extra argument passed to CMake configure; repeatable")
    args = parser.parse_args(argv)

    source = args.source.expanduser().resolve()
    output = args.output.expanduser().resolve()
    if not source.exists():
        parser.error(f"Source does not exist: {source}")
    if output.exists():
        parser.error(f"Output already exists (refusing to overwrite): {output}")
    if args.build and output.suffix.lower() == ".zip":
        parser.error("--build requires an output directory, not a ZIP.")
    if source == output or source in output.parents:
        parser.error("Output may not be inside the source tree.")

    try:
        with tempfile.TemporaryDirectory(prefix="switch-interface-") as tmp:
            working = Path(tmp) / "eden"
            if source.is_file():
                if source.suffix.lower() != ".zip":
                    raise PatchError("Only source directories and .zip files are accepted.")
                working.mkdir()
                working = extract_zip_safely(source, working)
            else:
                shutil.copytree(source, working, symlinks=False,
                                ignore=shutil.ignore_patterns(".git", "build", "build-*"))
            applied = patch_tree(working)
            output.parent.mkdir(parents=True, exist_ok=True)
            if output.suffix.lower() == ".zip":
                pack_zip(working, output)
            else:
                shutil.move(str(working), str(output))
            print(f"{'Applied' if applied else 'Already present'}: {output}")

        if args.build:
            print("Configuring Eden using CMake (this may fetch dependencies)...")
            subprocess.run(
                ["cmake", "-S", str(output), "-B", str(output / "build"), *args.cmake_arg],
                check=True,
            )
            subprocess.run(
                ["cmake", "--build", str(output / "build"), "--config", "Release", "--parallel", "2"],
                check=True,
            )
            print("Build finished. See the build directory for binaries.")
        return 0
    except (PatchError, OSError, zipfile.BadZipFile, subprocess.CalledProcessError) as exc:
        print(f"Switch Interface error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
