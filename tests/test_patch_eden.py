# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from patch_eden import PatchError, detect_emulator, extract_zip_safely, main, patch_tree


class PatcherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        (self.source / "src/yuzu").mkdir(parents=True)
        (self.source / "CMakeLists.txt").write_text("project(yuzu)\n", encoding="utf-8")
        (self.source / "src/yuzu/CMakeLists.txt").write_text(
            "find_package(Qt6 REQUIRED COMPONENTS Widgets)\n"
            "add_executable(yuzu\n    main_window.cpp\n)\n",
            encoding="utf-8",
        )
        (self.source / "src/yuzu/main_window.h").write_text(
            "class GameList;\n"
            "class MainWindow {\n"
            "    GameList* game_list = nullptr;\n"
            "};\n",
            encoding="utf-8",
        )
        (self.source / "src/yuzu/main_window.cpp").write_text(
            '#include "main_window.h"\n'
            "void startup() {\n"
            "    game_list->PopulateAsync(UISettings::values.game_dirs);\n\n"
            "    // Set up game list mode checkboxes.\n"
            "}\n"
            "void stop() {\n"
            "    game_list->SetFilterFocus();\n"
            "    tas_label->clear();\n"
            "}\n",
            encoding="utf-8",
        )

    def test_patches_expected_files_and_is_idempotent(self):
        self.assertTrue(patch_tree(self.source))
        self.assertTrue((self.source / "src/yuzu/switch_home.h").is_file())
        self.assertTrue((self.source / "src/yuzu/switch_home.cpp").is_file())
        content = (self.source / "src/yuzu/main_window.cpp").read_text()
        self.assertIn("BootGameFromList(game_path, StartGameType::Normal)", content)
        self.assertIn("switch_home->show();", content)
        self.assertNotIn("switch_home->onControllers", content)
        self.assertNotIn("switch_home->onSettings", content)
        self.assertIn("Controllers and Settings are now rendered", content)
        self.assertFalse(patch_tree(self.source))

    def test_eglantine_variant_detects_and_patches(self):
        target = self.source / "src/yuzu/main_window.cpp"
        target.write_text("// Eglantine Emulator Project\\n" + target.read_text())
        emulator, spec = detect_emulator(self.source)
        self.assertEqual(emulator, "eglantine")
        self.assertEqual(spec["frontend"], "src/yuzu")
        self.assertTrue(patch_tree(self.source))
        self.assertEqual(
            __import__("json").loads((self.source / ".switch-interface.json").read_text())["emulator"],
            "eglantine",
        )

    def test_lichii_variant_uses_its_own_cmake_target(self):
        renamed = self.source / "src/lichii"
        (self.source / "src/yuzu").rename(renamed)
        cmake = renamed / "CMakeLists.txt"
        cmake.write_text(cmake.read_text().replace("add_executable(yuzu", "add_executable(lichii"))
        emulator, spec = detect_emulator(self.source)
        self.assertEqual(emulator, "lichii")
        self.assertEqual(spec["target"], "lichii")
        self.assertTrue(patch_tree(self.source))
        self.assertIn("add_executable(lichii", cmake.read_text())
        self.assertTrue((renamed / "switch_home.cpp").is_file())
        self.assertFalse((self.source / "src/yuzu/switch_home.cpp").exists())

    def test_rejects_unrecognized_cmake_target(self):
        cmake = self.source / "src/yuzu/CMakeLists.txt"
        cmake.write_text(cmake.read_text().replace("add_executable(yuzu", "add_library(yuzu"))
        with self.assertRaises(PatchError):
            patch_tree(self.source)

    def test_unknown_upstream_revision_fails_without_writing(self):
        target = self.source / "src/yuzu/main_window.cpp"
        original = target.read_text().replace(
            "    // Set up game list mode checkboxes.", "    // upstream changed here"
        )
        target.write_text(original)
        with self.assertRaises(PatchError):
            patch_tree(self.source)
        self.assertEqual(target.read_text(), original)
        self.assertNotIn("switch_home.cpp", (self.source / "src/yuzu/CMakeLists.txt").read_text())
        self.assertFalse((self.source / "src/yuzu/switch_home.cpp").exists())

    def test_partial_patch_fails_closed(self):
        target = self.source / "src/yuzu/main_window.h"
        target.write_text(target.read_text() + "// SWITCH_INTERFACE_PATCH_v1\n")
        with self.assertRaises(PatchError):
            patch_tree(self.source)

    def test_rejects_zip_path_traversal(self):
        archive = self.root / "malicious.zip"
        with zipfile.ZipFile(archive, "w") as zf:
            zf.writestr("../outside.txt", "bad")
        target = self.root / "unpacked"
        target.mkdir()
        with self.assertRaises(PatchError):
            extract_zip_safely(archive, target)
        self.assertFalse((self.root / "outside.txt").exists())

    def test_cli_creates_a_separate_copy(self):
        output = self.root / "patched"
        self.assertEqual(main(["--source", str(self.source), "--output", str(output)]), 0)
        self.assertTrue((output / "src/yuzu/switch_home.cpp").is_file())
        self.assertFalse((self.source / "src/yuzu/switch_home.cpp").exists())
        with self.assertRaises(SystemExit) as error:
            main(["--source", str(self.source), "--output", str(output)])
        self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
