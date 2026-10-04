"""WGS 0.2 watertight check - offline CPython (not UE Python).
Reads Saved/wgs/weapons/winchester_parts.json, welds vertices by position (1e-4 cm),
counts boundary edges (not shared by exactly two triangles) per part, writes
Saved/wgs/weapons/winchester_parts_report.md.
"""
import json
import os

IN_PATH = "C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts.json"
OUT_PATH = "C:/UnrealEngine/Games/AZ/Saved/wgs/weapons/winchester_parts_report.md"

WELD_EPS = 1e-4  # cm


def weld_key(v):
    return (round(v[0] / WELD_EPS), round(v[1] / WELD_EPS), round(v[2] / WELD_EPS))


def check_part(verts, tris):
    # weld vertices by position
    key_to_id = {}
    remap = []
    for v in verts:
        k = weld_key(v)
        if k not in key_to_id:
            key_to_id[k] = len(key_to_id)
        remap.append(key_to_id[k])

    num_welded = len(key_to_id)

    # count edges (undirected, welded-index pairs)
    edge_count = {}
    num_tris = len(tris) // 3
    for t in range(num_tris):
        a, b, c = tris[t * 3], tris[t * 3 + 1], tris[t * 3 + 2]
        wa, wb, wc = remap[a], remap[b], remap[c]
        for u, v in ((wa, wb), (wb, wc), (wc, wa)):
            key = (u, v) if u < v else (v, u)
            edge_count[key] = edge_count.get(key, 0) + 1

    boundary_edges = sum(1 for cnt in edge_count.values() if cnt != 2)
    closed = boundary_edges == 0
    return {
        "num_verts_raw": len(verts),
        "num_verts_welded": num_welded,
        "num_tris": num_tris,
        "boundary_edges": boundary_edges,
        "closed": closed,
    }


def run():
    with open(IN_PATH) as f:
        data = json.load(f)

    results = {}
    for name, part in data.items():
        verts = part["verts"]
        tris = part["tris"]
        results[name] = check_part(verts, tris)

    lines = ["# Winchester weapon parts - watertight check\n"]
    lines.append("| part | verts (raw/welded) | tris | boundary edges | status |")
    lines.append("|---|---|---|---|---|")
    for name, r in results.items():
        status = "closed" if r["closed"] else f"open ({r['boundary_edges']} boundary edges)"
        lines.append(
            f"| {name} | {r['num_verts_raw']}/{r['num_verts_welded']} | {r['num_tris']} | "
            f"{r['boundary_edges']} | {status} |"
        )

    total_tris = sum(r["num_tris"] for r in results.values())
    lines.append(f"\nTotal triangles: {total_tris}")

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w") as f:
        f.write("\n".join(lines) + "\n")

    print("\n".join(lines))
    return results


if __name__ == "__main__":
    run()
