# Design briefs

Self-contained problem statements written for **external review** — each assumes no knowledge of this
project, so it restates the architecture, the measured data, and what has already been ruled out.

They are kept because the *measurements* in them are expensive to reproduce (root motion sampled per clip,
engine source read at specific line numbers) and because the "already tried" tables stop the same dead ends
being re-proposed.

| brief | question |
|---|---|
| `weapon-switch-status.md` | Animated rifle/pistol holster/draw and persistent carry actors; built, both profiles saved, user visual check pending. |
| `rifle-circular-magazines-status.md` | Current circular R, exact inventory Load into rifle, and inserted-only ammo HUD; full build/restart passed, user gameplay check next. |
| `rifle-reload-status.md` | Earlier reload transactions and runtime conservation evidence; fullest-spare and MAGS HUD policy superseded by circular magazines. |
| `additive-lean-rework.md` | Additive lean rework — port the CMC acceleration model, restore the Y axis, bind the layer |
| `claude-rifle-step0-assignment.md` | Claude assignment: rifle step 0, weapon-rig and magazine-content audit |
| `hero-camera-framing-and-aim-cone.md` | Hero camera: directional framing + aim free-look cone |
| `hud-magazine-icon-status.md` | HUD magazine icon — implemented and saved |
| `hud-phase1-status.md` | CHALK HUD Phase 1 — implemented, gameplay acceptance pending |
| `melee-wall-contact-proposal.md` | Punch contact with walls — implementation, 2026-09-05 |
| `mover-q-sprint.md` | Mover hero Q sprint |
| `moveraniminstance-gasp-refactor-plan.md` | `UAZ_MoverAnimInstance` → GASP-shaped refactor — design plan |
| `offsetrootbone-mover-port-plan.md` | OffsetRootBone in `AZ_ABP_MoverHero_MHC` — REVISED verdict (supersedes the "option 2 port" plan) |
| `quick-select-compact-status.md` | Quick select V4 — implemented, built and saved; gameplay check pending |
| `quick-select-eight-slots-status.md` | Quick select V3 — implemented, built and saved; gameplay check pending |
| `quick-select-implementation-plan.md` | CHALK quick select — proposed implementation plan |
| `quick-select-manual-slots-status.md` | Quick select revision — four manual slots and inventory composites |
| `quick-select-status.md` | CHALK quick select — implemented, built and saved; user gameplay check pending |
| `quick-select-v5-layout-status.md` | Quick Select V5 — implemented, compiled and saved |
| `rifle-animation-aim-status.md` | Rifle P01 exploration and aiming — ready for user validation |
| `rifle-automatic-status.md` | Rifle animation and automatic fire — built and wired, ready for user validation |
| `rifle-firing-status.md` | Rifle firing — built and wired, ready for user validation |
| `rifle-inventory-foundation-status.md` | Rifle inventory foundation — resume and manual acceptance |
| `rifle-inventory-magazines-plan.md` | Rifle, detachable magazines, inventory and HUD implementation plan |
| `rifle-p01-jump-fall-handoff.md` | Rifle P01 jump + fall — what changed (handoff) |
| `rifle-p01-jump-freeze-diagnosis.md` | Rifle jump freeze / repeat-jump block — 2026-09-07 |
| `rifle-p01-next-session.md` | Rifle P01 — next session |
| `rifle-p01-pose-debug-sprint.md` | P01 pose diagnostics and sprint carry |
| `rifle-p01-retarget-rebuild-plan.md` | Rifle P01 retarget rebuild — UE4 Manny → SurvivalMan (→ MetaHuman) — PLAN |
| `rifle-p01-transition-socket-fix.md` | P01 transitions and socket alignment — 2026-09-07 |
| `rifle-recoil-next-session.md` | Rifle recoil — next step after restart |
| `rifle-recoil-status.md` | Rifle recoil — built and saved; user gameplay check pending |
| `rifle-step0-content-contract.md` | Rifle step 0: content and contracts |
| `rifle-step0-skin-weight-review.md` | Step-0 M16 skin-weight verification |
| `rifle-step0-weapon-rig-audit.md` | Rifle step 0 — weapon-rig, mechanical animation, sockets and detachable-magazine audit |
| `bullet-impact-status.md` | Rifle impact smoke built and saved; reticle persistence repaired; user gameplay check pending. |
| `hud-reticle-status.md` | Scalable per-weapon rifle reticle implemented, built and saved; user gameplay check pending. |
| `hud-implementation-plan.md` | September 8 live HUD/inventory/GAS/GASP audit and proposed phased implementation, ownership, asset reuse, input handling and manual acceptance. |
| `hud-design-next-session.md` | Approved CHALK HUD v03, native GIMP layer requirements, existing asset findings, and next-session asset-review/implementation-plan agenda. |
| `stop-animation-problem-statement.md` | Why stop animations failed to play or match the capsule, and what the stable architecture is. Led to the latched stop contract and curve-driven braking. |
| `anim-speed-drive-problem-statement.md` | Should capsule speed come from a per-frame animation curve (`velocity = inputDir × clipSpeed`) rather than from tuned constants? |

Both are snapshots of what was known when written. Where they disagree with the memory notes under
`Docs/ai-memory`, the memory notes are newer.
