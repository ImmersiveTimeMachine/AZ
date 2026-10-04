"""Split a dumped weapon mesh (weapon_skm_dump.py JSON) into connected components -> parts JSON for geom.Weapon /
bake_grip_field.py (the inside test runs per part, so open or overlapping pieces are handled)."""
import json, sys
src, out = sys.argv[1], sys.argv[2]
d = json.load(open(src))
V, T = d["verts"], d["tris"]
# weld identical positions (render meshes split vertices on UV / normal seams)
key = {}; remap = []
for v in V:
    k = (round(v[0], 3), round(v[1], 3), round(v[2], 3))
    if k not in key: key[k] = len(key)
    remap.append(key[k])
parent = list(range(len(key)))
def find(a):
    while parent[a] != a:
        parent[a] = parent[parent[a]]; a = parent[a]
    return a
for i in range(0, len(T), 3):
    a, b, c = remap[T[i]], remap[T[i + 1]], remap[T[i + 2]]
    for x, y in ((a, b), (b, c)):
        rx, ry = find(x), find(y)
        if rx != ry: parent[rx] = ry
pos = [None] * len(key)
for k, i in key.items(): pos[i] = list(k)
comps = {}
for i in range(0, len(T), 3):
    a, b, c = remap[T[i]], remap[T[i + 1]], remap[T[i + 2]]
    comps.setdefault(find(a), []).append((a, b, c))
parts = {}
for n, (r, tris) in enumerate(sorted(comps.items(), key=lambda kv: -len(kv[1]))):
    idx = {}; verts = []; flat = []
    for t in tris:
        for v in t:
            if v not in idx: idx[v] = len(verts); verts.append(pos[v])
            flat.append(idx[v])
    parts["part%02d" % n] = {"path": d["mesh"], "verts": verts, "tris": flat}
json.dump(parts, open(out, "w"))
sizes = [(k, len(v["tris"]) // 3, [round(min(p[j] for p in v["verts"]), 1) for j in range(3)], [round(max(p[j] for p in v["verts"]), 1) for j in range(3)]) for k, v in parts.items()]
print("components", len(parts))
for s in sizes[:25]: print("  ", s)
