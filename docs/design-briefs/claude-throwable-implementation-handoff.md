# Claude — continue and complete CHALK throwables

**Primary implementation entry point, September18,2026.** Artur requested one complete handoff that incorporates the latest review and gives a clear execution order. Continue the existing feature; this is an implementation assignment, not another request to propose the same plan.

## 1. Read and establish the baseline

Read these completely before editing; follow their relevant linked references when working on that subsystem:

1. `C:/UnrealEngine/Games/AZ/AGENTS.md` and the applicable project skills.
2. **This handoff** — latest verified state, scope, precedence and completion gates.
3. `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-open-questions-review-response.md` — numbered decisions and corrections for stance, Mover, exclusivity, root motion, calibration, scale, pickups and ballistics.
4. `C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-slot-splice-decision.md` — exact graph architecture; the splice is now completed, so use it to verify and preserve the graph.
5. `C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-completion-work-order.md` — remaining presentation, ownership and item implementation requirements.
6. `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-system-implementation-plan.md` — full stone/grenade/knife scope and acceptance cases.
7. `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/README.md` and its baseline-kit link — supplied art assets and runtime parameter contract.

Precedence: explicit latest user decisions → this handoff's September18 state and execution order → the numbered review → remaining current contracts → historical audits. Source/live inspection establishes what exists; it does not silently change a gameplay decision. Do not treat old “missing” findings as current when they have been fixed. A genuine new contradiction should be named precisely; continue independent work rather than asking again about settled decisions.

After reading, give a short checkpoint: current behavior, the next incomplete milestone and any actual content blocker. Maintain a requirement ledger with **implemented / built-loaded / asset-saved / user-verified / blocked** evidence. Acknowledging that a file was read is not completion evidence.

## 2. What was actually verified today

The active hero is the APawn/Mover `AAZ_PawnMoverHeroCharacter`; the actual mesh is `SKM_MHC_Hero_BodyMesh`. The animation class is `UAZ_MoverAnimInstance`. Do not use ACharacter/CharacterMovement assumptions or edit the legacy `UAZ_AnimInstance` to fix this hero.

| Area | September18 evidence | Required action |
|---|---|---|
| Throwable graph | Active MHC AnimBP has52 nodes, compiled UpToDate and saved. The previous49 nodes remain, with one replaced edge and three new cache nodes. | Preserve it; do not recreate the splice. |
| New branch | Final ordinary aim blend → PreThrowable cache → Throwable slot/upper-body mask → FullBody. Mask spine_01/depth1; same cache feeds base and slot source. | Verify these links remain and use the branch. |
| Actual montage tracks | Carry, Start, Loop still use RifleFire. Close, Far, Cancel still use FullBody. | Route carry and separate crouched variants correctly; standing exclusive aim/release uses FullBody. |
| Stance/movement policy | No captured throw stance, generic Mover action lock or Run-before-Sprint throw cancellation in current source. | Implement; a connected graph does not supply these policies. |
| Exclusivity | Existing refusal/self/Sprint tags do not implement the whole agreed policy. | Add correct voluntary-action arbitration and forced-interruption cleanup. |
| HUD | Throwable data/view exists; complete phase/hint/contact population and display integration is not established. | Inspect live widget wiring, implement missing producers/consumers and verify. |
| Grenade/knife | Grenade remains a thrown recoverable object; fuse/explosion and equipped-knife behavior are incomplete. | Finish later milestones; do not describe the prototype as complete. |
| Art calibration | Current saved ability CDO reports PulseWidthPixels=1.0; last explicit requested tuning was7.5, with Brightness1.5. Who/why changed width is unverified. | Flag the mismatch. Preserve the observed value pending Artur/Codex art direction; do not silently reset it or claim7.5 is active. Continue independent gameplay work. |

The newest uncommitted change in `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_AnimInstance.cpp` is indentation only. Do not count it as a gameplay fix. Earlier documentation edits largely record review decisions rather than completed implementation.

Verified main graph links, using abbreviated node GUIDs for identification only:

