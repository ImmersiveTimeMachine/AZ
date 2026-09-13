"""Draw geometry projections for offline pivot review using Pillow."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
data = json.loads((ROOT / 'item_pivot_preview_data.json').read_text())
names = ['BP2_Axe', 'BP2_Knife', 'BP2_Bottle', 'BP2_Backpack', 'BP2_Front_Pouch', 'BP2_Bottle_Holder', 'BP2_Rope']
width, row_height = 1800, 450
image = Image.new('RGB', (width, row_height * len(names)), '#f0f2f5')
draw = ImageDraw.Draw(image)
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 22)
small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
axes = [(0, 2, 1, 'XZ : X right, Z up'), (0, 1, 2, 'XY : X right, Y up'), (1, 2, 0, 'YZ : Y right, Z up')]

for row, name in enumerate(names):
    part = data['parts'][name]
    points = part['points']
    for col, (u, v, depth, label) in enumerate(axes):
        left, top = col * 600, row * row_height
        draw.rectangle((left + 4, top + 4, left + 596, top + 446), fill='#ffffff', outline='#ced5dc')
        draw.text((left + 18, top + 10), name.replace('BP2_', '') + ' / ' + label, fill='#16283d', font=font)
        lows = [min(0, min(p[a] for p in points)) for a in (u, v)]
        highs = [max(0, max(p[a] for p in points)) for a in (u, v)]
        scale = min(510 / max(highs[0] - lows[0], .025), 325 / max(highs[1] - lows[1], .025))
        center = [(a + b) / 2 for a, b in zip(lows, highs)]
        def screen(point):
            return (left + 300 + (point[u] - center[0]) * scale, top + 228 - (point[v] - center[1]) * scale)
        faces = sorted(enumerate(part['faces']), key=lambda pair: sum(points[i][depth] for i in pair[1]) / len(pair[1]))
        for index, face in faces:
            component = part['face_components'][index]
            color = '#a9b2bc'
            if name == 'BP2_Axe' and component in [11, 167, 172]:
                color = '#885738'
            elif name == 'BP2_Knife' and component in [17, 121, 161]:
                color = '#4e7094'
            elif name == 'BP2_Rope':
                color = '#9b7954'
            elif name in ['BP2_Backpack', 'BP2_Front_Pouch', 'BP2_Bottle_Holder']:
                color = '#929575'
            elif name == 'BP2_Bottle':
                color = '#697b89' if component not in [98, 176, 177] else '#839cad'
            draw.polygon([screen(points[i]) for i in face], fill=color, outline='#59616b')
        ox, oy = screen([0, 0, 0])
        draw.line((ox - 11, oy, ox + 11, oy), fill='#d12d3d', width=3)
        draw.line((ox, oy - 11, ox, oy + 11), fill='#d12d3d', width=3)
        draw.ellipse((ox - 5, oy - 5, ox + 5, oy + 5), fill='#fefafa', outline='#d12d3d', width=2)
        draw.text((left + 18, top + 415), 'Red cross = proposed item origin', fill='#8a283a', font=small)

image.save(ROOT / 'item_pivots_preview.png')
print(ROOT / 'item_pivots_preview.png')
