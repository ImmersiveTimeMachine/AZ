# Review of Claude's throwable Phase 0 report

September 16, 2026. Reviewed `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-phase0-status.md` against the agreed plan, current input source and local engine prediction/collision code. No gameplay code, assets or config changed; no PIE, build or tests were run. Claude's new animation measurements were read from the report, not independently re-sampled in this review.

**Assessment:** useful Phase 0 evidence; proceed with the implementation foundation after correcting the preview-selection and collision-parity wording below. Release times remain provisional content calibration. Quiet Sage03 remains selected. **Later user-confirmed control override: hold RMB to aim, release RMB to throw, click LMB to cancel.** Earlier click-to-aim language is superseded.

## Answers to Claude's two questions

1. **Retarget the unarmed right-hand family before delivering the Phase 1 stone slice.** That is already the plan's intended first playable presentation and matches the unarmed stone mockup. The exact-MH pistol family can support temporary internal pipeline work, but should not become the shipped unarmed stance merely because it is available. Inventory, input and projectile foundation can proceed independently while the retarget is prepared. Reuse the existing retargeter as a template, verifying each source rig's mesh, retarget pose, chain mapping and target body; remeasure the resulting unarmed grip/release rather than copying pistol cue times.
2. **Leave MiddleMouseButton / Lethal unchanged.** The final user mapping is RMB hold/release with LMB cancellation. Existing MMB quick-select assignment and other mappings remain outside this change. The presence of an existing action is not a reason to change the agreed controls.

## Required correction 1 — select for preview, lock at commit

Phase 0 §4 says the chosen Close/Far anchor starts feeding the preview at commit. This would leave the pre-commit preview calibrated to the wrong throw whenever that choice changes. The reported92.3cm release-height difference makes that especially visible.

Correct flow:

- During Preparing/Aiming, choose a **provisional** Close/Far candidate from the live aim intent/profile with stable selection/hysteresis.
- Compute the displayed trajectory using that candidate's calibrated release transform, velocity and collision parameters, not the Loop hand or a universal origin.
- When RMB release actually begins Windup, freeze the currently displayed candidate and accepted aim intent. An early RMB release during Start remains one pending intent; the preview can continue updating until the ready seam where it commits, unless LMB cancels it first.
- Revalidate source/physical release at the actual cue. Do not silently swap to another animation/origin after commitment to salvage a blocked solution.

The original plan's phrase “choose once at commit” meant **lock once**, not defer the entire choice until the throw request. Both the plan and execution brief are clarified accordingly.

Store a complete grip/release transform and its coordinate space. The reported component-space hand XYZ values are not automatically actor-space projectile origins. Account for Mesh-to-World, the hand transform, item grip offset/orientation, active pose layering and any permitted movement.

## Required correction 2 — channel2 alone does not prove collision parity

The additive `ThrowableProjectile` profile is a sensible direction; `QueryOnly` is compatible with kinematic swept projectile movement. Keep the old macro/profile usages untouched unless a separately scoped change is justified.

However, a channel trace checks the encountered component's response to the trace channel. The moving sphere also has its **own response container** and movement ignore rules. Unreal collision outcomes depend on both objects' responses. [Epic collision response reference](https://dev.epicgames.com/documentation/unreal-engine/collision-response-reference-in-unreal-engine?lang=en-US)

Concrete counterexample: the proposed throwable ignores Ability objects. An Ability object that blocks Projectile/Ch2 can stop a bare Ch2 prediction sweep, while the real throwable ignores it. Thus “object type Projectile → trace Ch2 → exactly the same blockers” is not a general guarantee. This is a specification gap, not a claim that such an object was observed causing a failure in this session.

Local engine evidence:

- `C:/UnrealEngine/Engine/Source/Runtime/Engine/Private/GameplayStatics.cpp`, the `PredictProjectilePath` channel sweep around2887, passes query params but no projectile-specific response container.
- `C:/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/PrimitiveComponent.cpp:3201`, `InitSweepCollisionParams`, copies the moving body's response container, ignored actors/components, trace complexity and ignore mask.

Use a collision matrix covering scenery, movable geometry, Pawns/friendlies, PhysicsBody, Vehicle, destructibles, triggers, Ability, Pickup, own/other projectiles and attached equipment. Establish a shared response-aware sweep contract for prediction and launch validation. One practical approach is to obtain the ballistic samples without native collision tracing, then sphere-sweep each segment using the intended projectile object channel **and** its response container/ignore policy, stopping at the first blocking hit. Equivalent implementations are acceptable. Merely switching to an object-type query also does not automatically reproduce bilateral responses.

Preserve the first-contact point/normal and contact-time calculation, and do not introduce a second independent ballistics tuning model. Explicitly decide Vehicle handling rather than leaving it outside the listed profile rules. Runtime/preview differences from dynamic movement and substeps still need the planned user validation.

## Release candidates and recovery

Keep **Close0.280s / Far0.385s as reported kinematic candidates for these MH pistol clips**, not proven physical release instants or defaults for other families. A hand-height extremum and peak horizontal speed can identify a useful candidate but do not prove the exact moment of object release or that the arm stopped accelerating. Review with the correct held prop, release direction, finalized pose and first launched frame; persist the sampling method/step alongside the numbers.

The reported constant thumb-to-middle distance establishes that this particular metric offers no opening cue. By itself it does not prove every finger transform is static. If that broader claim matters, verify the finger tracks/local transforms; it does not need to block the generic systems work.

Playing the pistol release to its natural end with an appropriate blend-out is a reasonable starting choice. This **does not contradict** the original conditional instruction to use an early hand-back only when measured content supports it. Hand motion need not reach zero before a legal blend/control handoff, and Mover walking/look should remain responsive under the upper-body action. Do not add a separate “wait until hand is still” gate, append another recovery, or copy a universal0.1–0.3s tail cut to all clips.

## Short follow-up for Claude

Continue with Quiet Sage03 and the final RMB hold-to-aim/release-to-throw, LMB-cancel controls. Retarget the unarmed family for the first delivered stone slice; leave MMB alone. Keep Start→Loop with the measured Start-end seam and one early-RMB-release intent, cleared on cancellation. Select a stable preview candidate while aiming and lock it at commit. Keep the new additive projectile profile, but implement/verify both sides of collision filtering rather than assuming a bare Ch2 trace matches runtime. Treat pistol release times as candidates pending prop/launch visual confirmation; measure the new unarmed clips separately. Natural pistol recovery with sensible blending is acceptable. Preserve the inventory transaction, exact-once release,1× playback and no-agent-PIE/test rules already in the execution brief.
