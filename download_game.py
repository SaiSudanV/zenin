import urllib.request
import json
import os

url = "https://atmos.itch.io/super-cyberpunk-2077-fan-prototype/file/3205996"
req = urllib.request.Request(url, data=b"", headers={
    "X-Requested-With": "XMLHttpRequest",
    "Accept": "application/json",
    "User-Agent": "Mozilla/5.0"
})

with urllib.request.urlopen(req) as resp:
    data = json.loads(resp.read().decode())
    download_url = data["url"]
    print("Download URL obtained. Fetching SuperCyberpunk.zip (117 MB)...")

output_path = r"c:\Custom Rom\SuperCyberpunk.zip"
req_dl = urllib.request.Request(download_url, headers={"User-Agent": "Mozilla/5.0"})
with urllib.request.urlopen(req_dl) as dl_resp, open(output_path, "wb") as out_f:
    total_bytes = 0
    while True:
        chunk = dl_resp.read(1024 * 1024) # 1MB chunks
        if not chunk:
            break
        out_f.write(chunk)
        total_bytes += len(chunk)
        print(f"Downloaded {total_bytes / (1024*1024):.1f} MB...")

print("Download complete! File saved to:", output_path)
