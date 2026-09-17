# Claude — finish CHALK throwables, Quiet Sage first

Continue the existing implementation. **The first delivery must include equipped idle, Equippable inventory classification, working upper/lower-body blending in Explore, and the actual Quiet Sage03 arc, contact marker and throwable HUD.** Then complete the remaining behavior in the original plan. The user's screenshot proves the preview renders as white boxes/bars; the September17 follow-up also reports frozen lower-body animation while walking/running/crouching. Treat these as first-delivery defects.

Read the current [completion audit](C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-completion-audit.md), then relevant sections of the [original plan](C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-system-implementation-plan.md). Evidence is under `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/`; its `latest-*` receipts and consolidated audit supersede the earlier subreports where source/assets changed during review. Preserve working APawn/Mover, response-aware prediction, montages, inventory4→0 release/recovery and carry-pose work.

**Art is now supplied by Codex, per the user's explicit ownership instruction.** Read the [production kit and exact integration contract](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/README.md>).11 new Unreal assets are saved under `/Game/AZ/Blueprints/Throwables/Art/QuietSageV1/`. Codex already assigned SM_QS_Ribbon + MI_QS_Arc and SM_QS_MarkerPlane + MI_QS_Contact to BP_AZ_GA_Throw.PreviewStyle, with padded canvas128×64. Preserve those assignments. **Do not recreate placeholder art or send art production back to gameplay implementation**; request any needed art revisions from Codex. Your remaining work is runtime semantics, projection, HUD and the gameplay items below. Final in-game appearance still needs user review.

**Later sunlight correction is already applied:** use [the daylight revision contract](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v02_Sunlight/README.md>). Current materials are **MI_QS_ArcSunlight / MI_QS_ContactSunlight**, with matching optional blocked/body Sunlight variants. Same meshes/masks; six additional material assets. Current PulseWidthPixels=5.5 is the complete envelope, containing3.5px sage accents plus charcoal edge; FilamentAlpha=.55, PulseAlpha=1, StrokeFill=3.5/5.5. Keep these values together. Codex preserved the newer MarkerMaxTiltDegrees55 and every other existing style field. Earlier3px/.31 reference values and non-Sunlight material assignments are historical. The correction covers the close marker as well as the arc; it does not change gameplay or replace the depth-tested plane with a decal/particle system.

## Contract

**Throwable slot placement decision:** [current live-graph review and exact splice](C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-slot-splice-decision.md). Use final ordinary weapon aim result → new masked Throwable branch → existing FullBody; preserve the rig tail. This corrects the proposed RifleFireBase splice, which is upstream of the fire and aim layers. Review also records actual depth1 vs new depth4, montage tracks and skeleton group details. Do not ask the user again to choose the two older locations.

**Latest user art tuning:** PulseWidthPixels=7.5 (was5.5), active Arc/Contact Sunlight MI Brightness=1.5 (was1). Codex applied/saved these after the slot review. The mask/StrokeFill still defines a sage center plus dark edge; this is a deliberate larger line, not a throw-distance change. Preserve these values; the user can edit width in BP_AZ_GA_Throw Class Defaults → Preview Style → Pulse Width Pixels.

**Latest renderer fix already implemented by Codex:** read [live readability fix](C:/UnrealEngine/Games/AZ/docs/design-briefs/throw-preview-live-readability-fix.md). GetPreview now consumes coherent EditDefaultsOnly class-default style and refreshes cached art only on change. Marker stays on the true hit plane with at most4× in-plane height compensation; centre tilt was proven to bury its lower glyph and is removed. Preserve these changes. Live Coding built/loaded05:07:39UTC; no user post-fix capture yet. The old MI_AZ_ThrowPreview_Marker is unused; use the Sunlight instances. Do not reintroduce the tilt or assume CDO readback establishes instance calibration.

