**AZ / CHALK — Relaxed and Neutral mantle implementation handoff**

Prepared September 13, 2026 Toronto time (inspection September 14 UTC). This is an implementation proposal; no gameplay code or animation assets were changed. Inspection receipts: [C:/UnrealEngine/Games/AZ/docs/design-briefs/mantle-assets-audit.json](C:/UnrealEngine/Games/AZ/docs/design-briefs/mantle-assets-audit.json).

**Task for the implementing AI**

Implement a contextual mantle for the current MetaHuman Mover hero. Pressing Jump near a valid ledge attempts a mantle; otherwise preserve the existing normal jump. The user's latest scope is **Relaxed and Neutral first, walking and running later**. This plan interprets that as stationary, standing entry for both styles first. Use the existing `_stand_` variants for this first delivery; the originally requested `_walk_` variants belong to the following phase.

Treat style (Relaxed/Neutral), approach (Stand/Walk/Run), and foot variant as separate selection dimensions. Keep all player animation playback at **1.0**. Implement a complete stationary mantle slice, then stop for the user's gameplay review before extending it to walk/run.

Confirmed initial boundaries: grounded standing hero, stationary authored `LevelBlock_Traversable` obstacle, approximately one-meter ascent, sufficiently broad walkable top and forward-facing entry. Hands must be free during the mantle. The user selected automatic holster -> mantle -> draw the same weapon in relaxed posture, using the equipment flow below. Ordinary geometry detection, moving platforms, crouched entry, running/sprinting entry, high climbs, vaults, hurdles and ledge hanging remain later work. Stationary does not mean component mobility must be Static: the placed authored blocks are Movable/WorldDynamic.

September 14 clarification: the user requested design advice, a read-only phase-0 level check and an updated handoff, with special attention to existing running-barrier/head-hit behavior. All five gameplay questions below have now been answered. No gameplay changes were performed in this planning turn.

**Gameplay decisions from the September 14 phase-0 check**

- **Detection — user selected:** placed traversable actors first; ordinary geometry later. Reuse existing AZ front-hit infrastructure and consume authored ledges through a small provider adapter. Preserve the provider boundary so generic geometry can be added without replacing the action executor.
- **Armed — user selected:** validate the ledge first, then holster through the existing equipment system, mantle, and draw the same weapon in relaxed posture. Revalidate the ledge after holstering. Add a temporary-carried phase to the current transaction instead of replacing selection with fists and equipping again. This replaces the earlier recommendation to refuse all hand-held weapons.
- **Scope — user selected:** stationary Relaxed/Neutral mantles only; vault/hurdle and walk/run later. Detect sufficient top support now, and classify narrow walls as unsupported mantle/possible future vault targets. The inspected level contains 20 cm-thick walls; do not force a standing mantle onto their narrow tops. Thickness alone does not prove that a vault is safe: its far-side landing/path must also be checked when vault is implemented.
- **Style — user selected:** expose an explicit Relaxed/Neutral setting for visual comparison before selecting an AZ stance mapping. No confirmed native Relaxed/Neutral stance signal was found in the inspected locomotion context. Standing/crouching, gait, strafe/aim and weapon profile are different signals; avoid inventing a combat meaning for these two source styles.
- **Already-playing environmental impact — user selected:** let the current barrier/head-hit reaction finish. A timely valid mantle prevents new reactions. Do not start a mantle or an interrupting fallback jump from a press made during the already-committed reaction; require a fresh press after it finishes rather than silently queueing a delayed mantle.

**Verified starting point**

- Current editor is AZ on UE 5.8.3. Implement in `AAZ_PawnMoverHeroCharacter`, `UAZ_PawnMoverComponent`, and `UAZ_MoverAnimInstance`. The older `AAZ_HeroPawn` architecture in some skill/memory files is stale.
- Live imported GASP reference is `/Game/GameAnimationSample/Blueprints/AC_TraversalLogic`. Discover current paths rather than relying on the old `/Game/Blueprints` prefix. Do not use a separate GASP project/editor.
- Hero already owns a Motion Warping component. GAS montage playback, finite Mover root-motion driving, Chooser, and PoseSearch infrastructure exist.
- Current normal jump defaults to **root-motion rise followed by physics falling**. Preserve that behavior. Do not replace it with a new physics-only jump.
- Existing animation enum already contains `EAZ_MovementMode::Traversing`, but movement-mode mapping/state-machine handling need implementation.
- Current MHC AnimBP is `/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC`. Its September 13 saved graph receipt shows the `FullBody` slot after weapon layers and before procedural feet/hands. Re-read the active mesh/AnimBP before editing.

