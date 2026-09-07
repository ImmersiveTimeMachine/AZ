# Punch contact with walls — implementation, 2026-09-05

## Behavior change — 2026-09-06

The user requested no shortened punch when there is insufficient room. Environment preflight now
rejects the attempt before montage playback if the full planned swing cannot fit. The scenery-specific
position correction is also removed: live logs from the first iteration showed it approving swings that
then immediately hit the wall (`WARP` followed by `CONTACT_CANCELLED`). Enemy-target alignment remains
subject to the same full-swing clearance check. A new obstruction during a swing cancels the ability and releases its
movement and any unconfirmed paired reaction. Neither path selects a short jab/retract montage.

The saved `BlockedPunchMontages` arrays remain as the existing hero-ability opt-in for environment
preflight; clearing them would disable that check. Their assets are no longer selected as responses.
The September 5 validation below documents the earlier behavior, not validation of this change.
Current verification: the first iteration compiled and its rejection/cancellation paths appeared in live
PIE logs. The final stricter change also passed Rider error checks; UBT reported `Result: Succeeded`, and
Live Coding confirmed the AZ patch at 2026-09-06 23:39:30 UTC. Rider's build wrapper reported failure
without diagnostics despite those engine success records. The final behavior still needs a visual PIE
check. No automated tests were added.

Resume verification: the editor is connected to AZ and idle. The authoritative UBT log still reports
`Result: Succeeded`; Live Coding records the AZ patch as successful at 23:39:30 UTC. Fresh Punch L,
Punch R and HeavyStrike instances each retain four response references, so environment preflight remains
enabled (their sweep radius is 12 cm, giving an 8 cm base trajectory-clearance radius). The post-patch
PIE log at 23:40:20 UTC records an unpaired Punch L fallback immediately followed by `REJECTED` against
`StaticMeshActor_368`, with no subsequent melee-playback log for that attempt. This confirms the new
rejection path executes; clear-space swings and the final rendered wall behavior remain unverified.

Expected PIE evidence: `[MeleeWall] ... REJECTED ... reason=insufficient clearance` with no attack montage
when too close; full punches in clear space; `[MeleeWall] ... CONTACT_CANCELLED` if scenery interrupts an
already-started swing. No `SHORTEN` response should occur.

## September 5 baseline

Status: implemented and enabled on Punch L, Punch R and HeavyStrike. Full native build succeeded; both native automation tests passed; fresh ability instances verified the saved response references. The user tested in PIE and confirmed the visible fix works on 2026-09-05. The initial socket-monitor / montage-rewind prototype was removed after review.

## Implementation and validation status

- Public native helpers in `Source/AZ/Public/AbilitySystem/AZ_MeleeEnvironment.h`, corresponding implementation under `Private/AbilitySystem`. Uses `FAZ_` types and static functions; no new gameplay namespace or utility UObject.
- Existing GAS montage task feeds the live MHC `FullBody` slot. `CHT_v2_CharacterAnimations` and BlendStack continue to own the underlying locomotion pose. No AnimGraph or chooser edits.
- Ability preflight samples the active mesh's attack sockets, validates scenery/capsule clearance, attempts bounded unpaired translation warping, then selects the longest fitting static shortened jab/retract montage. Facing changes use a conservative expanded envelope.
- Unified runtime sweep resolves scenery and victims in temporal order. Teardown discards contacts; only explicit hit-window closure flushes its final segment.
- Paired strikes validate both contact capsule paths and hero limb trajectory. Actual victim contact, rather than a probe timer, confirms the pair. Alignment cleanup uses generation tokens and montage instance identity.
- `FLayeredMove_AZ_MeleeAlignment` uses priority 1 above default locomotion root motion (0); tests exercise engine mixer order, cloning and serialization.
- New wall source copies have constant root tracks; static montage time advances normally through positive advance and negative-rate retract segments. Original attack assets remain intact.
- Native validation: Unreal Header Tool passed; full `AZEditor Win64 Development` build reported `Result: Succeeded`. `AZ.Melee.Environment.Clearance` and `AZ.Melee.Alignment.Priority` both passed (2 passed, 0 failed). The editor also emitted an unrelated CMC preview tick-prerequisite warning during these tests.
- Content validation: all three ability Blueprints compiled through the dedicated MCP; newly constructed ability instances retained four correct static montage references each; packages saved.
- PIE evidence: logs show left/right shortened responses and bounded wall corrections (approximately 14 cm), followed by runtime scenery contact where needed. The user confirmed the tested behavior works. This does not claim that every case in the broader acceptance matrix below has been exercised.