- **Hold RMB aim; release RMB throw; click LMB cancel before physical release.** Cancel disarms later release; fresh press required. No charging/cooking/Held retries. Keep animation1×.
- One shared launch solution; preview is owner-only and predicts **first contact**, not final rest or grenade blast radius. No second cosmetic trajectory with different ballistics.
- Inventory owns GUID/count; GAS owns action; Equipment owns weapon/hand presentation; Mover owns movement. Physical release, not mouse release, spends the unit exactly once.
- **September17 explicit user requirements:** equipping/selecting the grenade immediately activates its held idle; grenade belongs in **Equippables**, not Consumables; Explore retains animated lower-body idle/walk/run/crouch underneath the throwable upper-body presentation. These supersede the earlier open question about rooting the player while holding/aiming.
- Work within `C:/UnrealEngine/Games/AZ/AGENTS.md`: no new automated tests; ask before starting PIE/editor tests, normally let Artur test. Do not change assets or start a conflicting build in his active Play session. Inspect current files first because implementation has continued during this review.

## 0. Fix equipped idle, inventory classification and locomotion blending

These are required behavior, not optional polish. Current evidence: `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/equipped-idle-category-followup.json`. The loaded pickup manifest still says `ItemCategory=Consumable`, `Item.Type.Consumable.Throwable`; CarryPose is assigned, but the actual Start and Loop montages both use FullBody. CarryPose assignment alone does not establish that the final rendered legs are working.

### Equipped idle and action lifetime

Selecting/equipping a valid grenade must immediately blend into a suitable held-item idle with the prop correctly attached, **before any RMB press**. Keep that presentation until deselection/depletion or an action takes ownership. This carry state must not activate the throw GA, reserve/spend a unit, show the trajectory, or hold a FullBody montage indefinitely. Use an appropriate carry animation in the existing upper-body layer; if it is a loop, verify it actually advances/loops rather than merely assigning a sequence pointer.

The user's Explore screenshot is `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/user-explore-grenade.png`: grenade in hand while the equipment HUD still shows EXPLORE. It is a stationary frame (`moving 0`), not proof of moving-leg behavior. Explore remains the underlying mode; grenade selection must not require Fight. When a throwable is selected, the primary equipment row displays that throwable's icon/name/count, then restores the appropriate existing weapon or Fight/Explore row on deselection/depletion. Avoid simultaneous overlapping item and Explore rows.

Carry, Preparing/Aiming and Release/Recovery are distinct states. RMB starts the authored preparation, then aiming loop; cancel or completed recovery returns to equipped carry when the same stack remains. Depletion/deselection clears the pose and restores existing Explore/equipment presentation. A higher-priority action may temporarily own the body and must restore only the still-valid source afterward. Do not force Fight mode merely because the grenade is selected.

### Explore upper/lower-body blending

Reuse the project's existing relaxed/weapon upper-body blend arrangement. The **base pose must remain the live locomotion result**: idle, walk, run, starts/stops/turns, crouch idle and crouch movement, including standing↔crouch transitions. The throwable overlay supplies the intended torso/arms, preserving root, pelvis and legs from locomotion. Inspect the actual final AnimGraph routing, branch filters or blend mask, slot order, alpha and cached poses; updating `WeaponRelaxedPose` alone is not proof.

Apply this to both equipped carry and the held Preparing/Aiming state. An indefinitely held FullBody aiming Loop must not replace the lower-body result. Keep the locomotion state machine and phase progression active; do not solve frozen legs by disabling movement, forcing IdleLoop, or leaving an old full-body slot active downstream. Retain appropriate existing feet/terrain behavior for movable carry/aim and 1× playback. Sprint remains a separate existing policy; the user's walk/run requirement must not be accidentally routed through a sprint-only suppression gate.

The current unarmed source turns the pelvis substantially during release, so do not blindly mask the complete release clip above the waist. Separate long-lived carry/aim layering from the short committed release. Preserve the selected release family where it works; use suitable torso content/pose correction for movable preparation if needed. If a brief FullBody release is necessary, its ownership and hand-back must be bounded to the authored release/recovery, without freezing carry, held aiming or crouch movement. Any incompatibility with throwing from crouch needs explicit content handling, not silent standing-leg substitution.

Trace both possible causes of the user's frozen legs: the base locomotion pose failing to update, and a valid base being overwritten later. Check equipped-without-RMB first, then RMB-held; the FullBody aiming montages explain a possible override during aiming but do not prove why equipped-only motion is wrong.

