# Design briefs

Self-contained problem statements written for **external review** — each assumes no knowledge of this
project, so it restates the architecture, the measured data, and what has already been ruled out.

They are kept because the *measurements* in them are expensive to reproduce (root motion sampled per clip,
engine source read at specific line numbers) and because the "already tried" tables stop the same dead ends
being re-proposed.

| brief | question |
|---|---|
| `bullet-impact-status.md` | Rifle impact smoke built and saved; reticle persistence repaired; user gameplay check pending. |
| `hud-reticle-status.md` | Scalable per-weapon rifle reticle implemented, built and saved; user gameplay check pending. |
| `hud-implementation-plan.md` | September 8 live HUD/inventory/GAS/GASP audit and proposed phased implementation, ownership, asset reuse, input handling and manual acceptance. |
| `hud-design-next-session.md` | Approved CHALK HUD v03, native GIMP layer requirements, existing asset findings, and next-session asset-review/implementation-plan agenda. |
| `stop-animation-problem-statement.md` | Why stop animations failed to play or match the capsule, and what the stable architecture is. Led to the latched stop contract and curve-driven braking. |
| `anim-speed-drive-problem-statement.md` | Should capsule speed come from a per-frame animation curve (`velocity = inputDir × clipSpeed`) rather than from tuned constants? |

Both are snapshots of what was known when written. Where they disagree with the memory notes under
`Docs/ai-memory`, the memory notes are newer.
