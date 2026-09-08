# Rifle step 0: content and contracts

Status: review package in progress after checkpoint 229f9b9. This is the requested content/contract step only. No runtime wiring, asset authoring, retargeting, build or editor tests have been performed for step 0.

User priority update: major systems first, using the existing rifle skeletal mesh as-is. The foundation audit is sufficient for the first pickup/inventory/basic-equipment slice. Detailed mechanism animation, magazine visual handoffs and final beat/socket tuning are deferred to their relevant milestones. This supersedes the earlier all-content-before-runtime gate at the end of the initial draft.

## Deliverables and ownership

- Codex: character-animation shortlist, existing retarget verification, rifle/magazine data model, action/commit semantics and mode/input policy.
- Claude: independent read-only weapon rig, mechanical animation, socket and detachable-magazine visual audit. Assignment: C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-rifle-step0-assignment.md. Report received and reviewed: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-weapon-rig-audit.md. Original audit is retained unchanged; integration refinements are recorded below.
- Machine-readable character inventory: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-character-assets.json.
- Machine-readable draft contract: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-contract.json. This is documentation, not a file loaded by gameplay.

The prior implementation proposal remains at C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-inventory-magazines-plan.md. The step-0 documents narrow that proposal into concrete assets and reviewable contracts.

## Confirmed foundation

The architecture is Mover + the current CHT/state-machine/BlendStack/MM approach. Rifle is another weapon profile alongside fists. A selected rifle supports exploration/lowered and aiming/fight modes, both standing and crouching. Sprint remains exploration-only. Character and rifle are separate animated rigs coordinated by one action contract.

Actual weapon actor: /Game/AZ/Blueprints/Weapon/AZ_BP_Rifle. Active weapon component: WeaponMesh3P, with /Game/AZ/Assets/M16/SKL/M16_Skeleton and its separate M16_Skeleton_Skeleton. Claude's report confirms the rigid-parts hierarchy and records its reference pivots. No weapon AnimBP or post-process AnimBP is assigned; the bounded registry audit found no mechanical clips for the M16 skeleton copies. A matching-part magazine prop exists at /Game/Assets/M16/mesh/UE4_M16_MagazMod. Driver assignments and prop existence were independently rechecked when merging the report. Subsequent read-only FBX inspection verified rigid skin influences, including 1,546 vertices solely weighted to UE4_M16_MagazMod. Final visual alignment remains unverified.

Actual character: /Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC, with /Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC. The intended rifle family is RifleAnimsetPro. Its located source clips use /Game/RifleAnimsetPro/UE4_Mannequin/Mesh/UE4_Mannequin_Skeleton. Exact ready retargets for this family have not been located. The known SurvivalMan-compatible set under /Game/AZ/Assets/RTG/Riffle_P01 is Rifle_01, a different source family.

## Character content shortlist

All paths in this table are under /Game/RifleAnimsetPro/Animations unless fully specified. The JSON inventory records 40 RootMotion candidates, eight InPlace action alternatives and 18 standing/crouched aim-offset sources, with existence, length, skeleton and actual flags.

| Role | Concrete source candidates | Step-0 disposition |
|---|---|---|
| Exploration lowered idle / raise | RootMotion/Rifle_Idle_GunDown; Rifle_Idle_GunDown2Idle; Rifle_Idle | Candidates verified; actual lowered movement coverage and raise/lower handoff need visual review. |
| Standing locomotion | RootMotion/Rifle_WalkFwdLoop, Rifle_RunFwdLoop, Rifle_SprintLoop; forward starts/stops | Use RM source for motion data/indexing as appropriate to existing Mover pipeline. Do not assume the source Loop flag is configured. |
| Aimed/fight directional movement | RootMotion/Rifle_StrafeLeftLoop, Rifle_StrafeRightLoop, Rifle_WalkBwdLoop, left/right45/135 loops | Existing CHT chooses the weapon/mode/gait pool; target clips need retarget and MM preparation. |
| Crouch idle / stance change | RootMotion/Rifle_CrouchLoop, Rifle_Idle2Crouch, Rifle_Crouch2Idle | Standing/crouch coverage is mandatory; pose compatibility with both rifle modes remains visual work. |
| Crouch directional movement | RootMotion/Rifle_Crouch_WalkFwd/Bwd/Lt/Rt and left/right45/135 variants | Eight-direction candidates verified, including the source spelling Rifle_Crouch_StrafeLeftt45. |
| Draw / holster | InPlace/EquipRifle and HolsterRifle, 1.800 s each; RM alternatives also exist | In-place action candidates avoid incidental capsule transport; choose based on intended action behavior and visual alignment. |
| Single fire | InPlace/Rifle_ShootOnce, 0.800 s | Timing/recoil-layer preparation required; no notify events. |
| Repeated-fire presentation | InPlace/Rifle_ShootLoop_Additive, 0.633333 s; source burst alternatives | Name is not configuration: actual additive type is AAT_NONE and Loop is false. Do not wire as an additive loop unchanged. |
| Crouched fire | InPlace/Rifle_Crouch_Burst, 0.500 s | Candidate only; firing cadence/shot cues must follow the common fire contract. |
| Standing reload | InPlace/Rifle_Reload_2, 2.166667 s | Candidate verified; semantic mag/bolt beats unmarked. Numbered suffix does not prove empty/tactical semantics. |
| Crouched reload | InPlace/Rifle_Crouch_Reload, 2.066667 s; Rifle_Crouch_Reload2, 2.400 s | Both verified; choose after hand/mechanism review. Same transaction semantics as standing. |
| Aim offsets | AimOffsets/Aim_* (nine standing); Rifle_Crouch_AimAdditive_* (nine crouched) | Source coverage verified. Target additive base/space and grip alignment must be checked. |