Relevant current source: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowableHandComponent.cpp` (`SetCarryPose`, `Refresh`), `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp` (`ThrowableCarryPose` → `WeaponRelaxedPose`), and the active `/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC` graph. Verify clearing/suppression versus its comments and action phase, since carried and released props must not share an accidental lifetime.

### Equippable inventory placement and selection

Set the grenade's canonical inventory category to **EInv_ItemCategory::Equippable** and align its type tag/filter behavior with the project's throwable taxonomy. Preserve its ThrowableFragment, stack count/size, icon, definition and identity. A grenade being spent on throw does not make it a Consumables-tab item. Do not keep a duplicate there or merely move its widget visually.

Inspect pickup defaults, placed-instance overrides and existing manifest copies/placements. A default change may leave existing inventory items categorized incorrectly; preserve their GUID/count and revalidate grid placement if migration is needed. Do not delete/recreate the player's stack or autoassign quick slots.

Equippable inventory classification does not require pretending the grenade is a rifle/knife or attaching an unrelated weapon EquipmentFragment. Expose the expected Equip/select action for throwable capability and route it to the same authoritative selected-in-hand context as QuickSelect. Selecting must show equipped idle without consuming. Existing internal readiness can serve this context if it obeys these visible semantics and ownership rules.

Review **all** entry points after recategorization. `IsReadyable` and `CanBindItem` already recognize non-weapon throwable capability, but `UAZ_QuickBarComponent::SelectInternal` still branches on `IsConsumable()` only. The inventory popup currently exposes Consume by that category and Equip only for an EquipmentFragment. Update these paths coherently so direct slot selection, QuickSelect and inventory Equip all work for an Equippable throwable, with no instant Consume path and no firearm fallback.

Relevant files: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Inventory/AZ_QuickBarComponent.cpp`, `C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryGrid.cpp`, `C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Utils/AZ_Inv_InventoryStatics.cpp`, and `C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Items/Throwables/BP_Pickup_Grenade.uasset`.

**Acceptance:** grenade appears only under Equippables; inventory Equip and existing bound-slot/QuickSelect selection immediately show held idle without RMB or spending. In Explore, equip→idle→walk→run→stop and stand→crouch→crouch-walk→stand all retain moving legs. Repeat while holding RMB. Cancel/release recovery restores the still-equipped carry pose without an idle reset, stale montage or count error. User performs these gameplay checks after a successful build; no new automated tests.

## 1. Integrate the supplied art and finish runtime presentation

Supplied art: native [GIMP board](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/QS_Production_Art_Board_NATIVE.xcf>), [PNG review](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/QS_Production_Art_Board.png>) and the production-kit README above. Materials use exact current renderer parameters and CPD0/1. The old assigned M_AZ_ThrowPreview exposed DashDensity/DashDuty while runtime sent PulseSpacing/PulseDuty; it was not the correct art interface. The live BP also had MarkerMesh=None; main references are now repaired. The specifications below are integration/visual acceptance criteria, not a request for Claude to author replacement art.

Reference PNG: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/CHALK_Throw_03.png`.
Editable master: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/CHALK_Throw_03_NATIVE.xcf`.
Exact authoring geometry: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/sources/build_throw_mockups.py`.
Current screenshot: `C:/UnrealEngine/Games/AZ/Saved/ThrowCompletionAudit/user-preview.png`.

Current entry points:

- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowPreviewComponent.cpp` and corresponding public header.
- `C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Throwables/M_AZ_ThrowPreview.uasset`.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/UI/AZ_PlayerUIComponent.cpp` and `C:/UnrealEngine/Games/AZ/Source/AZ/Public/UI/AZ_PlayerUITypes.h`.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.cpp` and its existing HUD WidgetBlueprint.

Use supplied SM_QS_Ribbon and MI_QS_Arc: a flat subdivided strip with full0–1 UVs,64triangles and continuous-distance pulse shading. Runtime must provide CPD0=segment start cm and CPD1=segment length cm and face the strip correctly. Preserve the faint connecting filament and short sage accents; restarting UV/dashes at every ballistic sample must not expose segment boundaries. Keep pulse spacing stable when sample count changes. Static accents are supplied; any later motion should remain restrained.

