# CHALK throwable completion audit

**September 16, 2026 Toronto / September 17 UTC.** Review of current source, loaded assets, existing user-run logs, the user's screenshot and Quiet Sage03. No gameplay code/assets changed, no build performed, and no PIE or tests started by this review.

**Conclusion:** the grenade prototype can prepare, launch, spend inventory and recover as a pickup. Its preview renders, but it does not reproduce the approved design. Explosive grenade behavior, complete action ownership, armed variants, knife support and the throwable HUD remain unfinished.

**Later September17 art-production update:** user explicitly assigned all art production to Codex. The [Quiet Sage production kit](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/README.md>) now supplies11 saved Unreal assets, native GIMP masters and a reviewed board. Both materials compiled; mesh UVs/topology and saved references verified. Live BP had Cube arc and MarkerMesh=None, and its old arc material parameter names mismatched the newer renderer. Codex assigned the new ribbon/arc and plane/contact material plus128×64 padded marker canvas to BP_AZ_GA_Throw. Only six art fields changed; no Source, montage, gameplay-logic edits or PIE/tests. This supersedes the earlier missing-art/assignment findings below, while projection/HUD/blocked-body semantics and in-game acceptance remain with Claude/user. Receipts: `C:/UnrealEngine/Games/AZ/Saved/ThrowArtProduction/`.

**Sunlight/close-marker follow-up:** user confirmed poor contrast on bright surfaces. Codex created six Sunlight material siblings with a charcoal keyline and stronger filament, compiled/saved them, and assigned ArcMaterial/MarkerMaterial plus5.5px envelope/.55 filament alpha/1 pulse alpha. Palette, trajectory and newer55-degree marker tilt preserved. Details and comparison: [daylight art revision](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/README.md>). Receipts in `C:/UnrealEngine/Games/AZ/Saved/ThrowArtProduction/Sunlight/`; final gameplay appearance pending user capture. No Source/montage edits or agent-started PIE/tests.

Next assignment: [Claude completion work order](C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-completion-work-order.md). The [original plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-system-implementation-plan.md) remains the behavior specification; its initial inventory of missing systems is historical.

## September17 follow-up: equipped idle, category and moving legs

The user explicitly requires immediate held idle on equip, grenade in **Equippables**, and Explore upper/lower-body blending for walk/run/crouch. They report frozen lower-body animation. The prior handoff mentioned carry and movement but left movement policy too open and did not specify the required inventory category. The work order now makes all three first-delivery requirements in section0.

Read-only recheck against editor PID30772 confirmed the pickup manifest still uses `ItemCategory=Consumable` and `Item.Type.Consumable.Throwable`. CarryPose points to `AZ_RTG_MH_ThrowLoop`, while actual Start and Loop montage slots are **FullBody**. The native carry path feeds `WeaponRelaxedPose`; this establishes partial wiring, not successful final-graph blending. A FullBody aiming loop can override working legs; equipped-only versus held-aim behavior still needs separate diagnosis. Receipt: `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/equipped-idle-category-followup.json`. No gameplay changes or playback were performed.

Recategorization also needs selection-path work: `IsReadyable`/`CanBindItem` recognize throwable capability, but direct `SelectInternal` still gates the readiness route on IsConsumable; inventory popup Equip requires EquipmentFragment while Consume uses IsConsumable. Update capability-based selection and canonical manifests together. Preserve stack identity/count and manual quick-slot bindings. Equippable category alone must not route a grenade into an unsupported firearm equipment path.

User's later [Explore screenshot](C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/user-explore-grenade.png) shows the grenade held while the HUD still displays EXPLORE. Its `moving 0` debug state does not verify moving-leg behavior. The handoff now explicitly requires the selected throwable row even before RMB, with Explore retained as the underlying mode. User supplied `/Game/FPS_Controller/UI/Textures/T_FragGrenadeIcon.T_FragGrenadeIcon` for the HUD; the inspected ImageFragment already uses it. The remaining task is consuming that view in the HUD, not creating grenade icon art.

## Evidence and latest changes

The review combined three independent source/asset audits under `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/`: `action-inventory.md`, `presentation.md`, and `flight-impact.md`. Their earlier observations must be read with this later live recheck:

