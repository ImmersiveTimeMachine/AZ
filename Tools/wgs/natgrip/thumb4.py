from grasp3 import *
import hand as H
Lc = H.clip_local()
WOOD_CX = -0.5
def thumb_solve4(hand, others, verbose=False):
    best = None; tried = 0
    for abd in range(-60, 31, 10):
        for fl in range(-40, 41, 10):
            for m in range(0, 61, 5):
                for k in (0.6, 0.8, 1.0, 1.2):
                    ip = k * m
                    if ip > 75: continue
                    L = thumb_locals(Lc, m, ip, (abd, fl))
                    W = world(hand, L)
                    p = max(link_pen(W, "thumb_01_r") - 0.35, link_pen(W, "thumb_02_r"), link_pen(W, "thumb_03_r"))
                    if p > 0 or finger_overlap(segs(W, "thumb"), others) > 0: continue
                    tried += 1
                    g3 = link_gap(W, "thumb_03_r")
                    tip = W["thumb_03_r"].pos(mul(MREF["thumb_03_r"].t, 0.9))
                    over = 1.0 if tip[0] > WOOD_CX else 0.0          # tip over / past the top centre line
                    s = -abs(g3 - 0.05) * 5 + 1.5 * over - (abd * abd + fl * fl) / 3000.0 \
                        - (max(0, 15 - m) ** 2 + max(0, m - 45) ** 2) / 400.0 - (max(0, 10 - ip) ** 2) / 400.0
                    if best is None or s > best[0]: best = (s, (abd, fl, m, ip), g3, tip)
    if verbose: print("thumb candidates", tried)
    return best