Use these **1080p visual calibration targets**, not literal world-centimeter constants:

| Detail | Reference |
|---|---|
| Arc and outer corners | sRGB **#B5C8B7**, converted once |
| Text, inner contact and center | sRGB **#EEEAE0** |
| Connecting filament | ~1.2px,31% alpha |
| Sage pulse strokes | ~3px,94% alpha; roughly28 on the illustrated path, about one-third occupied length |
| Four L corners | ~2.1px stroke,104×30px projected outer footprint in the example; each has both legs |
| Inner incomplete ellipse | ~60×18px,1.3px stroke,70% alpha |
| Center oval | ~10×4.4px |
| Range | Roboto ~17px, offset beside contact |
| Item row | Existing Oswald/Roboto hierarchy: name~27px, count~22px, hints~14px |

Final world-space size, perspective scaling, anti-aliasing and exposure compensation should be tuned against a capture at the actual camera. Keep the arc thin and readable on dark pavement, bright grid and foliage. Use restrained contrast support when necessary; do not make it a glowing white cable. The current arc MID already has the correct sage tint: changing its color parameter alone is insufficient.

Use supplied MI_QS_Contact: actual sage L corners, warm-white inner broken ellipse and center. Its texture has a128×64 reference-pixel padded canvas containing the104×30 visible anchor span. The current surface-plane sizing still foreshortens on ground; fix projection in runtime rather than compressing the texture or blindly restoring104×30 as the entire mesh size. Use hit point plus normal, stable tangent orientation and a minimal offset; avoid flips, floating marks and z-fighting. Preserve depth occlusion and owner-only visibility. MI_QS_BodyContact and MI_QS_Blocked are supplied for compact body/unsafe-launch states; runtime must select them and supply localized OBSTRUCTED text. A near wall hit is valid when launch clearance is valid. No hit within horizon means fade/open the line end and hide marker/range.

Range means straight-line distance from accepted release origin to predicted first contact, in meters; calculate real data. The marker is never a blast footprint.

Bind the throwable HUD to initial snapshot **and** changes. Extend the view with the phase/blocked/contact state needed for truthful presentation. Show the actual item's icon/name/available count above the existing health baseline **as soon as it is equipped/selected, including Explore and before RMB**. The selected throwable row takes presentation priority over the ordinary Explore/Fight/weapon row without changing committed gameplay mode or weapon ownership; restore the existing row when selection ends. Preserve the health baseline. Ready hint: HOLD RMB AIM; while preparing/aiming: RELEASE RMB THROW and LMB CANCEL; committed recovery must not advertise cancellation of an already released projectile. Resolve glyphs from bindings. Legacy PNG captions are superseded.

**Use the user-provided existing grenade texture:** `/Script/Engine.Texture2D'/Game/FPS_Controller/UI/Textures/T_FragGrenadeIcon.T_FragGrenadeIcon'` (file `C:/UnrealEngine/Games/AZ/Content/FPS_Controller/UI/Textures/T_FragGrenadeIcon.uasset`). The inspected grenade manifest's ImageFragment already references this exact asset, so feed it through the item view into the HUD rather than generating replacement art or hardcoding a grenade brush in the generic widget. Preserve aspect ratio and transparency; size/tint it consistently with the established equipment row and Quiet Sage/warm-white palette. Verify actual displayed icon as part of the user screenshot review.

Keep solve work bounded. Smooth presentation per rendered frame while aiming, without continuing an old path through a newly detected blocker or separating marker from endpoint. Preserve the full first-hit endpoint when resampling to a pool cap. Reuse components/materials; reconfiguration must actually update existing pooled resources. Cleanly hide on every exit. The preview must never affect traces, physics, shadows or other players' views.

**First delivery acceptance:** satisfy section0's equipped/categorization/movement checks, then obtain an actual user-run screenshot showing the fine sage filament/pulses, four L corners, incomplete inner ellipse, center, real range and correct item HUD. Compare it side by side with03. Assigned assets and successful compilation alone are not visual acceptance.

## 2. Align preview, hand and physical release