All inspected action candidates have zero notify events. Source clips named Loop in the audited RM shortlist have Loop=false. These are explicit preparation requirements, not grounds to discard the source content or replace the architecture. No raw asset flags were changed during this audit.

## Rifle and magazine definitions

The draft contract specifies:

- Existing gameplay profile Weapon.Rifle, separate from existing inventory category Item.Type.Weapon.Rifle.
- One stable rifle instance with a definition reference, location and optional inserted-magazine ID.
- One stable magazine instance with a definition reference, location and CurrentRounds in 0..Capacity. It cannot also be a backpack spare while inserted.
- Proposed first rifle definition ID Rifle.M16.First, compatible family M16.Standard, and magazine definition Magazine.M16.Standard30. These are proposed data identifiers, not newly registered assets/tags.
- Capacity 30 and rifle grid footprint 3x2 come from existing demo data and are proposed initial values. Magazine footprint and ammunition type remain open; do not infer magazine interchangeability from Weapon.Rifle alone.
- Existing Single fire mode and 10 rounds/second values are recorded as baseline/proposed tuning, not proven animation timings.
- Fists use the same equipment/action contract as an intrinsic non-droppable weapon entry. They have no magazine or backpack footprint.

Only inventory transactions mutate item locations and magazine rounds. Rifle actor counters and ASC ammo fields cannot become competing sources. A dropped rifle carries its inserted magazine record; transient hand/world props reference the same item ID.

## Mode and input contract

| Mode | CHT inputs and behavior |
|---|---|
| Exploration + rifle + Standing/Crouching | Weapon.Rifle plus existing stance/gait/state/direction/foot context; lowered rifle set; exploration movement. |
| Aiming/fight + rifle + Standing/Crouching | Weapon.Rifle plus aim/fight tags and existing stance context; aimed rifle set and directional fight movement; sprint blocked/cancelled. |

Rifle selection itself does not force fight mode. Aim requests that mode and releases only its own state when ending. Holster/switch ends the old aim/action ownership. No additional sticky-ready tier is assumed.

Two user choices were requested and remain open until answered:

1. LMB in exploration: require aim first (recommended for the stated two-mode design), or raise and fire from exploration.
2. RMB: hold to aim/release to exploration (recommended), or click to toggle.

R is reload for the rifle and remains heavy strike for fists, with only the selected profile granted the corresponding action. E and I are shared interaction/inventory commands. Crouch retains its existing ownership; each rifle action latches/selects an appropriate stance variant. Q is exploration-only.

## Shared animation/action beats

These are semantic labels, not new GameplayTags. Before registering tags, map them to the project's existing gameplay-event conventions. Every event is associated with ActionId and WeaponItemId; magazine actions also carry expected old/new magazine identities.

| Beat | Owning operation | Contract |
|---|---|---|
| DrawAttach | Equipment action | Attach the selected representation at the matching hand pose. No new inventory item. |
| WeaponReady | Equipment action | Publish usable active identity/profile once. Old/stale callbacks cannot ready a later weapon. |
| HolsterAttach | Equipment action | Attach carried representation; preserve rifle and inserted magazine state. |
| ShotAccepted | Fire ability / magazine operation | One accepted ShotId consumes exactly one round from the active inserted magazine. Character recoil and weapon mechanism/cues refer to that same accepted shot. |
| MagOut | Reload action | Present the old magazine leaving the rifle; inventory swap has not committed. |
| MagStowed | Reload action | Old-magazine presentation stows; its return space remains reserved. |
| MagPresented | Reload action | Selected new-magazine visual reaches the hand; selected item remains reserved. |
| MagInCommitted | Inventory transaction | Atomically swap magazine identities/locations, preserving both round counts. Duplicate delivery is ignored. |
| MechanismReady | Reload action | Complete any required charging/bolt presentation and release the fire lock. No second ammo write. |
| ActionEndedOrCancelled | Owning action | Clear reservations and presentation for this ActionId. Preserve original state before commit or completed state after commit. |

Standing and crouched reloads use the same semantic order at their own measured clip times. Character and weapon animation rates/interrupts follow the same action clock. Cosmetic weapon notifies never perform a second inventory swap, shot debit or damage application.