- Editor PID40684, normal DLL timestamp **03:40:18 UTC**. Native `UnrealEditorSubsystem.get_game_world()` found the user's existing `UEDPIE_0_L_001`, although Rider's status had reported Idle. No playback was started or controlled.
- The current profile again contains GripBone/GripOffset, and the loaded grenade now exposes origin lifts. The transient header/build mismatch in the earlier presentation note is **not an outstanding compile diagnosis**.
- `DA_ThrowProfile_Unarmed` now has **SeparateArcMontages=true**, Close/Far cues0.490/0.700, and a CarryPose referencing `AZ_RTG_MH_ThrowLoop`. Earlier one-clip=false observations are superseded. The C++ default remains false: inspect saved data, not comments/defaults alone. Do not automatically reverse the user's intervening decisions.
- A throwable carry pose now feeds the existing relaxed upper-body lane. This is progress for carrying while moving; it does not resolve the FullBody preparation/release movement policy or armed equipment ownership.
- Live grenade **CloseOriginLift=50cm, FarOriginLift=0**. The solver adds that world-up displacement to both preview and physical launch origins. See `latest-definition-recheck.json`.

## Why the preview does not match the picture

User screenshot: [current prototype](C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/user-preview.png). Approved reference: [Quiet Sage03](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/CHALK_Throw_03.png>).

The screenshot proves visible white rectangular segments and four white bars. It supersedes an assumption that no arc renders at all. It does not establish why an earlier attempt was invisible.

The supplied material is the assigned asset: `C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Throwables/M_AZ_ThrowPreview.uasset`, Unreal object `/Game/AZ/Blueprints/Throwables/M_AZ_ThrowPreview.M_AZ_ThrowPreview`.

| Element | Current implementation / evidence | Required Quiet Sage result |
|---|---|---|
| Curve | Pooled spline meshes using Engine Cube, 5cm cross-section | Fine continuous filament with short sage pulses; no visible boxes, caps or tube facets |
| Material | Translucent, Unlit, TwoSided, spline usage enabled; Color×Intensity to emissive, constant Opacity0.85 | Separate filament/pulse coverage, smooth edges and restrained brightness; depth aware |
| Color | Arc MID correctly receives linear conversion of **#B5C8B7**; all outer bars receive warm white | Sage arc **and outer corners**; warm-white inner ellipse, center and text |
| Contact shape | Four single straight Cube bars | Four actual L corners, incomplete inner ellipse, small center oval |
| Range | No rendered range label | Small real first-contact distance beside marker |
| HUD | PlayerUI produces name/icon/count, but inspected HUD native/event graph has no throwable consumer | Item icon/name/count and phase-appropriate binding-derived hints above existing health baseline |
| Motion | ShowSolution updates on the solve timer; component tick disabled | Smooth presentation between bounded solves, with immediate obstruction/endpoint correctness |

The latest pooled-component inspection found an identity preview transform and assigned MIDs, with correct sage Color, Intensity1 and Opacity0.85. All26 children were hidden by the time of inspection; this separate snapshot did not establish the active action phase and does not prove hidden components during aiming. Receipt: `preview-instance-latest.json`. Neither missing material, absent spline usage nor a demonstrated owner-visibility failure explains the supplied visible screenshot.

The white appearance is observable, but its exact exposure/translucency contribution has not been isolated. Cube geometry and missing material pattern are confirmed. Recoloring alone cannot produce the approved silhouette. Calibrate the final render under the actual camera/exposure instead of increasing emissive or disabling depth tests.

Two renderer correctness gaps accompany the art work: equal-count paths interpolate toward a new collision while the marker uses the raw new hit, potentially disconnecting the end or retaining an old path through cover; taking the first N samples under the component cap can discard the actual endpoint. Material reconfiguration also retains existing MIDs/pool assignments when the source material changes.

## What is already working

- Real APawn/Mover host, generic GA, readied throwable capability, held prop, authored Start/Loop/Close/Far/Cancel montages on the exact MH skeleton, and1× playback.
- Canonical RMB start/release routing and early-release latch exist. One cancellation with committed=0 was observed; complete cancellation/rearming behavior is not yet proven.
- Reservation and release commit exist. The inspected03:26UTC user session proves **4→3→2→1→0** consumption and four corresponding launch/recovery payloads. Stack splits receive new GUIDs; the last unit retains its original GUID. Preserve this working path.
- Shared solver now accepts APawn/body mesh, composes full transforms, uses the projectile profile's response container, and calculates fractional time to first contact. These specific foundation-review defects have been repaired.
- Pooled owner-only preview, launch clearance checks, sphere ProjectileMovement, impact noise and inventory-payload pickup conversion exist.

