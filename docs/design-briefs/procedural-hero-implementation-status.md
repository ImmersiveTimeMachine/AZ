# Procedural hero: terrain, planting and paired hands

Implemented after the user's approval of stages1,2,3 on2026-09-12; final build/wiring verification2026-09-13 UTC. All source/rig/curve/AnimBP changes are saved. Gameplay appearance remains for the user's Play review; no PIE or tests were started by Codex.

## Delivered

1. **Terrain correction:** an AZ-owned version of the current GASP feet rig, adapted to the actual342-bone MetaHuman body. It uses animated foot/toe targets, matching axes/reference transforms, real ground queries, bounded leg extension and smoothed foot/pelvis/root corrections. Missing mannequin IK bones are not required. The existing older AZ foot rig is not activated.
2. **Planting:**452 missing contact curves authored across226 used animation assets. Final coverage audit checks306 clips, preserving160 existing contact curves on80 clips. All native-key raw bone transforms, other curve keys, notifies, sync markers, metadata and animation settings were preserved around each mutation. Playback remains1x. Six conservatively sampled clips use all-zero contacts and therefore terrain adaptation without locking: pistol TurnL_90Loop, unarmed RunStrafeLeft/Right45Loop, RunStrafeLeft/Right135Loop, and WalkArchLoop_L. Full paths are in the verification receipt.
3. **Paired hands:** an AZ-owned adaptation of the source hand-effector PBIK rig for the existing grab **Wrestle** hold. A GAS-owned, replicated action identity supplies partner/montage information; each peer derives mesh-component-space hand transforms from its animated partner sockets. Targets have calibrated hand rotations, bounded correction/reach, a0.1s entry and0.2s release. Outcome, interruption, partner/mesh/controller loss, exact montage-instance change and action replacement clear the old contact safely. This first interaction is intentionally the grab hold, not every strike or weapon grip.

## Assets and graph

- `/Game/AZ/Blueprints/Animation/Procedural/CR_AZ_MHC_FootPlacement`
- `/Game/AZ/Blueprints/Animation/Procedural/CR_AZ_MHC_PairedHands`
- Active `/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC` now has `OffsetRootBone → Feet → PairedHands → PoseHistory`, with fourteen typed native-input wires.

The actual MHC hero subclass renders SKM_MHC_Hero_BodyMesh with this AnimBP, has one replicated PairedHandContact component, and retains its MetaHuman postprocess. Existing FullBody/RifleFire slots, pose layers, neutral OffsetRootBone bindings and1x playback path are preserved. Legacy grab TwoBoneIK/shake remains disabled.

Feet are independently enabled/gated by grounded movement, actual leg-bone availability, LOD and authored full-body action state. Paired grabs, physics/ragdoll, death and unsupported full-body actions bypass feet correction. Pins additionally require supported contact curves; actual curve values drive the rig's0.97 planting threshold. Missing/all-zero contacts release safely. WorldZDamperEnabled shares the slope-warping gate, including during fade-out.

The native based-movement adapter matches the source rig's unusual contract: publish rotation delta plus displacement evaluated at the **previous mesh-world origin**, since the rig rotates pins around that origin. The complete affine base delta is retained separately for teleport prediction. Reset handles mesh/component/owner/controller changes, stance changes, reentry and Mover teleport state. Continuous grounded base changes now release foot locks without clearing world-space terrain/root/pelvis smoothing; discontinuous base changes still reset. Simulated observers can use the rig's own traces when their local Mover floor-blackboard has no result.

## Step pop correction, 2026-09-13 UTC

The user's recording `C:/Users/Artur/Videos/NVIDIA/Unreal Engine 5/Unreal Engine 5 2026.09.12 - 22.46.21.04.mp4` shows an abrupt body rise around 1.65 seconds, while the camera glides. The corresponding log at 02:46:24.112 reports a +32.67cm capsule step; the locomotion loop had already been selected at 02:46:23.740, so no new animation selection coincides with this step. Extracted frames are in C:/UnrealEngine/Games/AZ/Saved/IKPopVideo/.

Live editor inspection confirmed the floor is marked Movable even though stationary, while the blocks are static. Crossing onto the block changes the Mover base from Floor to null. The prior unconditional base-change ForceReset cleared WorldZDamper/pelvis/pin history at full feet alpha, snapping the damper to the already-raised mesh. The native fix preserves the dampers on continuous grounded handoffs and disables pinning for that frame so the rig uses its existing smooth unlock/relock path. Teleport, identity, stance and relevance resets remain. No capsule/camera movement behavior or playback speed was changed.

A secondary source-copy typo in the owned rig's Reset function wrote the left floor reset to the right smoothed-floor null. Corrected that one pin, VM-compiled/saved and verified that all other graph pins/links and the imported source hash were unchanged. Receipt/backup: C:/UnrealEngine/Games/AZ/Saved/ProceduralHero/reset-left-floor-fix.json. The setup script includes the same correction for future fresh authoring; it was syntax-checked only, not rerun.

Body-only Live Coding succeeded in 7.87s; AZ patch applied at 02:54:17.332 UTC. No reflected layout change or restart needed for this patch. Run a normal build before a future editor restart to retain body patches in the main DLL. Independent source/rig reviews passed; user visual confirmation remains pending. No PIE/tests started by Codex.

