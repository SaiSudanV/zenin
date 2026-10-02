"""
Project Zenin - Live Interactive OS Window & Telemetry Monitor
Streams the real-time AArch64 bare-metal scanout framebuffer from QEMU into a native graphical desktop window with instant keyboard controls.
"""

import subprocess
import threading
import tkinter as tk
from PIL import Image, ImageTk

WIDTH = 180
HEIGHT = 320
SCALE = 2  # Window size: 360 x 640

def rgb565_to_rgb888(val16):
    r = ((val16 >> 11) & 0x1F) * 255 // 31
    g = ((val16 >> 5) & 0x3F) * 255 // 63
    b = (val16 & 0x1F) * 255 // 31
    return (r, g, b)

class ZeninLiveWindow:
    def __init__(self, root):
        self.root = root
        self.root.title("Project Zenin - Bare-Metal Live OS (1.0 GHz Dual-Core | 2GB RAM | 2000mAh)")
        self.root.geometry(f"{WIDTH * SCALE}x{HEIGHT * SCALE + 80}")
        self.root.configure(bg="#0B0F19")
        self.root.resizable(False, False)

        # Header Title
        self.header = tk.Label(
            root,
            text="PROJECT ZENIN BARE-METAL LIVE SCREEN",
            font=("Consolas", 10, "bold"),
            bg="#0B0F19",
            fg="#00E5FF"
        )
        self.header.pack(pady=4)

        # Canvas for direct scanout rendering
        self.canvas = tk.Canvas(
            root,
            width=WIDTH * SCALE,
            height=HEIGHT * SCALE,
            bg="#000000",
            highlightthickness=0
        )
        self.canvas.pack()

        # Bottom Instructions & Controls
        self.info = tk.Label(
            root,
            text="[1] Android  [2] Windows  [3] Linux  [4] macOS\n[SPACE] Action / Play  |  [Q] Exit",
            font=("Consolas", 9, "bold"),
            bg="#0B0F19",
            fg="#94A3B8"
        )
        self.info.pack(pady=6)

        # Current image buffer
        self.current_img = Image.new("RGB", (WIDTH, HEIGHT), color="#0F172A")
        self.tk_img = ImageTk.PhotoImage(self.current_img.resize((WIDTH * SCALE, HEIGHT * SCALE), Image.NEAREST))
        self.img_item = self.canvas.create_image(0, 0, anchor=tk.NW, image=self.tk_img)

        # Keyboard bindings
        self.root.bind("<Key>", self.on_key_press)

        # Launch QEMU background process
        qemu_cmd = [
            "wsl", "-d", "kali-linux", "-e", "bash", "-c",
            "cd /mnt/c/Custom\\ Rom/kernel && qemu-system-aarch64 -M virt -cpu cortex-a53 -smp 2 -m 2048 -nographic -kernel zenin-kernel.elf"
        ]
        self.proc = subprocess.Popen(
            qemu_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        self.running = True
        self.reader_thread = threading.Thread(target=self.read_qemu_stream, daemon=True)
        self.reader_thread.start()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def on_key_press(self, event):
        if not self.proc or self.proc.poll() is not None:
            return
        ch = event.char
        if ch in ['1', '2', '3', '4', ' ', 'q', 'Q', 'd']:
            try:
                self.proc.stdin.write(ch + "\n")
                self.proc.stdin.flush()
            except Exception as e:
                print("Stdin error:", e)
        if ch in ['q', 'Q']:
            self.on_close()

    def read_qemu_stream(self):
        in_frame = False
        frame_lines = []

        while self.running:
            line = self.proc.stdout.readline()
            if not line:
                break
            line_str = line.strip()

            if line_str == "---FRAME_START---":
                in_frame = True
                frame_lines = []
                continue
            elif line_str == "---FRAME_END---":
                in_frame = False
                if len(frame_lines) == HEIGHT:
                    self.update_frame(frame_lines)
                continue

            if in_frame:
                frame_lines.append(line_str)
            else:
                # Print kernel telemetry to stdout
                print(line_str)

    def update_frame(self, lines):
        pixels = []
        for y in range(HEIGHT):
            line = lines[y]
            row = []
            for x in range(WIDTH):
                idx = x * 4
                if idx + 4 <= len(line):
                    hex_val = int(line[idx:idx+4], 16)
                    row.append(rgb565_to_rgb888(hex_val))
                else:
                    row.append((0, 0, 0))
            pixels.extend(row)

        img = Image.new("RGB", (WIDTH, HEIGHT))
        img.putdata(pixels)
        scaled_img = img.resize((WIDTH * SCALE, HEIGHT * SCALE), Image.NEAREST)

        self.root.after(0, self.render_to_canvas, scaled_img)

    def render_to_canvas(self, scaled_img):
        self.tk_img = ImageTk.PhotoImage(scaled_img)
        self.canvas.itemconfig(self.img_item, image=self.tk_img)

    def on_close(self):
        self.running = False
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.stdin.write("q\n")
                self.proc.stdin.flush()
                self.proc.terminate()
            except Exception:
                pass
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = ZeninLiveWindow(root)
    root.mainloop()
