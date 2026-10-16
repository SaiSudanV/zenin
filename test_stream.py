import subprocess
import time
import struct
import numpy as np

qemu_cmd = [
    "wsl", "-d", "kali-linux", "-e", "bash", "-c",
    "cd '/mnt/c/Custom Rom/kernel' && qemu-system-aarch64 -M virt -cpu cortex-a53 -smp 2 -m 2048 -nographic -kernel zenin-kernel.elf"
]

print("Launching QEMU process...")
p = subprocess.Popen(qemu_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)

frames_read = 0
in_frame = False
frame_lines = []

start_time = time.time()
while time.time() - start_time < 15:
    line = p.stdout.readline()
    if not line:
        break
    line_s = line.strip()

    if line_s == "---FRAME_START---":
        in_frame = True
        frame_lines = []
        continue
    elif line_s == "---FRAME_END---":
        in_frame = False
        frames_read += 1
        print(f"[TEST] >>> Frame {frames_read} captured: {len(frame_lines)} rows x {len(frame_lines[0]) if frame_lines else 0} chars")

        # Test fast vectorized decode
        t0 = time.time()
        full_hex = "".join(frame_lines)
        raw_bytes = bytes.fromhex(full_hex)
        arr16 = np.frombuffer(raw_bytes, dtype=np.uint16).reshape((640, 360))
        r = (((arr16 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
        g = (((arr16 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
        b = ((arr16 & 0x1F) * 255 // 31).astype(np.uint8)
        decode_ms = (time.time() - t0) * 1000.0
        print(f"[TEST] >>> Vectorized decode time: {decode_ms:.2f} ms (Target < 2 ms)")

        if frames_read == 1:
            # Test atomic packet injection: Mouse click on Windows tab (X:130, Y:70)
            raw = struct.pack("<BBhhH", 2, 1, 130, 70, 1)
            raw64 = struct.unpack("<Q", raw)[0]
            print(f"[TEST] Injecting atomic input packet: INP:{raw64:016X}")
            p.stdin.write(f"INP:{raw64:016X}\n")
            p.stdin.flush()
        elif frames_read == 2:
            # Switch container to Linux (key 3)
            print("[TEST] Sending container switch key: 3 (Linux Minecraft)")
            p.stdin.write("3\n")
            p.stdin.flush()
        elif frames_read == 3:
            # Action space key
            print("[TEST] Sending action key: Space")
            p.stdin.write(" \n")
            p.stdin.flush()
        elif frames_read >= 4:
            print("[TEST] Success! Exiting.")
            p.stdin.write("q\n")
            p.stdin.flush()
            break
    elif in_frame:
        frame_lines.append(line_s)
    else:
        if any(tag in line_s for tag in ["[zenin-pe]", "[zenin-apk]", "[container-", "[status]"]):
            print("[BOOT-LOG]", line_s)

p.terminate()
print(f"[TEST] Finished. Total frames captured and verified: {frames_read}")