**0. Placed-level inspection completed; runtime clearance/animation verification remains**

Read-only editor inspection found **13** `LevelBlock_Traversable_C` actors in `/Game/AZ/Maps/L_001`. All have four populated ledge splines and opposite pairs, `MinLedgeWidth=60`, query-and-physics mesh collision, and Block responses to Pawn, WorldStatic and Visibility. None of their meshes simulates physics. All are Movable with WorldDynamic object type. Do not require static mobility or use a WorldStatic-only object query that would silently miss them; a channel query and object-type query have different semantics.

The inspected MHC hero Blueprint capsule has radius **30 cm**, half-height **90 cm**. Full actor bounds include editor billboards and are misleading: the dimensions below use **colliding-only bounds**.

| Actor label | World-aligned collision size X×Y×Z, cm | Bottom/top world Z, cm | Phase-0 use |
|---|---|---|---|
| `LevelBlock_Traversable4` | 200×400×100 | 0 / 100 | Primary broad one-meter mantle candidate |
| `LevelBlock_Traversable12` | 200×700×100 | 0 / 100 | Second broad candidate; check higher stacked geometry on its far portion |
| `LevelBlock_Traversable9`, `15`, `8` | 200×20×100 | 0 / 100 | Narrow-top mantle rejection; future vault candidates |
| `LevelBlock_Traversable2` | 200×50×100 | 0 / 100 | Narrower than capsule diameter; do not accept by a single top ray |
| `LevelBlock_Traversable11` | 100×50×100 | 0 / 100 | Narrow-top/corner rejection |
| `LevelBlock_Traversable17` | 200×50×100 | 20 / 120 | Narrow/elevated rejection; approach height must use actual support |
| `LevelBlock_Traversable24`, `25` | 200×600×250 | 0 / 250 | High-climb cases, outside the one-meter slice from ground |
| `LevelBlock_Traversable13` | 200×400×250 | 100 / 350 | Elevated/high-climb case; height depends on actual approach support |
| `LevelBlock_Traversable7` | 200×20×100 | 250 / 350 | Elevated thin wall; later traversal case |
| `LevelBlock_Traversable14` | 200×20×100 | 350 / 450 | Elevated thin wall; later traversal case |

For the primary candidate `LevelBlock_Traversable4`, colliding footprint is approximately X=-3418.884..-3218.884, Y=2714.430..3114.430, top Z=100. Its south/front lip is a useful starting side for the user gameplay check. These are structural candidate findings, not proof of clear support, headroom or a collision-free animated path: no runtime traversal or PIE was performed.

Live reference `/Game/GameAnimationSample/Levels/LevelPrototyping/LevelBlock_Traversable` provides `GetLedgeTransforms(HitLocation, ActorLocation, S_TraversalCheckResult in/out)`. It chooses a closest authored spline, enforces width and fills front/back ledge flags/locations/normals. The front normal is the sampled spline's **world up vector**, not world Z or its tangent. Opposite pairs are 1↔2 and 3↔4. No animation/skeleton dependency exists in this provider, but its Blueprint result struct is not interchangeable with a native FAZ struct. Use a small Blueprint-to-native output adapter or deliberately read its authored splines; start with a fresh result each query. It does not validate capsule path, height, headroom or landing support. Check scale/width semantics before copying its clamping math.

No additional level actors are needed to begin the standing-mantle integration. Remaining preflight work is the missing `attach` warp-reference calibration, FullBody slot/notify preparation, provider adapter and actual approach/path/destination queries. Preserve the user's placements and do not resize/reposition them automatically. Full instance receipt: [C:/UnrealEngine/Games/AZ/docs/design-briefs/mantle-level-phase0-audit.json](C:/UnrealEngine/Games/AZ/docs/design-briefs/mantle-level-phase0-audit.json).

**1. Audit and prepare only the standing mantle assets**

The initial four montages exist under `/Game/AZ/Assets/GASP/`:

