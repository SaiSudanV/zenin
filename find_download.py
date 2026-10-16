import urllib.request
import re

url = "https://atmos.itch.io/super-cyberpunk-2077-fan-prototype"
req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"})
with urllib.request.urlopen(req) as resp:
    html = resp.read().decode("utf-8", errors="ignore")

idx = html.find("download")
print("First download index:", idx)
while idx != -1:
    print("--- SNIPPET ---")
    print(html[max(0, idx-50):min(len(html), idx+150)])
    idx = html.find("download", idx + 1)
    if idx > 20000:
        break
