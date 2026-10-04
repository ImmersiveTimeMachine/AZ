"""Before/after sheet of two solves from two cameras: python compare3d.py out.png before.json after.json"""
import sys, json
from PIL import Image
import view3d as V
import render
cams = [((-30, -14, -8), (-3, 1, 1)), ((-24, 14, -10), (-2, 2, 1)), ((-6, 10, -28), (-2, 2, 2))]
ims = []
for title, f in (("BEFORE (applied)", sys.argv[2]), ("AFTER (natural index)", sys.argv[3])):
    d = json.load(open(f))
    pts = V.hand_points(d)
    row = [V.draw(V.camera(c, l), pts, "%s  cam %d" % (title, i)) for i, (c, l) in enumerate(cams)]
    ims.append(row)
W = sum(i.width for i in ims[0]); H = sum(r[0].height for r in ims)
out = Image.new("RGB", (W, H), (255, 255, 255))
y = 0
for row in ims:
    x = 0
    for im in row:
        out.paste(im, (x, y)); x += im.width
    y += row[0].height
out = out.resize((W // 2, H // 2))
out.save(sys.argv[1]); print("saved", sys.argv[1])