```text
A82E98F4 final aim result → 22307F6B SaveCachedPose PreThrowable
43FA8BD8 UseCachedPose PreThrowable → A30C1E6B Throwable.Source
A9F81D03 UseCachedPose PreThrowable → 62E42C8E mask.BasePose
A30C1E6B Throwable.Pose → 62E42C8E mask.BlendPoses_0
62E42C8E mask.Pose → 5BF55C28 FullBody.Source
```

Resolve full identifiers against the live graph before any mutation. DeadBlending, OffsetRootBone, procedural feet, paired hands and PoseHistory remain downstream. Node counts alone cannot validate links, bindings, montage routing or actual pose behavior.

## 3. Settled behavior

- **Hold RMB to aim, release RMB to request one throw, LMB to cancel before physical release.** No charging, cooking, held retries or automatic repeat. Early release during Start can latch once for the same action until the verified ready seam; cancellation clears it. A cancelled held RMB cannot rearm or throw on its later release.
- Selecting/equipping a grenade immediately shows a suitable carry idle and correctly attached prop, without RMB, reservation or consumption. It is **Equippable**. Carry permits walking, running, crouching and normal stance changes.
- **Standing aim/release: exclusive FullBody. Crouched action: upper-body mix over the crouched base.** Capture the settled actual Mover stance and its presentation profile at activation. New stance toggles are ignored without buffering until the action ends. Do not cancel the existing crouch owner. A live crouched base is not an implicit permission to translate during exclusive aim.
- Run/Sprint input cancels preparation before Sprint eligibility, releasing gameplay ownership immediately with an appropriate short visual blend. It must not wait through an entire Cancel montage. Before physical release cancellation spends nothing; after release it cannot refund or recall the world item.
- Death, grab, stagger and other mandatory system actions preempt the throw. Exclusivity is for normal voluntary actions, not immunity from those events.
- Animation playback stays **1×**. A montage slot label in the profile is descriptive; the actual montage tracks route the pose. Do not mutate shared montage assets dynamically to change stance.
- Preview depicts **first contact**. Keep shared flight parameters, true collision responses, owner-only presentation and depth occlusion. No invented ground marker or grenade blast radius at first bounce.
- Keep the approved bounded aim-distance→speed mapping and explicit origin-height lift control. Lift is a declared spawn displacement; calibrate the unlifted grip separately and sweep the grip-to-offset path so it cannot skip an obstacle.

## 4. Execute in this order

### A. Finish the usable carry/aim/cancel flow

1. Inspect the actual current profile, carry idle and montage tracks. Carry is driven by CarryMontage; assigning the legacy CarryPose/WeaponRelaxedPose variable does not establish a consumer in this MHC graph. Route suitable in-place carry into Throwable and verify source passthrough with no montage active.
2. Implement standing versus crouched presentation selection with captured stance and separate project-owned variants. Standing Start/held Loop/release uses FullBody. Crouched variants use the existing mask and require visual validation; do not apply a guessed pelvis counter-rotation or silently stand the player up. Measure the composed crouched release pose and anchor independently. Preserve authored source assets.
3. Add the action locomotion lock through the game-thread input producer and serialized Mover custom input. Zero voluntary intent and suppress residual base planar velocity as required for a planted action; keep moving-base support, gravity and explicitly owned action movement. Never put mutable latches in predictor-shared movement-mode instances. An RM checkbox or moving feet does not prove capsule root motion; use the existing owned Mover bridge only for measured desired root deltas.
4. Implement voluntary-action exclusion, existing-action cancellation and active-input-task gating. In particular, an already-active crouch ability must not receive a toggle while stance is locked. Run cancellation must precede Sprint eligibility on both owner and authority. Slot groups alone do not manage the ASC's montage owner.
5. Centralize idempotent action/avatar/source cleanup. Remove only this action's tags, timers, queued release, exact montage and movement/presentation claims. Preserve any higher-priority owner. Restore carry only if its selection still exists and is appropriate after the interrupt.

**Milestone A is complete only when** carry moves correctly, both stance paths are correctly selected, exclusive aim actually holds movement/stance, Run cancels promptly, and release-after-cancel cannot launch. Do not stop at graph wiring, a tag declaration or a compiling profile.

### B. Complete ownership and presentation