Recalibrate the **full transform** at each active release cue using the actual rendered `SKM_MHC_Hero_BodyMesh`, current socket and item offset. Saved anchors differed by Close14.89cm/Far29.11cm from this body/socket in the audit. Confirm against the finalized live pose and first launched frame; do not copy skeleton-default coordinates or universal release times.

Latest saved profile has SeparateArcMontages=true despite a false native default; preserve the latest verified user decision and keep candidate montage/cue/anchor consistent. Choose provisionally with hysteresis while aiming, freeze at Windup, and capture fresh accepted intent at that edge rather than the previous20Hz update.

The live grenade adds **50cm world-up for Close**, including the physical projectile. This is a spawn displacement, not arc curvature. Correct the actual grip/clearance/velocity solution; do not hide disagreement with arbitrary world-up lifts. Physical release must originate at the validated grip/item launch transform. A legitimate item-local offset belongs to its measured grip calibration and must be included in held art and clearance. Fail closed if a required live grip is missing. Do not move the real projectile to the preview target.

Keep and complete the new carry-pose work under section0. The user's September17 instruction settles the long-lived movement policy: **Explore carry and held preparation/aim use upper/lower-body blending with live locomotion, including crouch**. The pelvis-turning source release still needs appropriate short-action treatment; it does not justify rooting equipped carry or the held aiming loop. Do not ask the user to reconfirm this decision.

## 3. Complete action and item ownership

Use the current action/inventory audit for exact functions. Finish captured avatar, source item, readiness/equipment generations and inventory revisions; pre-release menu/source/weapon change, grab/death/stagger and incompatible locomotion must cancel/disarm. Server revalidates current context and physical clearance, including after spawn/construction callbacks.

Make LMB disarm reach authority immediately; do not wait for a local Cancel montage to end. Clear input ownership even if cancellation finishes while RMB is still held. Later release does nothing; a fresh press works. Consume readied-throwable mouse context even when activation is refused, so firearm aim/melee cannot leak through. Respect quick-select and CommonUI release gates.

Validate release against exact action, montage instance and expected phase/cue. Preserve immutable commit receipts and replay-safe Begin/Commit semantics. Guard callback-producing transaction stages so last-unit readiness changes cannot cancel/refund/destroy a committed payload or resurrect an ended ability. Mark activation ownership before observable projectile callbacks. Clean all failure paths, including failed Cancel playback, without timer-generated throws.

## 4. Finish the original item and weapon scope

The live grenade is still BounceAndSettle/recoverable. Implement **server fuse beginning at physical release**, persistent through ability end and movement stop, exactly-once explosion, cover-occluded/deduplicated radial damage through existing AZ GAS, explicit self/friendly policy and replicated effects. Armed grenades do not become normal pickups on stop. Consume their payload intentionally on detonation. Merely selecting FuseAndDetonate in the enum is insufficient.

Deliver the real stone definition/pickup, impact hearing with correct thrower attribution and recoverable unit. Deliver equipped-knife source resolution in addition to ReadyItemId, unique GUID transfer, impact/embed/recovery and no duplicate pickup. Handle recovery conversion failure and lifetime expiry explicitly.

Use one profile resolver for GA, held prop and preview. Wire actual MH rifle/pistol profiles and token-owned Equipment presentation, throwing-hand IK/overlay suppression, camera/reticle ownership and restoration. Local mouse interception does not replace authoritative action mutual exclusion. Keep throw camera separate from firearm precision zoom.

Initialize remote projectile and held-item visuals from replicated authoritative presentation state; replicating actor position alone does not unhide or assign a mesh. Bound finite ballistic inputs, prediction work and horizon; implement an explicit zero-gravity policy matching actual flight. Preserve the repaired projectile response-container collision logic.

## Delivery

Make staged, reviewable changes; finish independent work before asking for a needed build/editor handoff. Report changed source/assets, successful build/load/save readback, calibration evidence, and what is implemented versus user-tested. Keep the completion audit current rather than repeating the initial foundation defects as still open.

After a successful build, request the user's manual check for visible style, close/far hand release, cancel-while-held then release/new press, UI/source changes, final stack unit, wall/body/open-sky preview, grenade resting/airborne/covered explosion, armed contexts, knife recovery and observers. No new automated tests. Do not call the whole system complete with only the visual pass or a recoverable grenade prototype.
