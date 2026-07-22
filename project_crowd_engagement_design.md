---
name: project_crowd_engagement_design
description: "★ NEXT-SESSION design (drafted 2026-07-22 close, user-approved direction): fight-ring SLOT system + engagement rotation for the crowd brain — fixes NPC-blocking-NPC stacking AND delivers the observers/rotate-engagement concept. Implement on top of crowd brain v2 (commit bfaa7bc)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-22T03:42:36.220Z
---

# Crowd Engagement v3 — Ring Slots + Rotation
> STATUS UPDATE: user green-lit immediate implementation same session ("start now"); C++ written 2026-07-22, see rulebook for state.

## The two problems this solves (user-reported 2026-07-22 after roles v2 PIE)
1. **NPCs block each other**: pawn capsules block (correct — bodies matter) but everyone paths AT THE PREY, so
   zombies queue behind each other and the back ones can't reach. Roles v2's Ring MoveTo targets TargetActor
   (accept 400) = all ringers converge on the SAME point.
2. **Engagement concept (user's words)**: PC + some NPC become ENGAGED in a fight; engagement excludes the
   others — they are OBSERVERS — and engagement occasionally ROTATES (random disengage → an observer takes
   over) so the pack feels alive, not a static 2-lock.

## Industry survey (what these systems are called)
- **Attack tokens/slots** (TLOU, Arkham, Doom 2016, God of War): cap simultaneous attackers — WE HAVE THIS (rule 7).
- **Kung-fu circle / boss slots** (Arkham, AC): coordinator maintains ANGULAR SLOTS around the target; NPCs
  path to their SLOT, not to the target → natural surround, zero stacking. ← the missing piece #1.
- **Rotation/choreography** (Shadow of Mordor, TLOU coordinator): attacker "turns" end (post-swing / timeout /
  hit taken) → token passes to a posturing observer, cadence randomized. ← the missing piece #2.
- Avoidance (DetourCrowd/RVO) exists but is NOT the first tool: slots remove most path contention; Mover ×
  UCrowdFollowingComponent compatibility is unverified — defer until slots prove insufficient.

## Design (all owned by UAZ_HordeSubsystem — extends v2, no new architecture)
1. **FFightRing per prey** (subsystem map): center = prey, radius = RingDistanceCm; **6 angular slots**.
   - Slot assignment on role grant: nearest free slot by current angle from prey; stored in role state.
   - Per beat: write BB `SlotLocation` (vector) = prey + slot direction × radius (slots ROTATE with a slow
     angular drift ~10°/s for the circling feel; clamp to navmesh via ProjectPointToNavigation).
   - **BT change (HAND-EDIT)**: Ring MoveTo key → `SlotLocation` (accept ~75); BB gains `SlotLocation` key.
   - Active fighters keep pathing at the prey (accept AttackRange) — attackers charge, observers circle.
2. **Rotation**: role state gains `ActiveSinceSeconds`. Active hold = random 4–8s (per grant, seeded per
   controller). On expiry (checked in the beat): demote to Passive (goes to a slot), promote best observer
   (closest + facing bonus). Also demote-on-flinch option (hit taken while Active = lose turn) — knob, default off.
   Anti-ping-pong: MinPassiveSeconds ~2s before re-promotion eligibility.
3. **Observer life at the slot**: face prey (facing override), idle sway; every 6–12s randomly shuffle to an
   adjacent free slot (crossing = the TLOU circling read). Anim menace posture reads the Passive tag (already
   replicated).
4. **Anti-stacking assist** (phase 2, only if slots aren't enough): radial separation in the infected pawn's
   ProduceInput — push-away term when another infected capsule within ~90cm and moving same direction. NO
   DetourCrowd until Mover compat is proven.
5. **Disengage naturally**: existing rules untouched — death/escape releases everything (rules 8/9); rotation
   only adds the TIMED handover. All knobs → DA_ChalkieConfig in the config batch.

## Implementation order (next session)
1. FFightRing struct + slot math + SlotLocation BB writes in the subsystem beat (LC-safe? NEW struct/members =
   editor-closed build — batch with leftover task-#9 items: native UAZ_ChalkieAnimSet, DA_ChalkieConfig,
   AT_MeleeSweep wiring, State.Dead tag, TEMP-diag removal).
2. Hand-edit: BB key `SlotLocation`; Ring MoveTo re-key. (BT structure edits = HAND ONLY — proven rule.)
3. Rotation timers in AssignCombatRoles.
4. PIE choreography test: 3-5 zombies — 2 press, rest circle at slots, handovers every few seconds.

## Scalability doctrine (user question 2026-07-22: shooters/animals/multi-team — "does it fit?" YES)
- GENERIC already: the conductor pattern (facts→decisions→execution), role tags (deliberately State.Combat.*,
  not Infected.*), BB mirror keys, ApplyRole funnel, ranking-with-modifiers, slot allocation+hysteresis.
- PROFILE layer (per-archetype, → future DA_CrowdProfile in the config batch): role counts, slot GEOMETRY
  (melee ring | prey-RELATIVE biased ring for animals-circle-behind | cover/LOS slot provider for shooters
  — the one genuinely new component, EQS/cover query), turn/shuffle cadences.
- MULTI-TEAM: one crowd ledger per TeamId; registry generalizes AAZ_InfectedAIController* → small combatant
  interface (fresh target, team, BB, mid-committed-action, facing hook) WHEN the second faction exists.
- Sequencing rule: DO NOT pre-abstract — extract the interface when the shooter/animal archetype is real;
  until then only externalize knobs to the profile DA and mark the two seams in comments.

## Standing constraints reaffirmed this session
- BT STRUCTURE edits BY HAND ONLY (programmatic graph-null regen is banned — 4 crashes, see
  [[feedback_cpp_executescript_harness]] final verdict). Value tweaks via python remain fine.
- Proper long-term tool = AZEditor module linking BehaviorTreeEditor, driving real editor code paths (backlog).
