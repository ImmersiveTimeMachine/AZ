---
name: feedback-aaa-design-first
description: "★ USER RULE (2026-08-03) — AAA approach on every task; design + anticipate BEFORE coding; propose, get go, then implement"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-03T04:08:48.605Z
---

# ★ USER RULE: AAA approach — think before coding, on EVERY task

**"I need the AAA approach, not just do it — think before coding, look ahead and anticipate things.
Make this a rule for each task I ask."** (2026-08-03, during the combat arch refactor.)

**Why:** an evening of reactive fixes produced 4 parallel clocks for one gameplay beat, three wrong
diagnoses from reasoning instead of instrumenting, a comment asserting the opposite of the code, and a
silent AI-pacing change. Each fix was locally fine; nobody looked ahead. The user caught the systemic
smell ("a lot of timing, not event-based") before I did — twice.

**How to apply — before implementing ANY non-trivial user task:**
1. **Propose first, wait for go.** Short design: requirements, the decisions with WHY, rollout order,
   test plan. User explicitly approves before code.
2. **Anticipate the standard failure axes** every time: death/interrupt mid-X · re-entry/re-trigger ·
   ordering vs other systems (GAS cancels, BT loop, slot preemption) · timeline vs wall-clock (rate
   scale, hitstop, pause) · shared-asset conflicts (two variants, one clip) · corpse/stale-pointer ·
   SP-first but co-op-extensible (server-auth now, note the replication seam).
3. **Events drive, timers guard.** Gameplay beats anchored to a clip live ON the clip (notify/section);
   FTimerHandle only as watchdog/cooldown/real-world duration. Never N clocks for one fact.
4. **One owner per fact.** A number (beat, gate, duration) resolves in ONE place; call sites never
   re-derive it. A state (staggered, grabbing) has ONE authoritative representation (tag), never
   inferred from animation playback.
5. **Measure, don't guess** (clip peaks, root paths, actual BT node values) and **instrument before
   diagnosing** (one log line beats three hypotheses — proven repeatedly).
6. Scale the ceremony to the task — a one-line tweak needs a sentence of look-ahead, not a document.

Related: [[project-combat-arch-refactor]], [[feedback-seam-trace-before-pie]]