Exact beat timestamps are intentionally null in the contract. Current clips have no authored events, and the final character retarget/mechanical clips are not yet selected. Clip lengths alone cannot establish magazine insertion or trigger frames. Those anchors must be visually reviewed before authoring; no guessed fractions are presented as verified timings.

Reload handles an empty rifle magazine slot normally. An automatic reload chooses the fullest accessible/unreserved compatible spare containing rounds; a full inserted magazine makes ordinary reload a no-op. It retains old partial/empty magazines and preflights return space, accounting for the incoming magazine's vacated location. Stance-change interruption uses the same pre/post-commit rule until smooth in-reload stance transitions are validated. Separate chamber/+1 and loose-round packing remain proposed later scope.

## Socket and mechanical handshake

The known character sockets are BackRifleSocket, RightHandRifleSocketRelaxed, RightHandRifleSocketAim and Hand_LeftSocket. Their attachment bones differ, so existence is not visual alignment proof. The weapon-side LeftHandGrip/LeftHandGripAim sockets were independently confirmed. Muzzle, CasingEject and Magwell were independently confirmed absent. The actor's 1P and item-base mesh components are empty; WeaponMesh3P is the active representation to integrate.

### Integration review of Claude's report

- **Magazine prop:** /Game/Assets/M16/mesh/UE4_M16_MagazMod is the preferred existing candidate, matching the assembled part name. Referencing this existing project asset is consistent with current source-content use; no copy or extraction was performed. Exact alignment still requires visual comparison, including the prop's pivot offset.
- **Stable Magwell frame:** use a visible, fixed receiver frame (weapon hand_r or a verified receiver geometry bone), independent of the removable magazine subtree. Do not lock the socket to a bone that may later be hidden or animated out of the receiver. Even if hiding only UE4_M16_MagazMod leaves its parent magazine visible, a stable receiver attachment remains the clearer contract. Identity attachment is not assumed: the geometry child has a compensating reference offset and the standalone prop has its own pivot.
- **Skin weights resolved:** the original audit established hierarchy but could not read influences through its inspected Python/Blueprint APIs. A subsequent read-only ASCII FBX export exposes the skin clusters: all 10,485 vertices have one 100% influence; 1,546 belong exclusively to UE4_M16_MagazMod. Choose that geometry bone for HideBoneByName(..., PBO_None). Details: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-skin-weight-review.md and rifle-step0-skin-weights.json in the same directory. No manual weight preview is needed to establish this fact; visual handoff/alignment remains to be checked.
- **Muzzle frame:** a socket on a receiver bone can still be correctly placed at the bore with a valid offset. Parent name alone does not determine world position; select the parent based on attachment stability and validate the final transform/forward axis at the muzzle. The compensator bone is a candidate, not a mandatory rule.
- **Current firing evidence:** source confirms the missing-socket handling gap, but fresh BP_GA_Shoot defaults have FireSound=None and MuzzleFlashEffect=None. Treat the report's audio-at-pivot description as a potential fallback once configured, not an observed current audio result. Existing camera-origin targeting also means a source socket field alone is not proof of the actual shot origin. Require a valid muzzle before the new firing path becomes usable.
- **Mechanism beats:** hierarchy names do not establish which parts should cycle on every shot. Match firing/charging/empty-state motion to the actual mechanism and selected clips. On cancellation, restore the committed magazine and selected fire-mode/mechanism state; do not blindly reset the selector or every moving part to reference pose.
- **Visibility:** prevent two representations of the same magazine in the gun/hand handoff. Distinct magazine instances can legitimately be visible at once, for example a world magazine and a replacement in hand.
- **Secondary preview actor:** the audit flags two skeletal components with the same mesh in BP_Inv_EquipActor_Rifle_M16. This is a preview verification item, not evidence that the active AZ_BP_Rifle renders twice.

These refinements update the shared contract without changing Claude's factual report. Exact socket transforms and character/weapon beat timestamps remain unset pending visual validation.

Do not change the base rifle mesh/skeleton or shared source animations while the audit is running. Future authoring will use an explicit owned asset folder and one active editor writer per asset.

## Review/exit gate

Ready for review now: concrete existing actor/rig paths, source animation shortlist/flags, rifle/magazine identity model, two-mode CHT behavior, ordered transaction semantics and independent Claude assignment.

Remaining feature-specific decisions, not blockers for pickup/inventory/basic equipment:

- Confirm intended RifleAnimsetPro target retarget set, or approve preparing selected source clips using the established target-skeleton route.
- Verify magazine/socket alignment, then resolve the magazine footprint/ammunition definition and weapon-animation authoring specification. Claude's report delivery and the mechanical skin-influence gates are complete.
- Resolve the two input-policy questions.
- Visually select reload variants and map character/mechanical beat frames, with user permission for any editor preview/testing.

The first implementation milestone is now pickup -> inventory -> basic rifle/fist selection -> drop/re-pick, with stable rifle/magazine identities and round counts. Use basic attachment of the existing skeletal mesh; detailed presentation is not a prerequisite. Character retargets/input policy must be resolved for the aim/animation milestone, and a correct muzzle is required for shooting. This package continues to separate inspected facts from proposals.