| Montage name after `AZRTG_GASP_AM_M_` | Actual referenced sequence after `AZRTG_GASP_M_` | Montage duration |
|---|---|---|
| `Relaxed_Traversal_Mantle_1_0_stand_F_Lfoot` | `Neutral_Traversal_Mantle_1_0_stand_F_Rfoot` | 2.0 s |
| `Relaxed_Traversal_Mantle_1_0_stand_F_Rfoot` | `Relaxed_Traversal_Mantle_1_0_stand_F_Lfoot` | 2.0 s, using first 2 s of a 3.5 s sequence |
| `Neutral_Traversal_Mantle_1_0_stand_F_Lfoot` | `Neutral_Traversal_Mantle_1_0_stand_F_Lfoot` | 2.0 s |
| `Neutral_Traversal_Mantle_1_0_stand_F_Rfoot` | `Neutral_Traversal_Mantle_1_0_stand_F_Rfoot` | 2.0 s |

These unusual standing references also exist in the imported GASP source montages. They are **not established retargeting mistakes**. Check actual pose, planted foot, segment range and notify timing before changing references. Do not blindly match sequence and montage filenames. The Relaxed walk Rfoot montage also references the Relaxed walk Lfoot sequence; retain this finding for the later walk audit.

All four standing montages currently use `DefaultSlot`. Their referenced sequences have root motion enabled, and sampled root tracks rise approximately 99.15 cm. The first root samples have substantial horizontal offsets: work with relative transforms and the Mover mesh adapter, not an assumption that the animation starts at identity.

Required preparation:

- Create gameplay-owned montage copies, proposed destination `/Game/AZ/Blueprints/Animation/MHC/Traversal/`. Keep retargeted sequences/source montages intact unless a specific verified repair is required. Route copies through the existing `FullBody` slot; verify skeleton, slot group, segment rate, sequence rate and montage rate.
- **Resolve the missing warp reference before proceeding.** Both Motion Warping windows inspected on Neutral Stand Lfoot use target `FrontLedge`, Bone provider, bone `attach`, translation/rotation enabled, and Z enabled. The target MetaHuman skeleton `/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel` has 342 bones and **no `attach` bone**. Inspect every selected montage's modifiers, not just this representative one.
- Preferred narrow solution: author a per-window **Static warp-point transform** in the copied montages. Derive it from the source contact anchor and calibrate against the retargeted root/hands at that window's endpoint. Unreal defines this transform in the root-track coordinate space. Use engine conversion helpers; inspect the local implementation before computing it. Avoid adding bones to the shared MetaHuman skeleton or substituting `hand_l` without proving equivalent contact semantics.
- Preserve the two distinct warp windows and their intended contact phases. Neutral Stand Lfoot windows are approximately 0.141–0.365 s and 0.365–0.699 s; Neutral Stand Rfoot approximately 0.225–0.449 s and 0.450–0.800 s. Other variants differ. Do not apply one window timing to every montage.
- Inspect `BP_NotifyState_TraversalBlendOut` and other copied GASP notifies for interface dependencies. Replace needed behavior in gameplay-owned assets with an AZ-owned completion/handoff contract. Do not depend on a notify silently casting to a GASP AnimBP or character. Preserve useful source timing; do not assume the full 2 s or the first blend-out callback is the correct locomotion handoff.

Deliver an asset mapping/calibration receipt before wiring gameplay. A filename and `EnableRootMotion=true` alone are insufficient acceptance evidence.

**2. Add a small traversal component and data-driven selection**

Proposed new types: `UAZ_TraversalComponent`, `FAZ_MantleCandidate`, `FAZ_MantleSettings`, and `UAZ_GA_Mantle`. Follow current AZ folders/naming. Keep geometry detection, selection data and action execution separable.

Candidate data should include weak target component, ledge location/normal, candidate landing capsule transform, measured height/top clearance, style, approach, foot/pose observation and selected montage/start time. Action identity and owner must accompany a committed request. Settings expose probe dimensions, facing tolerance, supported height/distance bands, clearance margins and debug drawing. Derive values from the real capsule and calibrated clips; `_1_0` is not proof of a broad supported range.

