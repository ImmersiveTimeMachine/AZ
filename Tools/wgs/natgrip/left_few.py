import os, sys, json
os.environ.setdefault("NATGRIP_SIDE", "l")
from multiprocessing import Pool
import left_solve
P = [(0, 0, 0, 0.0, 0.0, -0.5), (0, 0, 0, -0.5, 0.0, -0.5), (0, 0, 0, 0.0, 0.0, -0.8), (0, 0, 0, 0.0, 0.5, -0.5)]
if __name__ == "__main__":
    with Pool(4) as pool:
        res = pool.map(left_solve.solve, P)
    res.sort(key=lambda r: r["J"])
    json.dump(res, open("left_few.json", "w"), default=str)
    for r in res:
        print("J=%.2f p=%s palm %.2f | %s | thumb %s" % (r["J"], r["p"], r["palm"],
              " ".join("%s:phi%+.0f %s g%s" % (f[0], r[f][0], [round(v) for v in r[f][1]], [round(v, 2) for v in r[f][2]]) if r[f] else f[0] + ":x" for f in ("index", "middle", "ring", "pinky")),
              r["thumb"] and ([round(v) for v in r["thumb"][1]], round(r["thumb"][2], 2))))
