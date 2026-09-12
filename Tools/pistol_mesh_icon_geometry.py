"""Extract editable silhouette contours from the pistol OBJ for native GIMP art.

This projects supplied mesh geometry; it never edits an existing image.
Unreal's OBJ exporter maps weapon forward Y to OBJ Z, and weapon up Z to OBJ Y.
"""
from pathlib import Path
import json
import math

ROOT = Path('C:/UnrealEngine/Games/AZ')


def simplify(points, tolerance=0.8):
    if len(points) < 3:
        return points
    a, b = points[0], points[-1]
    dx, dy = b[0] - a[0], b[1] - a[1]
    denominator = dx * dx + dy * dy
    distances = []
    for c in points[1:-1]:
        t = max(0, min(1, ((c[0] - a[0]) * dx + (c[1] - a[1]) * dy) / denominator)) if denominator else 0
        distances.append(math.hypot(c[0] - a[0] - t * dx, c[1] - a[1] - t * dy))
    maximum = max(distances, default=0)
    if maximum <= tolerance:
        return [a, b]
    split = distances.index(maximum) + 1
    return simplify(points[:split + 1], tolerance)[:-1] + simplify(points[split:], tolerance)


def main():
    source = ROOT / 'Saved/Pistol/Pistol_Single.obj'
    vertices, faces = [], []
    for line in source.read_text().splitlines():
        if line.startswith('v '):
            vertices.append(tuple(float(x) for x in line.split()[1:4]))
        elif line.startswith('f '):
            faces.append(tuple(int(x.split('/')[0]) - 1 for x in line.split()[1:]))
    width, height = 512, 384
    low = [min(v[i] for v in vertices) for i in range(3)]
    high = [max(v[i] for v in vertices) for i in range(3)]
    scale = min((width - 40) / (high[2] - low[2]), (height - 40) / (high[1] - low[1]))
    points = [(width / 2 + (v[2] - (low[2] + high[2]) / 2) * scale,
               height / 2 - (v[1] - (low[1] + high[1]) / 2) * scale) for v in vertices]
    mask = [bytearray(width) for _ in range(height)]
    for face in faces:
        polygon = [points[i] for i in face]
        for y in range(max(0, math.ceil(min(p[1] for p in polygon) - .5)),
                       min(height, math.ceil(max(p[1] for p in polygon) - .5))):
            sy, intersections = y + .5, []
            for a, b in zip(polygon, polygon[1:] + polygon[:1]):
                if min(a[1], b[1]) <= sy < max(a[1], b[1]):
                    intersections.append(a[0] + (sy - a[1]) * (b[0] - a[0]) / (b[1] - a[1]))
            if len(intersections) >= 2:
                x0 = max(0, math.ceil(min(intersections) - .5))
                x1 = min(width, math.ceil(max(intersections) - .5))
                if x1 > x0:
                    mask[y][x0:x1] = b'\1' * (x1 - x0)
    edges = {}

    def add(a, b):
        edges.setdefault(a, []).append(b)

    for y, row in enumerate(mask):
        for x, occupied in enumerate(row):
            if not occupied:
                continue
            if y == 0 or not mask[y - 1][x]: add((x, y), (x + 1, y))
            if x == width - 1 or not row[x + 1]: add((x + 1, y), (x + 1, y + 1))
            if y == height - 1 or not mask[y + 1][x]: add((x + 1, y + 1), (x, y + 1))
            if x == 0 or not row[x - 1]: add((x, y + 1), (x, y))
    contours = []
    while edges:
        first = next(iter(edges))
        current, loop = first, []
        while True:
            loop.append(current)
            following = edges[current].pop()
            if not edges[current]: del edges[current]
            current = following
            if current == first: break
            if current not in edges: raise RuntimeError('Open mesh contour')
        area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(loop, loop[1:] + loop[:1])) / 2
        if abs(area) <= 15:
            continue
        middle = len(loop) // 2
        outline = simplify(loop[:middle + 1])[:-1] + simplify(loop[middle:] + [loop[0]])[:-1]
        contours.append(dict(area=area, points=outline))
    contours.sort(key=lambda c: c['area'], reverse=True)
    result = dict(source=str(source), width=width, height=height, contours=contours)
    (ROOT / 'Saved/Pistol/icon-contours.json').write_text(json.dumps(result, indent=2))
    print({'contours': [(c['area'], len(c['points'])) for c in contours]})


if __name__ == '__main__':
    main()