Limits for future coverage: trajectory sampling excludes final montage blending and MetaHuman postprocess; warping preflight is a finite prediction, not a second runtime movement solver. When an obstruction already overlaps the displayed fist, all recoil candidates can be rejected and the fallback is cancellation. Holding input in this no-clear-response case can retry the refused swing through the existing input rail. Dynamic obstructions, network play and slow-frame cases remain outside the confirmed user test.

MCP check after editor restart: UnrealClaude connected to AZ 5.8.2; Unreal MCP tool registry and test runner responded; Rider MCP completed initialization and returned 77 tools, and RiderLink `ue_health` reported AZ connected. Rider tools were not exposed in this chat's initial tool catalogue, so its health check used the configured local MCP endpoint directly. UI status shown as unknown was not evidence of server failure.

For CHALK's current montage-based, no-procedural-IK combat, use **environment-aware attack placement through the existing Motion Warping component, backed by collision-ordered contact resolution and a blocked-punch response**. An arbitrary change to stand-off distance or a capsule collision setting does not cover the problem.

## Existing decisions and evidence

The current Claude knowledge is under `C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/`. The repository's `docs/ai-memory/` export and the old `C--UE57-Games-AZ` folder are substantially older.

- [September 3 strike plan](C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/project_psia_heavy_strike_plan.md): NEXT STEPS item 3 already calls for obstacle-aware unpaired strikes, registering a scenery warp target in the no-hostile branch, plus clearance validation for the paired victim transform. This was an open design item, not a shipped fix.
- [Motion Warping findings](C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/project_motion_warping.md): Mover's root-motion attribute drive invokes Motion Warping; in-place clips can receive synthesized translation. Their speed clamp is ineffective, so the existing bounded correction must remain. Travelling clips must never be warped backward. Translation and facing must remain separate, and their montage windows must retain distinct start/end times to avoid engine deduplication.
- [Punch contact progress](C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/project_punch_contact_next_session.md): previous contact, facing, input buffering and back-step fixes have specific measured reasons. Do not retune them as a wall workaround.
- [MetaHuman/Mover decision](C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory/project_mover_metahuman_2026-08-31.md) and `Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp`: procedural combat IK is deliberately off.

Baseline source inspection before this implementation confirmed:

- `UAZ_GA_MeleeAttack::FindWarpTarget` and `UAZ_AT_MeleeSweep` query `ECC_Pawn`; neither resolves scenery as a contact.
- With no hostile target, the ability removes its warp targets and plays the authored attack, including its travel.
- `UAZ_GA_StrikeInteraction::PlayPairedStrike` clears ordinary melee warp targets because the interaction pair owns alignment. Adding a competing wall warp to that path would break its ownership model.
- Pair selection limits victim displacement, but does not validate the environment along both actors' paths and at their destinations.

Live editor inspection confirmed the MHC hero uses `AZ_ABP_MoverHero_MHC`. Its FullBody montage slot feeds PoseHistory, then the grabbed animation layer, then output. The body also has a MetaHuman postprocess AnimBP. The existing grab IK nodes are disabled; there is no generic wall-hand constraint.

## Proposed behavior

| Situation | Outcome |
| --- | --- |
| Clear space | Existing punch, timing and input behavior. |
| Wall within attack reach; small safe correction can make the animation fit | Register an environment warp target and finish the correction by the existing contact window. |
| Too close, rear wall, corner, or swing cannot fit | Choose a short blocked/recoil response instead of forcing the full extension or a large backward slide. |
| Enemy behind a wall | Reject that target; no damage or paired reaction through scenery. |
| Paired attack lacks clearance for either participant | Try another fitting variant, then use the environment-aware unpaired fallback. |
| Obstruction appears after activation | Resolve the actual blocking contact, end forward transport and the damage window, and enter the blocked response. |

