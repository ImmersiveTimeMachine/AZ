import json, math, sys
from collections import defaultdict

D = json.load(open(r"C:/UnrealEngine/Games/AZ/Saved/az_gun_scan.json"))


def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), 2 * (x * vy - y * vx)
    return (vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def T(t):
    return (tuple(t[:3]), tuple(t[3:7]))


def apply(t, p):
    r = qrot(t[1], p)
    return (r[0] + t[0][0], r[1] + t[0][1], r[2] + t[0][2])


def qconj(q):
    return (-q[0], -q[1], -q[2], q[3])


def inv(t):
    qi = qconj(t[1])
    p = qrot(qi, t[0])
    return ((-p[0], -p[1], -p[2]), qi)


def compose(a, b):
    return (apply(b, a[0]), qmul(b[1], a[1]))


class Cloud:
    def __init__(self, pts, cell=4.0):
        self.pts, self.cell, self.g = pts, cell, defaultdict(list)
        for p in pts:
            self.g[tuple(int(math.floor(c / cell)) for c in p)].append(p)

    def dist(self, p, maxr=6):
        k = tuple(int(math.floor(c / self.cell)) for c in p)
        best = 1e18
        for r in range(0, maxr + 1):
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dz in range(-r, r + 1):
                        if max(abs(dx), abs(dy), abs(dz)) != r:
                            continue
                        for q in self.g.get((k[0] + dx, k[1] + dy, k[2] + dz), ()):
                            d = (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 + (q[2] - p[2]) ** 2
                            if d < best:
                                best = d
            if best < ((r) * self.cell) ** 2:
                break
        return math.sqrt(best) if best < 1e17 else 99.0


pack_clouds = {k: Cloud([tuple(p) for p in v["pts"]]) for k, v in D["pack_guns"].items()}
pack_bone = {k: v["gun_bone"] for k, v in D["pack_guns"].items()}
models = {k: Cloud([tuple(p) for p in v[::2]]) for k, v in D["models"].items()}
socks = {k: (tuple(v[:3]), tuple(v[3:7])) for k, v in D["sockets"].items() if v}


def palm(fr, side):
    a, b = fr["hand_" + side][:3], fr["middle_01_" + side][:3]
    return tuple((a[i] + b[i]) / 2 for i in range(3))


def model_for(name, skel):
    if "ShotGun" in name:
        return "remington"
    if "Automatic" in name or "DoubleBarrel" in name:
        return None
    return "winchester"


HOLD = 11.0     # pack palm within this of its gun = the hand is on the gun
OFF = 5.0       # ours further than pack + OFF on such frames = off the gun
rows = []
by_set = defaultdict(lambda: [0, 0, 0])
for c in D["clips"]:
    if "frames" not in c:
        print("ERR", c)
        continue
    mk = model_for(c["name"], c["skel"])
    if mk is None:
        continue
    pc, gb = pack_clouds[c["skel"]], pack_bone[c["skel"]]
    oc, sk = models[mk], socks[mk]
    worst = {"l": (0.0, -1, 0, 0), "r": (0.0, -1, 0, 0)}
    held = {"l": 0, "r": 0}
    off = {"l": 0, "r": 0}
    for fr in c["frames"]:
        gw = T(fr["pack"][gb])
        ow = compose(sk, T(fr["mst"]["az_weapon_r"]))
        oinv = inv(ow)
        ginv = inv(gw)
        for side in ("l", "r"):
            dp = pc.dist(apply(ginv, palm(fr["pack"], side)))
            do = oc.dist(apply(oinv, palm(fr["mst"], side)))
            if dp <= HOLD:
                held[side] += 1
                extra = do - dp
                if extra > OFF:
                    off[side] += 1
                if extra > worst[side][0]:
                    worst[side] = (extra, fr["f"], dp, do)
    rows.append((c["name"], mk, len(c["frames"]), held, off, worst))
    s = c["name"].split("/")[0] + "/" + c["name"].split("/")[1] if c["name"].count("/") > 1 else c["name"].split("/")[0]
    by_set[s][0] += 1
    by_set[s][1] += 1 if off["l"] else 0
    by_set[s][2] += 1 if off["r"] else 0

n = len(rows)
bad_l = [r for r in rows if r[4]["l"]]
bad_r = [r for r in rows if r[4]["r"]]
print("clips scanned %d (Automatic/DoubleBarrel skipped: no model of ours)" % n)
print("LEFT hand off our gun (> pack + %.0f cm while the pack holds it) in %d clips; RIGHT in %d clips" % (OFF, len(bad_l), len(bad_r)))
print("\nper set: clips / left-off / right-off")
for s, v in sorted(by_set.items()):
    print("  %-45s %3d  L %3d  R %3d" % (s, v[0], v[1], v[2]))
print("\nworst LEFT (extra cm @frame, pack cm -> ours cm):")
for r in sorted(bad_l, key=lambda r: -r[5]["l"][0])[:25]:
    w = r[5]["l"]
    print("  %-60s %-10s +%.1f @f%d (%.1f -> %.1f), off %d/%d held frames" % (r[0], r[1], w[0], w[1], w[2], w[3], r[4]["l"], r[3]["l"]))
print("\nworst RIGHT:")
for r in sorted(bad_r, key=lambda r: -r[5]["r"][0])[:15]:
    w = r[5]["r"]
    print("  %-60s %-10s +%.1f @f%d (%.1f -> %.1f), off %d/%d held frames" % (r[0], r[1], w[0], w[1], w[2], w[3], r[4]["r"], r[3]["r"]))
# distribution of left extra over all held frames
json.dump([{"name": r[0], "model": r[1], "held": r[3], "off": r[4], "worst": r[5]} for r in rows],
          open(r"C:/UnrealEngine/Games/AZ/Saved/az_gun_scan_result.json", "w"), indent=0)