Use a dedicated AZ traversal Chooser/data configuration. Initially populate only standing Relaxed/Neutral rows. No match returns a failure reason. Per the user's choice, expose a Relaxed/Neutral setting, initially Relaxed, so both can be compared before mapping gameplay stance. Do not automatically select Neutral from aiming or an equipped weapon.

Latch the selected style/foot/montage once. Stationary contact curves can be ambiguous; use verified entry-pose/foot semantics and a deterministic configured fallback. At this stage, a calibrated start at time zero is acceptable if the idle blend is sound. Do not import GASP's entire pose-matching schema just for four stationary entries. Later pose-based start selection must remain inside a validated entry interval before required contact windows.

**3. Validate an authored ledge; keep a provider boundary for ordinary geometry later**

On a fresh eligible Jump press, probe forward from the standing capsule, find a blocking face on an allowed authored traversable actor, then obtain its front/back ledge data. Independently check the real top surface. Validate facing, height above capsule feet, walkable top, ascent/head clearance and a destination capsule sufficiently inside the top. Check width/support around the destination so a thin rail or narrow corner cannot pass on one successful ray. The later generic provider can derive its lip from geometry queries and return the same candidate representation.

For the stationary phase use actor forward and a speed/entry gate; do not require nonzero movement input. Preserve raw input/approach information for later phases because AZ's obstacle clamp can zero locomotion intent when facing a wall.

For the first delivery, follow GASP's authored `LevelBlock_Traversable` ledge-provider boundary. **AZ already has more than a blank forward trace:** `UAZ_ObstacleSensorComponent` makes low/chest/head sphere sweeps, checks local base/top height to ignore steps, and produces Brace/Stumble/HeadHit. `UAZ_MovementDirectionCapabilityComponent` separately sweeps a shortened capsule and constrains ordinary walking intent. Reuse their capsule/collision conventions, hit/query helpers and suitable fresh observations. Keep their different purposes and shapes intact; three reaction bands do not replace a landing capsule test.

Extend a common read-only obstacle observation/helper boundary as needed to retain full `FHitResult`, primitive component, probe direction, sample time/pose and measured surfaces. The sensor currently publishes actor/normal/distance rather than a complete reusable ledge candidate. Avoid another continuously ticking duplicate sensor. Perform the extra top/depth/head/path/destination queries only for a traversal attempt; refresh cached hits if their direction/age/actor pose is unsuitable. A stationary Jump must work even when the reaction sensor is inactive: that sensor clears when there is no raw movement intent or the mode is not Walking.

Preserve the distinction between **obstacle height above its local base**, used by existing step/reaction suppression, and **height above the hero's feet**, needed for mantle reach. Reusing one scalar for both would regress stairs and ramp-to-platform transitions. Align traversal obstruction/clearance queries with the capsule's actual effective collision responses; the sensor defaults to WorldStatic while movement capability can use the capsule response container.

Prefer a fully colliding, swept ascent. If the authored path requires a temporary exemption for the touched obstacle component, scope it to that exact component, prevalidate the ascent/destination and retain collision with all other geometry. Never disable the whole capsule. Restore the exemption before evaluating final floor support. Reject moving/removed targets in this first phase.

**4. Arbitrate Jump exactly once**

Use one input owner/router for `Input.Action.Jump`. Choose mantle before requesting the ordinary jump and before installing the jump ability's landing-wait lifecycle. A separate `UAZ_GA_Mantle` owns traversal; do not give two independently activated abilities the same input tag.

Routing contract: first respect hard gameplay locks, an active traversal and a previously committed environmental reaction. Otherwise, eligible candidate plus successful mantle activation consumes the press; no supported candidate or failure before committing the action routes to the existing jump if its normal gates permit it. A pending network decision is not a failure. Once a mantle/preparation has begun, interruption performs cleanup/recovery and never emits a delayed fallback jump. Holding/releasing Jump must neither repeat the mantle nor leave jump pressed after completion.

Add a native traversal state/tag using the current runtime `DeclareAbilityTags()` pattern and grant the configured ability through the hero's existing `StartupAbilities`. Block incompatible movement/combat/equipment actions for its duration, while preserving death and required cancellation. Respect existing jump/aim restrictions and input ownership.

**4a. Resolve barrier reactions and traversal through one explicit priority decision**

