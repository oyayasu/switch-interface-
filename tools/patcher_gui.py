#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Simple graphical wrapper for the Switch Interface Eden source patcher."""
from __future__ import annotations

from pathlib import Path
import queue
import threading
from contextlib import redirect_stdout, redirect_stderr
import patch_eden
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

class MessageStream:
    def __init__(self, messages):
        self.messages = messages
        self.pending = ""

    def write(self, value):
        self.pending += value
        while "\n" in self.pending:
            line, self.pending = self.pending.split("\n", 1)
            self.messages.put(("log", line))
        return len(value)

    def flush(self):
        if self.pending:
            self.messages.put(("log", self.pending))
            self.pending = ""



class PatcherWindow(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Switch Interface — Eden UI Patcher")
        self.geometry("780x560")
        self.minsize(660, 480)
        self.configure(bg="#f5f6f8")
        self.source = tk.StringVar()
        self.output = tk.StringVar()
        self.compile = tk.BooleanVar(value=False)
        self.messages: queue.Queue[tuple[str, str]] = queue.Queue()
        self.running = False

        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure("TFrame", background="#f5f6f8")
        style.configure("TLabel", background="#f5f6f8", font=("Segoe UI", 10))
        style.configure("Heading.TLabel", background="#f5f6f8",
                        font=("Segoe UI", 21, "bold"))
        style.configure("TButton", padding=(12, 8))
        style.configure("TEntry", padding=8)

        outer = ttk.Frame(self, padding=24)
        outer.pack(fill="both", expand=True)
        ttk.Label(outer, text="Eden UI Patcher", style="Heading.TLabel").pack(anchor="w")
        ttk.Label(outer, text="Apply the Switch-style interface to a fresh Eden source copy.").pack(
            anchor="w", pady=(1, 22))

        ttk.Label(outer, text="Eden source (.zip or cloned folder)").pack(anchor="w")
        source_row = ttk.Frame(outer)
        source_row.pack(fill="x", pady=(6, 16))
        ttk.Entry(source_row, textvariable=self.source).pack(side="left", fill="x", expand=True)
        ttk.Button(source_row, text="ZIP", command=self.choose_source_zip).pack(side="left", padx=(8, 3))
        ttk.Button(source_row, text="Folder", command=self.choose_source_folder).pack(side="left")

        ttk.Label(outer, text="Save modified source to (.zip or new folder)").pack(anchor="w")
        output_row = ttk.Frame(outer)
        output_row.pack(fill="x", pady=(6, 10))
        ttk.Entry(output_row, textvariable=self.output).pack(side="left", fill="x", expand=True)
        ttk.Button(output_row, text="ZIP", command=self.choose_output_zip).pack(side="left", padx=(8, 3))
        ttk.Button(output_row, text="Folder", command=self.choose_output_folder).pack(side="left")

        ttk.Checkbutton(
            outer, text="Compile using local CMake toolchain (folder output only)",
            variable=self.compile,
        ).pack(anchor="w", pady=(5, 16))

        self.run_button = ttk.Button(outer, text="Patch Eden", command=self.start)
        self.run_button.pack(anchor="w", pady=(0, 16))

        ttk.Label(outer, text="Output log").pack(anchor="w", pady=(0, 5))
        self.log = ScrolledText(
            outer, height=9, font=("Consolas", 9),
            wrap="word", relief="solid", borderwidth=1,
        )
        self.log.pack(fill="both", expand=True)
        self.log.configure(state="disabled")
        self.after(150, self.read_messages)

    def choose_source_zip(self) -> None:
        value = filedialog.askopenfilename(
            title="Select Eden source ZIP", filetypes=[("ZIP archives", "*.zip")])
        if value:
            self.source.set(value)

    def choose_source_folder(self) -> None:
        value = filedialog.askdirectory(title="Select Eden source checkout")
        if value:
            self.source.set(value)

    def choose_output_zip(self) -> None:
        value = filedialog.asksaveasfilename(
            title="Choose a new output ZIP", defaultextension=".zip",
            filetypes=[("ZIP archives", "*.zip")])
        if value:
            self.output.set(value)
            self.compile.set(False)

    def choose_output_folder(self) -> None:
        # Asks for an existing parent folder, then generates a new name within it.
        value = filedialog.askdirectory(title="Choose parent directory for patched source")
        if value:
            self.output.set(str(Path(value) / "eden-switch-custom"))

    def append_log(self, line: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def start(self) -> None:
        source = self.source.get().strip()
        output = self.output.get().strip()
        if not source or not output:
            messagebox.showerror("Missing paths", "Select a source and a destination.")
            return
        if not Path(source).exists():
            messagebox.showerror("Invalid source", "Selected source does not exist.")
            return
        if Path(output).exists():
            messagebox.showerror("Output exists", "Choose an unused destination.")
            return
        if self.compile.get() and output.lower().endswith(".zip"):
            messagebox.showerror("Build requires folder", "Choose a folder output to compile.")
            return
        self.running = True
        self.run_button.configure(state="disabled")
        self.append_log(f"Source: {source}")
        self.append_log(f"Destination: {output}")
        command = ["--source", source, "--output", output]
        if self.compile.get():
            command.append("--build")
        threading.Thread(target=self.worker, args=(command,), daemon=True).start()

    def worker(self, command: list[str]) -> None:
        stream = MessageStream(self.messages)
        try:
            with redirect_stdout(stream), redirect_stderr(stream):
                status = patch_eden.main(command)
            stream.flush()
            self.messages.put(("done", str(status)))
        except (Exception, SystemExit) as error:
            stream.flush()
            self.messages.put(("log", f"Could not complete patch: {error}"))
            self.messages.put(("done", "1"))

    def read_messages(self) -> None:
        try:
            while True:
                kind, value = self.messages.get_nowait()
                if kind == "log":
                    self.append_log(value)
                elif kind == "done":
                    self.running = False
                    self.run_button.configure(state="normal")
                    if value == "0":
                        messagebox.showinfo("Complete", "Patched Eden source is ready.")
                    else:
                        messagebox.showerror("Failed", "See the output log for details.")
        except queue.Empty:
            pass
        self.after(150, self.read_messages)


if __name__ == "__main__":
    PatcherWindow().mainloop()
