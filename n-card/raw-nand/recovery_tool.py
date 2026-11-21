#!/usr/bin/env python3
"""Offline N-Card recovery tools for saved NAND dumps."""

import os
import subprocess
import sys
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

from arm_banner import default_template, prepare_from_dumps
from inspect_boot import (
    SECONDARY_TABLE,
    SPARE_SECTOR_SIZE,
    TABLE_SIZE,
    inspect_boot,
    read_main_dump,
    read_prefix,
)


class RecoveryTool:
    def __init__(self, root):
        self.root = root
        root.title("N-Card Recovery Toolkit")
        root.minsize(640, 480)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)

        tabs = ttk.Notebook(root)
        tabs.grid(row=0, column=0, sticky="nsew", padx=12, pady=(12, 6))
        recovery = ttk.Frame(tabs, padding=12)
        banner = ttk.Frame(tabs, padding=12)
        tabs.add(recovery, text="Inspect and export")
        tabs.add(banner, text="Banner repair")

        recovery.columnconfigure(1, weight=1)
        recovery.rowconfigure(4, weight=1)
        self.main_dump = self.file_field(recovery, 0, "Target main dump")
        self.spare_dump = self.file_field(recovery, 1, "Target spare dump")
        ttk.Label(recovery, text="Use separate main and spare files from the same card.").grid(
            row=2, column=0, columnspan=3, sticky="w", pady=(4, 8))
        actions = ttk.Frame(recovery)
        actions.grid(row=3, column=0, columnspan=3, sticky="w", pady=(0, 8))
        ttk.Button(actions, text="Inspect dumps", command=self.inspect).pack(side="left", padx=(0, 6))
        ttk.Button(actions, text="Save spare sector 0", command=self.save_spare).pack(side="left", padx=(0, 6))
        ttk.Button(actions, text="Export secondary table for review", command=self.export_table).pack(side="left")

        self.report = ScrolledText(recovery, width=76, height=16, wrap="none", state="disabled")
        self.report.grid(row=4, column=0, columnspan=3, sticky="nsew")
        self.set_report("Select the target dumps and choose Inspect dumps.\n"
                        "The table export is a candidate for offline review.")

        banner.columnconfigure(1, weight=1)
        self.file_field(banner, 0, "Target spare dump", self.spare_dump)
        self.donor_main = self.file_field(banner, 1, "Donor main dump")
        self.donor_spare = self.file_field(banner, 2, "Donor spare dump")
        ttk.Label(banner, text="Use a donor with banner data compatible with the target boot layout.",
                  wraplength=540).grid(row=3, column=0, columnspan=3, sticky="w", pady=(6, 8))
        ttk.Button(banner, text="Prepare banner files", command=self.prepare_banner).grid(
            row=4, column=0, columnspan=3, sticky="w")

        footer = ttk.Frame(root, padding=(12, 0, 12, 10))
        footer.grid(row=1, column=0, sticky="ew")
        footer.columnconfigure(0, weight=1)
        self.status = tk.StringVar(value="Ready")
        ttk.Label(footer, textvariable=self.status, wraplength=455).grid(row=0, column=0, sticky="w")
        ttk.Button(footer, text="Open DS tools folder", command=self.open_ds_tools).grid(row=0, column=1, padx=(8, 0))

    def file_field(self, parent, row, label, value=None):
        if value is None:
            value = tk.StringVar()
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=3)
        ttk.Entry(parent, textvariable=value).grid(row=row, column=1, sticky="ew", pady=3)
        ttk.Button(parent, text="Browse…", command=lambda: self.browse(value, label)).grid(
            row=row, column=2, padx=(8, 0), pady=3)
        return value

    def browse(self, value, label):
        path = filedialog.askopenfilename(parent=self.root, title=label)
        if path:
            value.set(path)

    def set_report(self, text):
        self.report.configure(state="normal")
        self.report.delete("1.0", "end")
        self.report.insert("1.0", text)
        self.report.configure(state="disabled")

    def dump_file(self, value, label):
        path = Path(value.get().strip())
        if not path.is_file():
            raise ValueError(f"Select a {label} file.")
        return path

    def show_error(self, error):
        self.status.set("Action failed")
        messagebox.showerror("N-Card Recovery Toolkit", str(error), parent=self.root)

    def inspect(self):
        try:
            main = self.dump_file(self.main_dump, "target main dump")
            spare = self.dump_file(self.spare_dump, "target spare dump")
            self.set_report(inspect_boot(main, spare))
            self.status.set("Inspection complete")
        except (OSError, ValueError) as error:
            self.set_report(str(error))
            self.show_error(error)

    def save_bytes(self, name, data):
        path = filedialog.asksaveasfilename(parent=self.root, initialfile=name, defaultextension=".bin")
        if path:
            output_path = Path(path)
            output = output_path.open("xb")
            try:
                with output:
                    output.write(data)
            except OSError:
                output_path.unlink(missing_ok=True)
                raise
            self.status.set(f"Saved {output_path}")

    def save_spare(self):
        try:
            spare = self.dump_file(self.spare_dump, "target spare dump")
            self.save_bytes("shared-spare-sector0.bin", read_prefix(spare, SPARE_SECTOR_SIZE))
        except (OSError, ValueError) as error:
            self.show_error(error)

    def export_table(self):
        try:
            main = self.dump_file(self.main_dump, "target main dump")
            table = read_main_dump(main, SECONDARY_TABLE + TABLE_SIZE)[SECONDARY_TABLE:]
            if len(set(table)) == 1:
                raise ValueError("The secondary table region contains a single repeated byte.")
            self.save_bytes("secondary-table-0x40000-review.bin", table)
        except (OSError, ValueError) as error:
            self.show_error(error)

    def prepare_banner(self):
        try:
            target = self.dump_file(self.spare_dump, "target spare dump")
            donor_main = self.dump_file(self.donor_main, "donor main dump")
            donor_spare = self.dump_file(self.donor_spare, "donor spare dump")
            parent = filedialog.askdirectory(parent=self.root, title="Choose where to create banner-ready")
            if parent:
                output_dir = Path(parent) / "banner-ready"
                prepare_from_dumps(target, donor_main, donor_spare, default_template(), output_dir)
                self.status.set(f"Prepared the banner app and payload in {output_dir}")
        except (OSError, ValueError) as error:
            self.show_error(error)

    def open_ds_tools(self):
        directory = Path(__file__).resolve().parent / "compiled"
        try:
            if sys.platform == "win32":
                os.startfile(directory)
            else:
                opener = "open" if sys.platform == "darwin" else "xdg-open"
                subprocess.Popen([opener, str(directory)])
        except OSError as error:
            self.show_error(error)


def main():
    root = tk.Tk()
    RecoveryTool(root)
    root.mainloop()


if __name__ == "__main__":
    main()