## Implementation boundaries

1. **Measure the actual attack envelope.** Inspect current montage windows and CDO tuning, then sample wrist/knuckle or foot trajectories through extension and recovery on the active MetaHuman. Earlier SurvivalMan measurements are a starting hypothesis, not current-rig proof. Include root displacement, montage blend-in, and fist thickness. A root path or the single peak-contact pose alone is insufficient for a curved hook or a moving recovery.

2. **Add scenery to target planning.** When no reachable hostile is selected, find a blocking surface in the selected attack's corridor. Derive the root destination from that surface and this clip's measured striking reach. Do not reuse an enemy stand-off blindly: enemy tuning includes the victim's capsule radius and damage sweep tolerance. Keep the facing target directed toward the intended contact, separate from the root stand-off. Validate the capsule path and destination, including the rear space for an in-place correction. Preserve existing approach/back-step bounds and the rule that travelling clips cannot retreat. No fit selects the blocked response.

3. **Validate paired geometry before starting either half.** Check both actors' proposed paths, destination occupancy and strike space. Keep PSIA as the sole alignment owner. Revalidate moving geometry during execution. Pair-related close-in and hold moves need activation-scoped cancellation, so aborting cannot leave either actor sliding under an old move.

4. **Resolve contacts in one place.** Extend the existing melee contact resolver to gather scenery and victim hits across the same limb segments, then choose the earliest valid contact. A wall before a victim blocks damage; a victim before a later wall must not lose a legitimate hit because a separate task happened to tick first. Handle a limb already beyond a thin wall when the window opens. Preserve authoritative damage and predict only the local visual response.

5. **Use an animation response to a blocked strike.** Transition before deep penetration into a short, clearance-checked blocked/recovery pose or montage. Its clip/section events own the phase; avoid a second wall-clock contact timer. The current asset searches found fist hit-reaction clips but did not establish a suitable wall-punch recoil. Preview candidates or author a dedicated response; a receiving-hit clip is not automatically an appropriate fist recoil. Preserve legitimate victim reactions only after confirmed contact.

## Why Motion Warping is part of the solution

Motion Warping adjusts root motion, including when its warp point comes from a bone. It can place an authored fist correctly by positioning the body, provided there is enough room. It does not independently bend the elbow or constrain the whole arm to a wall surface. This matches both the local engine implementation and [Epic's Motion Warping documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine).

For exact, persistent hand contact across arbitrary surfaces, a small wall-specific arm constraint would be the additional technique. [Two Bone IK](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-two-bone-ik-in-unreal-engine) can place a hand while controlling elbow bend. That would be an explicit extension to the current no-IK design, evaluated after the montage and against the downstream MetaHuman postprocess. It is not required to begin the existing obstacle-aware strike plan, and it should not silently re-enable grab IK.

The discarded prototype only sampled already-evaluated sockets and rewound the montage clock. That does not restore the body transform or refresh the pose already evaluated for the frame. A previously clear clip time may intersect a wall after a lunge. Its separate wall and damage tasks also lost temporal contact ordering. These are reasons to replace the prototype, not to claim the bug fixed.

## Rollout and acceptance

First reproduce and instrument a single unpaired standing jab near a flat wall. Log the selected clip, raw/blocked target, proposed and accepted root destinations, first contact type and montage time. Compare capsule motion and the final rendered knuckle position. Then add moving/heavy punches and pairs, one failure axis per test.

Acceptance cases: left/right jab against a flat wall; oblique wall and corner; point-blank wall with another wall behind; moving/heavy punch; enemy behind a thin wall; reachable enemy in front of a wall; paired victim whose destination is occupied; obstruction appearing mid-strike; interrupt/death/retrigger; hit-stop and slow frame rates; kick variant with a planted foot on the floor.

Pass means no visible wall penetration in the tested final poses, no damage/reaction through scenery, no backward-playing moving punch, no stale close-in/hold movement after abort, and unchanged clear-space attacks/input cadence. Compilation alone is not acceptance. Finite authored poses do not guarantee exact contact for every possible surface; any residual cases determine whether wall-specific IK is justified.