An obstacle being ahead or walking into it being blocked is expected at a mantle target. Neither `bObstacleAhead`, `bBlockedThisQuery`, zero clamped walking intent nor a new uncommitted sensor trigger may alone reject a geometrically valid mantle. A valid contextual request is evaluated independently of those ordinary walking results, before the frame commits a new reaction clip or the ordinary jump. A reaction committed before the press is a separate action lock and finishes first, per the user's choice. Implement explicit ownership/order, not a dependency on incidental component tick order.

| Situation | Required result |
|---|---|
| Ordinary approach without a valid accepted traversal request | Existing wall slide/stop, Brace, Stumble and HeadHit behavior continues |
| Jump plus valid mantle target | Accepted mantle/preparation owns the action; suppress expected target-contact reactions and consume Jump |
| New cosmetic reaction in the same decision frame as accepted mantle | Mantle wins; clear the prevented reaction's pending/latching state |
| Cosmetic environmental flinch committed before the new Jump press | Let it finish; no interrupting mantle/fallback jump or automatic late mantle; require a fresh press |
| Blocked ceiling, ascent path or landing | Reject before commitment; normal jump retains its existing physical collision rules |
| Unexpected obstruction during the mantle | Traversal abort/recovery owns the response; do not layer a run-into-wall/head-hit clip over the mantle |
| Death, grab or gameplay damage reaction | Existing high-priority gameplay interruption policy remains effective |

The **three independent reaction layers** all need coordination: sensor `LatchedEntry`/0.2 s trigger/approach peak, AnimInstance `LatchedReaction`/`ChooserContext.Reaction`/push bookkeeping, and locomotion SM `ReactionEndTime`. Disabling the sensor is insufficient: it already clears outside Walking, but the animation latch and clip-held SM reaction can survive. Add explicit action-scoped suppression/reset hooks and a Traversing branch before reaction/jump phase selection. Hand off thread-owned state through the existing game-thread/animation snapshot boundary; do not mutate worker-owned state from arbitrary callbacks.

On accepted entry, clear stale/uncommitted reaction/transition work before queuing mantle root motion; do not cancel a still-playing committed reaction to reach this point. Prevent a late reaction-clip callback from rearming the SM hold under traversal; scope it to the active owner/generation. During preparation, suppress only the intended target's cosmetic contact and retain physical walking/clamping and unrelated hazard checks. During Traversing, its movement mode owns the motion. Do not pass authored movement through the walking-intent clamp or require a positive `bIsMoving` flag to sustain it.

At completion/abort, clear stale trigger/peak data and require a fresh approach/contact before another environmental reaction. Recompute walking capability at the actual new position; do not restore a cached wall normal from the departure ledge. Suppression must end with the action, not disable reactions permanently for that actor.

**4b. Extend the existing holster transaction for armed entry**

Recommended phases: `Validate -> Holstering -> CarriedForTraversal -> Revalidate -> Traversing -> Drawing -> Complete`. Skip holstering/drawing when hands were already free. Preparation stays grounded; enter Traversing only when both hands-free readiness and the fresh geometry check succeed. A move away or changed target during preparation cancels/reconciles the request; never mantle toward a stale lip. Once preparation has consumed the press, cancellation does not produce a delayed surprise jump.

The current equipment component already has `BuildSwitchPhase`, `StartSwitchPhase`, `ApplySwitchSocket`, attachment blending, phase IDs and observer cosmetics. Its normal tick advances Holster -> Draw -> CommitSelection, and these helpers are private. There is **no public temporary-stow/hold-carried/restore API yet**. Add that small owner-aware API within equipment, using the existing executor; illustrative operations are `BeginContextualStow(ActionId)`, a hands-free completion receipt, `RestoreContextualStow(ActionId)` and `CancelContextualStow(ActionId, Reason)`.

Preserve selected inventory item, magazine, ability grants and selection generation through temporary stow. Capture action ID, item ID, selection generation, weapon actor, pawn, mesh and ASC; verify them before each callback/restore. Wait for authored holster recovery and socket transfer, then explicitly stop that cosmetic montage before mantle takes the hands. Restore only after mantle has released the hands/full-body pose, and only if the same selection is still valid. Finish relaxed; require fresh aim/fire input. Reuse actor-based cosmetic replication so observers see both phases.

