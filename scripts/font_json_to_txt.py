#!/usr/bin/env python3

"""
Convert json output from msdf-atlas-gen (https://github.com/Chlumsky/msdf-atlas-gen) to custom format for parsing in C++.
Output goes to asset/atlas.txt.

Example:
```
msdf-atlas-gen-font /usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf -type msdf -fontname ubuntu_mono -uniformcols 10 -imageout atlas.bmp -json atlas.json --pxrange 8
./font_json_to_txt.py atlas.json -square4 > atlas.txt
```
"""

import json
import sys

if len(sys.argv) == 1:
    sys.exit("missing json")

with open(sys.argv[1], "r") as fp:
    data = json.load(fp)

a = data["atlas"]
print(f"distance_range {a['distanceRange']}")
print(f"em_size {a['size']}")
print(f"grid_width {a['grid']['cellWidth']}")
print(f"grid_height {a['grid']['cellHeight']}")
print("unicode")

for a in data["glyphs"]:
    print(a["unicode"], end=" ")
    print(a["advance"], end=" ")

    if "planeBounds" not in a:
        print("0 0 0 0 0 0 0 0")
        continue

    print(a["planeBounds"]["left"], end=" ")
    print(a["planeBounds"]["bottom"], end=" ")
    print(a["planeBounds"]["right"], end=" ")
    print(a["planeBounds"]["top"], end=" ")
    print(a["atlasBounds"]["left"], end=" ")
    print(a["atlasBounds"]["bottom"], end=" ")
    print(a["atlasBounds"]["right"], end=" ")
    print(a["atlasBounds"]["top"], end=" ")
    print("")
