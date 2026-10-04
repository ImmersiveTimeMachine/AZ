"""Hand placement for a NATURAL trigger finger (M16 PIE review 2026-09-30, "the finger is a bit crooked").
With the applied re-grip the index knuckle sits so that the pad reaches the trigger only with the knuckle bent back
(MCP -5) and a hook at the middle joints; every natural trigger-finger shape (MCP 15-40, PIP 40-80, DIP ~0.65 PIP)
misses the trigger by 1.2-2.7 cm. So the hand placement itself must change a little: search placements near the
approved one where a natural index reaches the trigger, the palm stays out of the grip and the change is smallest.
python place_nat.py search [slices]   (runs the grid in parallel slices as separate processes)
python place_nat.py slice i n out.json"""
import os, sys, json, math, itertools, subprocess
from grasp3 import *
import place2, index_nat as I
import rsolve
P0 = (10.0, -10.0, 10.0, -1.0, -1.0, -1.0)                 # the applied (user-approved) re-grip


def natural_set():
    out = []
    one = T()
    for phi in (-4.0, -2.0, 0.0, 2.0, 4.0, 6.0):
        for a0 in range(10, 51, 5):
            for a1 in range(35, 86, 5):
                for r in (0.55, 0.65, 0.75):
                    x = [phi, float(a0), float(a1), r * a1]
                    u = I.unnatural(x)
                    if u > 1.5:
                        continue
                    W = world(one, finger_locals("index", 0, *x))
                    out.append((x, u, pad_point(W, "index"), W[chain("index")[-1]].vec((0.0, 1.0, 0.0))))
    return out


NAT = natural_set()


def dev(p):
    return math.sqrt(sum((p[k] - P0[k]) ** 2 for k in range(3))) / 10.0 + math.sqrt(sum((p[k] - P0[k]) ** 2 for k in range(3, 6)))


def eval_p(p):
    C = place2.corr(p[0], p[1], p[2], tuple(p[3:])); hand = place2.hand_new(C)
    best = None
    for x, u, pad_h, n_h in NAT:
        e = length(sub(hand.pos(pad_h), rsolve.TRIG))
        face = dot(hand.vec(n_h), I.PULL)
        s = (e / 0.1) ** 2 + u + 3.0 * (1.0 - face)
        if best is None or s < best[0]:
            best = (s, x, e, face)
    if best[2] > 0.6:
        return None
    palm = place2.palm_worst(C)
    if palm < -0.15:
        return None
    s, x, e, face = best
    pen = max(pens(world(hand, finger_locals("index", 0, *x)), "index"))
    J = s + (max(0.0, 0.05 - palm) * 20) ** 2 + max(0.0, pen) * 30 + 0.8 * dev(p)
    return {"p": p, "J": J, "idx": x, "err": e, "face": face, "palm": palm, "pen": pen, "dev": dev(p)}


def grid():
    return list(itertools.product((0, 5, 10, 15, 20, 25), (-25, -20, -15, -10, -5, 0, 5), (-5, 0, 5, 10, 15, 20, 25),
                                  (-3, -2, -1, 0, 1), (-3, -2, -1, 0, 1), (-3, -2.25, -1.5, -0.75, 0, 0.75, 1.5)))


if __name__ == "__main__":
    if sys.argv[1] == "slice":
        i, n, out = int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
        G = grid()[i::n]
        res = [r for r in (eval_p(p) for p in G) if r]
        json.dump(res, open(out, "w"))
    else:
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 12
        procs = [subprocess.Popen([sys.executable, "-u", __file__, "slice", str(i), str(n), "pn_%d.json" % i]) for i in range(n)]
        for pr in procs:
            pr.wait()
        res = []
        for i in range(n):
            res += json.load(open("pn_%d.json" % i)); os.remove("pn_%d.json" % i)
        res.sort(key=lambda r: r["J"])
        json.dump(res[:300], open("place_nat.json", "w"))
        print("candidates", len(res), "natural index set", len(NAT))
        for r in res[:20]:
            print("J %5.2f p %s idx %s err %.2f face %.2f palm %+.2f pen %+.2f dev %.2f" % (
                r["J"], r["p"], [round(v) for v in r["idx"]], r["err"], r["face"], r["palm"], r["pen"], r["dev"]))