**Do not add traversal blindly to `EquipmentBlockTags()`.** `OnGateTagChanged` currently cancels weapon switching for those tags, and `IsSwitchContextValid()` rejects hard-blocked transitions. That would cancel the mantle's own holster. Separate true hard interrupts from contextual reservations: block external equip/fire requests while allowing only the matching action's internal stow/restore. Death/grab/stagger must still interrupt it. Teach `ReconcilePresentation` and `RefreshCarryPresentation` to respect temporary carry for both rifle and pistol, so they cannot reattach the selected weapon to a hand during mantle. Inventory removal, owner change or death must invalidate restoration before callbacks; never redraw an obsolete weapon.

**5. Execute through a distinct Mover Traversing mode**

Register a dedicated `Traversing` mode, reusing/subclassing the swept gravity-free mechanics of `UAZ_PawnMovementMode_RMAction` as appropriate. Its apex handoff is disabled by construction. Leave the existing jump `RMAction` defaults intact: that mode currently transitions to Falling at the apex or its one-second safety limit.

Before the first animated movement frame, register the calibrated `FrontLedge` warp target, establish traversal ownership/mode, bind completion/interruption handlers and play the FullBody montage through the existing GAS task at 1.0. Pair successful playback with finite `DriveRootMotion()` and store its generation for `ReleaseRootMotion()`. Use current Mover APIs and inspect same-frame queued cancellation behavior; montage playback by itself does not drive this APawn capsule.

GASP's verified target is `FrontLedgeLocation + FVector(0,0,0.5)` with rotation `MakeRotFromX(-FrontLedgeNormal)`. This is the **physical lip/contact anchor**, not the destination capsule center. `BackLedge` and `BackFloor` are unnecessary for this mantle. Account for the calibrated warp provider before adopting the source's 0.5 cm offset. Motion Warping matches root motion to targets over specified windows; use spatial correction while retaining 1x playback. [Epic Motion Warping documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine).

Maintain the project's single-player-first, future-co-op architecture: authoritative validation of geometry, replicated action identity/selection/targets/timing, and Mover-owned movement. Reuse the current GAS prediction/replication pattern and define rejection cleanup. Montage replication alone does not replicate warp targets. Simulated proxies play the pose and follow replicated movement; they must not queue a second root-motion drive. Avoid raw actor teleport/lerp timelines.

**6. Protect animation, procedural and equipment ownership**

Map `Traversing` explicitly to the existing traversal enum and handle it before jump/landing transitions. The FullBody montage controls the visible action; underlying locomotion must not select a jump or manufacture a jump landing on successful mantle completion.

This is mandatory: the current AnimInstance directly queues/cancels the broad `Mover_AnimRootMotion` tag at pending locomotion transitions, start/stop exits and raw RMAction exits. Clear pending locomotion root-motion work at traversal entry and prevent these callbacks from cancelling a mantle/new successor. Prefer ownership-aware cancellation where touched; using the generation API only in the ability does not protect against these existing broad cancellations.

Feet already fade out for FullBody and nongrounded movement. Add explicit traversal gating for entry/exit gaps if needed, release old pins and replant on supported completion. Keep neutral OffsetRootBone behavior. Existing paired-hand targets belong to the grab system; do not borrow them for a ledge. First calibrate authored hands plus warping; add an independent mantle contact provider only if user review shows correction is needed.

Integrate traversal with equipment reservations/subscriptions and relevant overlay gates so Ready/aim/fire/reload cannot fight the action, following section 4b's self-cancellation rules. Hands-free readiness is required before montage execution; use the temporary holster flow for an equipped weapon. Merely zeroing aim weights is insufficient. A valid armed mantle request is routed before ordinary jump's explore-only restriction; do not globally enable armed normal jumps as a side effect. Keep camera look available and verify existing grounded height smoothing does not create a completion pop.

**7. Make every exit complete and owner-aware**

Use one idempotent cleanup path for completion, interruption, failed playback, lost target, blocking obstruction, death, unpossess, end play and watchdog expiry. Release only this action's root-motion generation, warp targets, tags, delegates, input locks and collision changes. A stale callback must not terminate a newer action.

On successful handoff, recheck real support/clearance and enter Walking; if unsupported, enter Falling. If another system has already taken control (for example death/grab), do not overwrite its mode or state. Keep movement driven until the calibrated spatial handoff, then release cleanly without a final teleport. Callback/notify events drive normal completion; a duration-based watchdog is only a backstop.

