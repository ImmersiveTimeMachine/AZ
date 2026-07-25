---
name: feedback_seam_trace_before_pie
description: "MANDATORY before asking the user to PIE-test NPC/combat work: trace ONE full engagement loop end-to-end with REAL NUMBERS across the BT-values x C++-constants seam. Unit-validated pieces + reviewed subsystem still shipped 5 seam bugs in one evening (2026-07-22)."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-22T22:42:36.988Z
---

# Trace the seam numerically before every PIE request

**What happened (2026-07-22, crowd brain v3 integration):** the subsystem C++ was hypothesis-validated
and adversarially reviewed — and PIE still failed 3 times in a row, one seam bug per run: Press gait left
at Walk default while Ring had Sprint (attackers crawled, observers sprinted); turn clock burned on travel
time (rotated out mid-approach = perpetual milling); Press MoveTo acceptance 200 vs attack start gate 180
(permanent 2m standoff, ForceSuccess masking every failure); both Wait nodes at the 5.0s node default; ring
drift 70cm/s tangential vs ~30cm/s Walk gait (slots mathematically uncatchable — caught in advance only
because the user demanded the trace). Every single one was knowable from data already on hand.

**Why:** every combat bug this project has ever had lived at a SEAM (BT observation layer, BB key wiring,
gait/distance/timer constants split between BT node values and C++), never inside a unit-validated piece.
Reviewing components in isolation systematically misses exactly this class.

**How to apply:** before saying "PIE now", walk one full loop on paper with the CURRENT values — e.g. for
combat: role grant → decorator observation → branch gait (cm/s) → MoveTo acceptance + capsule radii vs the
attack task's start-distance/facing/settle gates → swing → Wait duration → loop → rotation timer vs real
approach time → demotion path → ring slot speed vs drift speed. Check every inequality with numbers. Dump
the actual BT node values (never trust "the user hand-built it per design" — defaults sneak in: SelfActor
MoveTo, Walk gait, 5s Waits were all fresh-node defaults). If any step's numbers don't close, fix BEFORE
the PIE request. Related: [[feedback_validate_agent_findings]], [[project_chalkie_fight_rules]].