These receipts establish the ordinary local path, not all cancellation, reentrancy, failure or remote-client cases.

## Remaining gameplay and integration work

| Priority | Finding | Consequence / completion requirement |
|---|---|---|
| First visual pass | Prototype geometry/material/marker/HUD | Implement the selected artwork as a complete presentation, not just a colored line |
| Release alignment | Current body/socket sampling differs from saved Close/Far anchors by **14.89/29.11cm**; live Close additionally lifts the actual spawn50cm | Recalibrate using the rendered `SKM_MHC_Hero_BodyMesh`, socket and full transform; validate finalized pose to first launched frame. Shared erroneous offsets still produce a physically wrong throw |
| Action/source ownership | No complete ready/equipment/avatar/menu invalidation and authoritative revalidation | A reserved item can outlive the context presenting it. Cancel/disarm before release; capture and verify source revisions |
| Cancel/commit identity | LMB RequestCancel does not immediately send authoritative disarm; release cue validates phase only | Remote cancel/release race and wrong/late event risks. Bind cue to exact action/montage instance; clear held-edge state independently of incidental input tags |
| Transaction lifetime | Repeated Begin resets committed state; replayed Commit lacks a durable payload receipt; GA Released phase follows callback-producing commit | Complete replay/reentrancy semantics, including last-unit callbacks after source-change cancellation is added; do not undo a committed world payload |
| Grenade | Live BounceAndSettle, recoverable=true; no fuse/detonation/damage implementation | Currently a recoverable grenade-shaped object. Implement server fuse at physical release, once-only occluded GAS explosion, intentional payload consumption and replicated presentation |
| Weapon contexts | GA and hand component use DefaultProfile; live ContextProfiles empty | Rifle/pistol families and Equipment hand ownership/IK/camera arbitration are not complete |
| Equipped idle / movement | Carry layering partly wired; actual Start/Loop are FullBody; user reports frozen legs | Mandatory Explore carry/held-aim upper/lower blend with live idle/walk/run/crouch; short pelvis-dependent release treated separately. No long-lived movement lock as a substitute |
| Inventory category / selection | Loaded grenade still Consumable; direct-slot and popup routes contain category/fragment gates | Canonical Equippable category plus working inventory Equip, bound-slot and QuickSelect; immediate carry idle, no consumption on selection |
| Stone/knife | Grenade is the configured slice; equipped knife source is excluded from Ready but GA reads ReadyItemId only | Deliver real stone behavior/assets and equipped unique-knife source, embed/recovery and exact GUID ownership |
| Network | Definition not replicated; no remote activation initializer for hidden projectile mesh; local held props | Complete observer-visible held/released objects and fuse/explosion state; advanced prediction can remain a later phase |
| Failure handling | Missing-live-grip fallback, finite/work caps, true zero-G policy, cancelled-montage failure path, recovery failure/expiry disposition | Fail closed before spending, never invent a watchdog throw, preserve/dispose payload deliberately |

The current throw sequence's screenshot cannot determine the correct animated hand pose at release. A preview based on the future release pose can legitimately start away from the current Loop hand. The measured anchor mismatch and50cm physical offset are separate concrete issues; do not fix them by moving the launch to an arbitrary visual endpoint.

## Visual acceptance and scope

Match Quiet Sage's shapes, palette, line weight, spacing and HUD hierarchy at the reference resolution. World projection and lighting vary; exact pixel equality in every camera view is not a useful requirement. The illustrated stone, count3 and9.4m are examples, not hardcoded gameplay data. Preserve final controls: **hold RMB aim, release RMB throw, click LMB cancel**; old mockup captions are not input authority.

Manual user review still needs dark street and bright prototype ground, ground/slope/wall/body contact, open sky, blocked hand, moving aim, cancellation then release/new press, last unit, armed variants, grenade at rest/airborne/behind cover, and observer visibility. No automated tests were added and no editor tests were run for this audit.