`az.Cam.Debug 1` is enabled for the next user-run crossing. It now includes `[ProceduralFeetTrace]` with the reset reasons/eligibility/base handoff and `[CameraStepEnd]` with root/pelvis/foot/head and camera world heights after the published bone buffer flips. Do not read GetSocketLocation inside NativePostEvaluate for current-frame pose diagnostics: engine flips its double buffer later. Disable the debug cvar after collecting the user's next pass. Brief airborne flicker and repeated teleport flags are secondary possibilities only if future traces show them; do not change those unrelated policies without evidence.

## Native implementation and authoring tools

- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance_Procedural.cpp and the corresponding additions to AZ_MoverAnimInstance.h/.cpp: game-thread movement/contact snapshots, gates, reset lifecycle and rig inputs.
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_PairedHandContactComponent.h and Private counterpart: exact action identity, hand targets, calibrated defaults and replication/lifetime checks.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.cpp and header: begin/end contact hooks and GAS montage publication for the victim follower. Existing MontageSync_Follow still owns local paired timing at1x.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp: replicated component default subobject.
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_ProceduralRigEditorUtils.h and Private counterpart: safe fresh ControlRig node/dynamic-pin/getter creation without reconstruction or Python AnimBP compilation.
- C:/UnrealEngine/Games/AZ/Source/AZ/AZ.Build.cs: editor-only ControlRig/ControlRigDeveloper authoring dependencies.
- C:/UnrealEngine/Games/AZ/Tools/procedural_hero_rigs_setup.py and procedural_hero_contact_setup.py: guarded, backed-up, resumable asset authoring/readback.

Tuning is in the AnimBP Class Defaults under AZ|Procedural|Feet and AZ|Procedural|Interaction, on the new ControlRig node inputs, and on the hero's PairedHandContact component. Defaults include independent enable switches, LOD2, feet blend0.2s, normal smoothing0.08s, pin radius40cm and extension limit0.98. Hand socket offsets use the real AZ_Catch_Fight role origins and sampled Wrestle midpoint; translation offsets remain zero.

## Verification

The native compile-only checks succeeded (64.24s, then56.09s) without applying new reflected class layouts to the old running editor. The user then performed the normal build/restart: UBT succeeded49.57s, main AZ DLL timestamp2026-09-13 00:19:47.135 UTC, fresh editor PID20732 from00:19:56.135 UTC. Live reflection confirmed all new fields, bridge functions and component defaults.

Both rigs were adapted, VM-compiled and saved; source rig hashes remained unchanged. The hero graph was wired only after the new native reflection loaded, compiled through the dedicated native tool outside Python, and saved. Final live status is BS_UP_TO_DATE. The damper gate correction was recompiled/saved and read back as the fourteenth wire.

Final review found and fixed an observer-only hand-gating issue: the authority validates its local grabbed tag, while observers trust the replicated action plus exact local montage windows. That body-only fix compiled in6.84s and its AZ Live Coding patch applied at00:25:45.185 UTC, with no object changes. Use a normal build before a future editor restart to retain this final body patch in the main DLL.

Both independent reviewers approved all seven structural/native gates: live source, field types, function metadata, property defaults/replication, adapted logic, engine APIs and build/export. The right-hand rig alpha is deliberately double while left is float. Visual quality, moving-platform appearance and multiplayer runtime remain user-review items.

The curve pass briefly paused for an unrelated cancelled Delete Assets modal, then resumed and completed. No in-progress curve save remains. Curve FName display casing may normalize during Unreal's Sequencer-model synchronization; case-insensitive names and exact original key values were verified, rather than treating equivalent FName casing as data loss.

## Receipts and recovery

- C:/UnrealEngine/Games/AZ/Saved/ProceduralHero/: prepared/authored rig snapshots, original AnimBP backup, graph-before.json, graph-authored.json, verified.json, paired-hand-calibration.json, native-full-build.log, final-live-graph.json and final-rig-review.md.
- C:/UnrealEngine/Games/AZ/Saved/ProceduralContacts/: audit.json, author-session.json, verification.json and Backups/20260913T000313Z/backup-manifest.json.
- C:/UnrealEngine/Games/AZ/Saved/ProceduralAudit/: original source graph/rig evidence.

Do not rerun graph authoring after success; use verify. Do not activate the old grab IK or legacy foot rig alongside these layers. The old all-IK-off comment above bGrabIKEnabled refers to the legacy branch; the new layers have their own independent switches.

User visual review: flat ground/stairs/slopes, walking/running/crouch and turn transitions with rifle/pistol, jump/land release, moving bases, and the paired grab hold plus its release/outcome. These are review suggestions, not agent-run tests.

User confirmed the step fix works. Follow-up runtime traces at 2026-09-13 03:00:23?26 UTC show repeated +32.67cm/-32.67cm grounded crossings with baseChanged=1, handoff=1, reset=0, feet alpha=1 and pin=0 on the transfer frame. Temporary az.Cam.Debug tracing was then disabled and read back as 0.
