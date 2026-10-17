---
name: feedback-h2l-one-depth-check
description: "USER DECISION 2026-10-15 — High→Low agent may run ONE native depth check against the High and make ONE correction (2 runs max); all skill/prompt rules must be general for any object, never cabinet-specific"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 2fe7c274-bddc-45e1-ab33-fd1c3fc14bd2
  modified: 2026-10-15T23:31:41.312Z
---

Artur, after seeing door gaps and a hinge buried in a side wall: "Делай оба: правила в скилл и автоматическую сверку с High с одним проходом исправления агентом" and "инструкции … должны быть общими", not only for lockers.

Implemented in AZ AI Utilities 1.5.0: `depth_check.py` (six-axis ray depth High vs Low: hole/extra regions, systematic per-side offset, PNG per side), `reconstruction.preview_check` limited to 2 runs, runner rejects jobs without a check or with edits after the 2nd; High numeric report has `surface_planes_source_local` (bounds include protruding hardware — the agent once put the whole front 1.7 cm forward from them). This supersedes the older blanket "no Low comparison / no refine loop" rule only for this one bounded check.

**Why:** the agent builds blind; small measurement errors (pitch drift in `repeat`, missing frame members, wrong face depth) were invisible to it. Control run: holes 4075→104 cm² and front offset caught; with planes the front was right first time and the frame was modeled.
Later the same day (user run 20261015-162434, luna/medium): hinge cylinders rotated [90,0,0] → discs (agent didn't know cylinder axis = local Z), no bevels, and the correction pass made the check WORSE (8645→9792 cm²). Added: explicit primitive-axis + rounded-edge rules, and the runner keeps the pre-correction inventory when check 2 is >5% worse. (Astra was compared later, see below; Sol left for Artur to run himself.)

Model comparison 2026-10-15 (same locker, new rules; sheet `Art/AIUtilities/h2l_model_comparison.png`): **gpt-6-astra/high** best — silhouette 99.6%, visible deviation p95 0.7 cm, check 415/934 cm², vertical hinges, bevelled vents, 377 s; gpt-6-luna/medium — 98.2%, 1.7 cm, 6074/2613, 202 s. gpt-6.1-sol failed only because the addon called the OLD npm Codex CLI 0.155 ("not supported when using Codex with a ChatGPT account"); the desktop app's build 0.162 runs it fine. The addon now auto-picks the newest codex.exe; Sol is back and is Artur's choice (stronger, fewer tokens). I wrongly removed it first — check the client version before blaming the account. In BOTH runs the agent's single correction made the check worse and the guard kept the first inventory — the correction pass is not yet adding value; the rules (planes, axes, bevel) and the stronger model are what helped.

**How to apply:** keep rules object-agnostic (surface continuity, depth from the High, repeat step from first/last instance, nothing visible buried). Open issue seen 2026-10-15: after the correction the front can come out too flat (agent pushed protrusions in) — watch for over-correction before adding more penalties. Related: [[project-blender-ai-utilities-audit]].