**8. Build, read back assets, then let the user validate gameplay**

Follow project build/asset-authoring skills. Preserve unrelated edits already present in the workspace. Complete required C++ build and asset compile/save/readback checks; report actual results and pre-existing unrelated errors separately. For any copied Blueprint logic, verify live pin types/metadata and document intentional AZ adaptations rather than claiming full GASP parity.

Project rules: **do not add automated tests; ask before starting PIE or running editor tests.** Default to giving the user the following manual checks, then read their logs:

- Stationary Relaxed and Neutral mantle, including each enabled foot variant; approach from different starting distances and modest facing angles.
- Empty space, unsupported height, narrow top, blocked ceiling, occupied destination and invalid target preserve normal Jump routing.
- Capsule follows the animation; hands meet the lip; feet release/replant; no mid-mantle gravity drop, jump montage, double root motion or completion teleport.
- Tapping/holding/releasing/repressing Jump; cancel/death/target removal; movement and input recover without stuck tags or collision exemptions.
- Ordinary jump, locomotion starts/stops, crouch, camera and existing equipment still behave correctly.
- Same-frame Jump/target contact and stale reaction data cannot insert Brace/Stumble/HeadHit into an accepted mantle. An already-playing reaction finishes if Jump is pressed late; a fresh press afterward can mantle. After completion, a new run into an unrelated wall still reacts normally.
- Stationary Jump detects the ledge without held movement input. Stairs, ordinary steps and ramp-to-platform transitions retain their existing suppression; a 20 cm-thick wall without standing support is not misclassified as a mantle landing.
- Equipped rifle/pistol holster once, remain carried throughout mantle and draw once afterward. Preparation cancellation/target loss, death and inventory removal leave coherent equipment; stale input never resumes firing and no reservation cancels its own holster.
- If multiplayer review is requested: listen-server owner/client/proxy agree on action/variant/targets; no duplicate capsule driving or permanent lock after rejection.

Deliver code/assets, exact mapping and warp calibration, exposed settings, build/readback results and a short remaining-user-check list. Do not claim visual success before the user checks it.

**Later deliveries**

After the standing slice is accepted, add `_walk_` variants for both styles, raw approach context, calibrated entry windows and foot/pose selection. Then add `_run_` variants and appropriate speed/distance/top-clearance bands. Extend the same detector/Chooser/ability/Mover path rather than creating separate mantle systems. Preserve 1x playback throughout.

**Current source locations to start from**

- Jump lifecycle: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PawnJump.cpp:37](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PawnJump.cpp:37).
- Hero input/motion-warping/grants: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:167](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:167).
- Movement registration, jump and root-motion ownership: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverComponent.cpp:40](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverComponent.cpp:40).
- Swept movement/apex behavior: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMovementMode_RMAction.cpp:79](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMovementMode_RMAction.cpp:79).
- AnimInstance root-motion conflicts/mode mapping: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:411](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:411).
- Traversal state integration: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:138](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:138).
- Procedural feet gates: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance_Procedural.cpp:138](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance_Procedural.cpp:138).
- Equipment gate/cancellation: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp:31](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp:31).
- Existing reaction probes and step exclusion: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_ObstacleSensorComponent.cpp:53](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_ObstacleSensorComponent.cpp:53).
- Walking capability sweep/clamp: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_MovementDirectionCapabilityComponent.cpp:31](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_MovementDirectionCapabilityComponent.cpp:31).
- Hero's reaction/overhead input lock: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:959](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:959).
- Persistent reaction latch: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:894](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:894).
- Clip-held reaction: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:204](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:204).
- Existing GAS montage usage: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_MeleeAttack.cpp:422](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_MeleeAttack.cpp:422).
- Static warp-point coordinate-space implementation: [C:/UnrealEngine/Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/RootMotionModifier_SkewWarp.cpp:315](C:/UnrealEngine/Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/RootMotionModifier_SkewWarp.cpp:315).

The editor connection dropped during a read-only GASP chooser-tree dump and subsequently recovered after an editor restart. The plan does not claim verified chooser row thresholds or MH pose-search compatibility. Subsequent targeted montage/skeleton reads succeeded; no PIE or automated tests were started during this planning work.
