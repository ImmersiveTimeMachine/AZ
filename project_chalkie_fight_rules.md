---
name: project_chalkie_fight_rules
description: "★ THE Chalkie combat/engagement RULEBOOK (formalized 2026-07-21 at user request): 9 numbered rules, each with ONE code owner (file:function). Any combat behavior change starts by finding which rule it belongs to. Read before touching NPC combat/perception."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-21T04:27:42.373Z
---

# CHALK — Chalkie Fight Rulebook (v1, 2026-07-21)

> **⚠ OPEN BUG @ close (USER-CONFIRMED, task #12 = start here):** bystanders inside the noise sphere STILL don't converge on an active fight — the clear->set re-pin fix did NOT resolve it. Hypothesis was wrong/incomplete. Next session: evidence-first triage per task #12 (HEARD-line vs ArmInvestigation vs BT-edge discrimination; note the debug sphere radius 980 is STALE vs the raised carry ~2000).
> **STATE @ session end (commit `2d10a99`, pushed):** all 9 rules implemented + 13 confirmed audit fixes in. TWO ADVERSARIAL AGENT REPORTS ran (rules-edge-cases + GAS/BT-races; 25 findings; the confirmed ones fixed same-session — key roots: montage-task OnDestroy was a stub so bStopWhenAbilityEnds never worked; facing override fed a POSITION not a direction; corpses ate punches; investigate re-pin needed clear->set to edge-trigger a branch restart; death RM needed deferred Mover deactivate). **DEFERRED to batch (task #9/#12):** flinch replication (GameplayCue), client corpse freeze, co-op single-slot target memory, alert-beat flicker decay, 150-vs-180 stare dead-zone, corpse nav obstacle, token ledger→member, header default drift (source says Sight 800/Hearing 1500/Grace 3 — live BP truth is 500/700/4.5). NOISE TUNING live: punch carry 2000, scream 2500, knobs = BP vars (ImpactNoise* on punch BPs, Scream* on BP_AZ_Chalkie, HearingRange 700 on controller BP); engine formula VERIFIED: heard iff dist² ≤ (HearingRange·Loudness)², capped by MaxRange·Loudness (AISense_Hearing.cpp:147-152). PIE-UNVERIFIED at close: the investigate re-pin fix (screenshot case: bystanders marching to stale noise coords), the raised noise ranges, death-collapse travel — FIRST PIE next session. TEMP instrumentation still in code: [ChalkieDiag], [Noise] logs, [Vitals] HandleDamaged line, 980-radius yellow noise sphere, ABP divide-by-zero spam in editor (turn-chain BP math, dies with turn-controller v2).

Every rule has exactly ONE owner in code. Change a behavior = find its rule = edit its owner.
Thresholds marked (DA) migrate to DA_ChalkieConfig in the native batch.

| # | Rule | Owner |
|---|---|---|
| 1 | **Sight commits.** Cone (70° half) + LOS + range 500; new targets get the ALERT BEAT (0.6s freeze-face) unless within InstantDetectRange 350; crouched targets invisible beyond 250 until engaged. | `AZ_InfectedAIController::UpdatePerception` |
| 2 | **Pain locks, unconditionally.** Any damaging hit from a hostile = instant target lock on the causer — no cone, no range, no beat. Routed vitals→pawn→controller because the attribute-change delegate loses the causer (GEModData null on direct sets). | `UAZ_VitalsAttributeSet::PostGameplayEffectExecute` → `AAZ_PawnMoverInfectedCharacter::HandleDamaged` → `NotifyDamagedBy` |
| 3 | **Point-blank can't be lost.** An ENGAGED target within InstantDetectRange stays fresh outside the cone (ears/touch at arm's length). Never acquires. | `UpdatePerception` proximity-retention block |
| 4 | **Hearing draws the unengaged.** Heard hostile noise → investigation at the noise location. Noise TAG sets urgency: footsteps=calm walk-over, Combat/Scream=urgent run. Carry = listener HearingRange (700) × event loudness: steps ≤700, punch impacts ~980, stagger screams ~1400. Every landed hit re-pins the fight's location. | receive: `OnTargetPerceptionUpdated`; emit: `GA_MeleeAttack::OnMontageEvent` (Combat), `HandleDamaged` (Scream), hero `ReportMovementNoise` (steps) |
| 5 | **The engaged ignore rumors.** A Chalkie with a fresh target discards ALL investigation prompts (pack alerts, noises). Its own fresh→lost edge still arms normally. | `ArmInvestigation` engaged-guard |
| 6 | **Move XOR fight.** Swings need stillness (<80 speed) + facing (60° cone) + reach (180; 230 vs rushers; never vs fast-fleers) + a pack token; movement kills the swing (StopMovement at start, >120 mid-bite = cancel); target escaping 240 = break-off; full-body flinch replaces any montage. | `UAZ_BTTask_ZombieAttack` gates + `HandleDamaged` |
| 7 | **Max 2 attackers per prey — roles are PUBLISHED state.** Engagement tokens from the horde subsystem. **DESIGN COMMITTED 2026-07-21 (implement AFTER the adversarial audit, needs restart for tags): token state = GAS role tags, subsystem-owned exclusively** — `State.Combat.Engaged.Active` (holds token: press the attack) / `State.Combat.Engaged.Passive` (live target, no token: hold a ~350-450 ring, face prey, strafe, retry each beat). Subtags under State.Infected.Aggressive (phase = knowledge, role = permission). Subsystem swaps tags on every grant/release (AddStateTag/RemoveStateTag — replicated) so tokens⇄tags can never disagree. Consumers: BT Chase forks on a bActiveFighter BB mirror (Passive = ring MoveTo + hold; attack task doubles as token retry), AnimInstance reads tags for the menace display (Atk_Loop posture at ring = TLOU circling pack), melee GA gains ActivationRequiredTags Engaged.Active; future GEs can force roles (taunt/fear). Ring distance + MaxAttackersPerPrey → DA_ChalkieConfig. | `UAZ_HordeSubsystem::Request/ReleaseAttackToken` (+ role-tag publishing) |
| 8 | **Every hit staggers (and shouts).** Full variant KnockBack montage from the top, RM drives the capsule, cancels the victim's attack, screams (rule 4). Single-zombie stun-lock accepted — danger lives in the pack. | `HandleDamaged` |
| 9 | **Real escape = memory decay.** Target stays fresh LoseTargetGraceSeconds (4.5) after last stimulus; then fresh→lost edge arms an URGENT investigation at last-known; search loop; then home. Deaths alert the pack at the killer's position (rules 4/5 apply to listeners). | `UpdatePerception` stale-clear + service edge + `GA_Death` pack alert |

## Known accepted trade-offs
- Solo zombie is stun-lockable (rule 8) — pack pressure is the difficulty budget.
- Standard Chalkie (sprint 273) cannot catch a running hero (375) — Sprinter variant (421) can. Speed = per-variant data.
- Corpses persist and their capsules block (user: don't touch collision) — doorway-plug risk logged.
- Hero at 0 HP: Event.Death fires, nothing listens yet (hero death = task #12).

## GAS pitfalls encoded here (cost real debugging)
- Attribute-change delegates from DIRECT sets carry GEModData=null → never source a causer there (rule 2's routing exists because of this).
- BT latent task × ability cancel: unbind delegates BEFORE CancelAbilityHandle (sync re-entry wedges the tree).
- Loaded CDOs (BP and native) don't see Live-Coded ctor-default changes — set live values via python on the CDO.