- Preserve the proven inventory4→0 path and identity transfer. Strengthen exact action/montage release-cue validation, immutable commit receipts, reentrant last-unit handling, source/ready/equipment/avatar revisions, authority revalidation after spawn callbacks, and no-refund behavior after physical release. A missing cue aborts; a watchdog never throws.
- Populate the complete throwable UI state and bind initial snapshot plus updates to the actual HUD. Show selected item icon/name/count before RMB, then truthful phase/blocked/range/control hints. Reuse the existing grenade texture through the item view; keep the underlying Explore/Fight/weapon ownership and health baseline.
- Preserve the renderer's coherent class-default style refresh and surface-aligned marker. The old center tilt buried the glyph; do not reintroduce it. Wire blocked/body-contact states using supplied art. Correct collision-endpoint smoothing only where evidence shows it remains incomplete.
- Calibrate preview versus finalized live release in the same body, stance, montage/time and coordinate frame. Log the unlifted base separately from approved lift. Do not average incomparable old measurements or treat a repeated35cm error as harmless because preview is an estimate.
- Classify the floating-object symptom by owner/type/GUID before changing it. A recovered non-physics pickup spawned above support is a plausible cause, not proven duplicate held art. Preserve safe spawn and one-owner identity transfer.

### C. Finish the originally requested item scope

- **Stone:** definition/content, impact hearing with thrower attribution, bounce/settlement and one recoverable unit.
- **Grenade:** server fuse beginning at physical release, continuing through rest/ability end, one detonation, cover-occluded/deduplicated GAS damage, explicit self/friendly policy, intentional payload consumption and replicated presentation. An enum value or recoverable grenade-shaped pickup is not an explosive grenade.
- **Knife:** equipped-item source resolution as well as readiness, correct hand/weapon ownership, impact/embed behavior and recovery preserving the exact unit identity without duplicate pickup.
- **Weapon contexts and observers:** one consistent profile resolver for ability/held prop/preview, suitable MH rifle/pistol families, scoped Equipment/IK ownership, and visible remote held/released items. Keep advanced client prediction separate from basic authoritative correctness.

Complete the authorized scope in coherent milestones. If real missing content or a new user choice blocks one portion, identify the exact blocker and complete independent portions; do not present the whole system as finished. Art creation/revision belongs to Codex: integrate supplied assets and report precise art needs instead of replacing them with placeholders.

## 5. Change and verification discipline

- Preserve existing dirty work. Snapshot target graph links/properties and back up only assets being changed. Do not revert unrelated source, maps, sockets, skeleton settings or styling.
- The seven L_001 pickup manifests previously inspected were already Equippable; re-check before migrating. Preserve identity/count/state/bindings for any genuinely stale copies. Do not globally rescale the hero or collision from the unsupported1.65× inference.
- Use the appropriate project authoring utilities and the safe native AnimBP compile/save workflow. Never compile/reconstruct/save an AnimBP inside the Python mutation stack. Read back saved graph edges, montage tracks, slot groups and live property values.
- Follow the project build skill. A success from an earlier build is not validation of your new change. New reflected fields/functions need a normal build/restart; a Live Coding patch is session-local. Report what is actually loaded.
- **No new automated tests. Ask before starting PIE or editor gameplay tests; normally Artur tests and you inspect logs.** Prepare the code, assets and successful build first, then give one concise relevant manual checklist. Do not invent test results or repeatedly ask for settled design decisions.
- Verify the core matrix: equipped carry idle/walk/run/crouch; standing and crouched activation; stance toggles; Run/LMB during Start/Loop/Windup; cancel followed by RMB release and a fresh press; menu/source/avatar/death/grab interruption; remaining and last unit; hand/offset obstruction; ground/wall/body/open-sky preview; grenade resting/airborne/covered detonation; unique knife recovery; observer visibility. Respect the chosen pre-/post-release boundary in every case.

Finish with a concise report: changed files/assets, exact behaviors delivered, build/load/save evidence, what the user actually exercised, remaining limitations, and next steps. Keep technical receipts in the project. Do not substitute another general plan or a “shall I continue?” for implementation that is already authorized.
