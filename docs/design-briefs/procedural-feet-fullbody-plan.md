# GASP procedural layers: AZ readiness and implementation plan

2026-09-12. Read-only analysis requested by the user; no game code/assets changed, no build, PIE or tests run.

## Conclusion

Both systems are implementable with the engine and source rigs already installed. They are not ready to activate unchanged on our rendered MetaHuman. Feet can be introduced first using the real animated foot/toe bones. Reliable foot locking needs contact-curve coverage and native state/reset plumbing. Interaction full-body IK needs a separate target provider and contact authoring; the exact GASP constraint evaluator also has a confirmed skeleton mismatch with our current paired animations.

The authoritative source in this project is now `/Game/GameAnimationSample/Blueprints/SandboxCharacter_Mover_ABP`, exactly as provided by the user. The old `/Game/Blueprints/SandboxCharacter_Mover_ABP` path in May memories/skills is absent. Do not use the old snapshots as current graph truth.

## What the two source layers do

### Procedural_Feet

The live layer uses `/Game/GameAnimationSample/Blueprints/ControlRigs/CR_Biped_FootPlacement`. This is a Control Rig terrain/foot placement solution, with an alternative Basic Leg IK branch; there is no native Foot Placement node in this layer.

The rig traces the floor, adjusts legs/feet/toes and pelvis height, smooths terrain changes, and pins feet during planted portions of an animation. It reads `contact_l` and `contact_r`; the measured source lock threshold is 0.97. Missing curves cannot provide reliable planting.

Source inputs include ground normal, movement-base world delta, raycast enable, foot-pinning and slope-warping gates, teleport detection and a reset pulse. Pinning is enabled on ground. Slope warping is allowed on ground/sliding. The relevant callback sets ForceFootPlacementReset on becoming relevant, and PostEvaluate clears it after consumption. Teleport detection uses a 50cm frame displacement threshold in this source; adapt reset detection to AZ's actual teleport/possession events rather than blindly interpreting fast movement as teleporting.

Useful source settings: pin radius40cm, pelvis/pin-release smoothing0.2s, floor smoothing0.05s, root damper0.15s, leg extension limit0.98, toe length5cm, primary sphere trace radius5cm using TraceTypeQuery1. These are reference settings to tune on MHC, not approved final character dimensions.

Grounded/default selects Control Rig or Basic IK with a0.2s blend. Sliding, Traversing and InAir use Basic IK; Ragdoll bypasses procedural correction. Basic IK references `ik_foot_l/r` and `foot_l/r`, so that fallback needs adaptation or a plain-pose bypass on our mesh.

Two source-audit traps: AllowFootPinning's disconnected AND/IsMoving node is not part of the actual return path; the editor node callback is Biped_FootPlacement_OnBecomeRelevant even though a nested runtime struct retains the stale OnBecomeRelevant name. FootPlacementMode is read elsewhere but does not select this layer's live branch.

### Procedural_FullBody

The live layer is paired-interaction hand correction using `/Game/GameAnimationSample/Characters/UEFN_Mannequin/Rigs/CR_UEFN_Mannequin_FullBodyIK`. It is gated by Enable_MMI_IK and an active interaction montage.

Its PBIK solver roots at pelvis and has two hand effectors. Hand targets drive both position and rotation alpha. It uses20 iterations, no stretch, and RootBehavior=PinToInput. Pelvis is pinned while the solver distributes hand correction through the relevant upper-body chains. Enable/disable blends are0.1/0.2s according to the live pins.

Game-thread Update_InteractionConstraints reads the pawn's MMIResult, selected multi-character asset, role, participating animation contexts, and active montage position. PoseSearchInteractionLibrary.UpdateConstraints extracts PoseSearch constraint notify windows and produces socket-to-socket constraints. GetConstraint retrieves hand_l/r reach weights/transforms, which are converted from world into mesh-component space for the rig's unparented target controls.

This supplies paired action contacts. Weapon grip targeting would need weapon socket targets and separate gates; those targets do not come from this source layer.

Verified source chain: authored locomotion/layers/ragdoll/DefaultSlot → Dead Blending → OffsetRootBone → Procedural_Feet → Procedural_FullBody → PoseHistory → output. Integrate the procedural passes into our own equivalent final pose path; do not replace our chooser, movement or equipment architecture.

## What AZ currently has

| Dependency | Verified current state | Required work |
|---|---|---|
| Engine modules | ControlRig, RigVM, FullBodyIK, AnimationWarpingRuntime and IKRig are loaded in the AZ editor | No new plugin download required; declare native module dependencies only where new C++ types actually need them |
| Main hero | Actual SKM_MHC_Hero_BodyMesh uses metahuman_base_skel; active AZ_ABP_MoverHero_MHC still targets compatible SKEL_SurvivalMan | Bind new procedural code to actual rendered mesh/reference transforms, not an assumed mannequin |
| Leg/arm chains | Actual342-bone mesh has root/pelvis, thigh/calf/foot/ball, spine_01–05 and both arm/hand chains | Validate axes and MHC proportions |
| IK targets | Actual rendered mesh has no ik_* or virtual bones | Start foot rig with UseIKBoneTargets=false; adapt fallback instead of copying missing-bone references |
| AZ foot rig | CR_AZ_Hero_FootPlacement exists, but is an older SurvivalMan rig; defaults pinning=false, raycast=false, debug=true, IK-target mode=true; no newer BasedMovementDelta | Prefer an AZ-owned adaptation of the latest source rig |
| Native plumbing | Active UAZ_MoverAnimInstance derives directly from UAnimInstance | Old UAZ_AnimInstance foot flags/helpers are not inherited; add current grounded/base-motion/reset snapshot and graph bindings |
| Contact curves | Several active locomotion sets already have contact_l/r | Audit all supported clips and author missing contacts before broad pinning |
| Paired gameplay | GAS grab/strike abilities and PoseSearch strike assets exist | Publish active interaction/target data and clear it on end, interruption, target loss and possession change |
| Full-body target provider | No active MMIResult, Update_InteractionConstraints or equivalent hand-transform/weight bridge | Implement an AZ-owned provider; a copied rig alone has no targets |
| MetaHuman postprocess | ABP_Body_PostProcess enabled; live graph has optional MHA head movement IK and RigLogic body correctives | Preserve it; validate final composition instead of replacing the postprocess |

Representative contact sampling, not a full coverage audit: unarmed walk/run, pistol walk and five actual rifle locomotion DB entries (walk explore/aim, jog explore, crouch explore/aim) have both contact curves. Sampled unarmed idle/crouch walk, pistol ready/relaxed idles, rifle standing/crouched aim idles and L/R punch source clips lack them. All sampled playback rates remain1.

## Confirmed extra blocker for the exact GASP interaction evaluator

Actual hero grab, L/R punch, heavy-punch and kick montages and their source clips use SKEL_SurvivalMan. The rendered mesh uses metahuman_base_skel. The real heavy/kick PoseSearch interaction entries reference those same montages.

Engine C:/UnrealEngine/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchInteractionLibrary.cpp:752 explicitly checks equality between the role animation skeleton and the context skeletal mesh skeleton. Compatible-skeleton playback does not satisfy this check. The evaluator additionally diagnoses nonidentity sampled animation roots; complete root coverage remains unverified.

Across16 sampled actual assets (5 montages +11 unique source clips), there are zero PoseSearchConstraint notify states. Attack montages instead have existing motion-warping and melee-window states; grab montage/source clips contain no notifies. Existing alignment/search results are not a substitute for those timed hand constraints.

Two valid implementation routes:

1. **Recommended AZ integration:** retain our GAS pair ownership and use an explicit mesh-aware hand-contact provider, with authored partner sockets/bones, local offsets, contact windows and reach weights. Feed the same adapted FBIK rig using component-space transforms. Reuse appropriate existing grab target sockets, but verify both position and rotation; the old disabled grab helper only supplies positional targets. This avoids requiring a wholesale GASP pawn/MMI port.
2. **Exact GASP evaluator:** prepare true MetaHuman-compatible role animations/montages, update the corresponding interaction assets/databases, author PoseSearchConstraint notify windows, validate root transforms, then expose the selected result/roles/contexts to UpdateConstraints. This is more content preparation than copying the layer.

One imported GASP takedown interaction also has an unassigned Attacker montage reference, though the expected montage asset exists. Repair it only if that specific sample interaction is adopted; it is not a dependency of the proposed feet phase.

## Proposed implementation order

1. **Feet: terrain adaptation first.** Duplicate the latest rig into AZ ownership, adapt MHC initial transforms/axes and FK foot targets, enable actual ground traces, and keep planting off initially. Provide smoothed ground normal, movement-base delta, grounded/air state and reset lifecycle through the active native AnimInstance. Choose sensible trace filtering against ground and ignore the owner/equipment. Insert after relevant authored action poses and before PoseHistory, preserving the existing output/postprocess chain. Unsupported full-body actions and ragdoll bypass it.
2. **Feet: planting coverage.** Audit the real chooser/database clip set, generate or author reliable contact windows, and enable pinning only on supported ground states/clips. Missing curves release pins. Tune trace reach, toe orientation, pelvis drop, extension limits and smoothing on flat ground, stairs, slopes, crouch, aim/turn and moving bases. Reset safely on teleport, mesh/owner changes and reentry. Keep the existing neutral OffsetRootBone behavior; these foot corrections do not require reenabling the root modes that previously broke turning.
3. **Full-body: one paired action.** Adapt the source rig to MHC and implement the recommended GAS-owned target snapshot for one grab/hold or shove. Carry exact action identity and target lifetime; read actor/mesh state on the game thread and pass copied transforms/weights into animation evaluation. Publish enough action/participant information for observers to derive cosmetic targets locally. Clear and blend out on interruption or contact-window end. Expand to other pairs only after that action is correct. Leave ordinary rifle/pistol aiming on its existing pose path.

Give feet and interaction IK separate enable switches and LOD limits. Keep animation playback at the user's required1x. Any new reflected native fields/functions require a normal build/editor restart; subsequent checks should include live node bindings, bone references, curve coverage, safe fallbacks and user-performed gameplay review. No automated tests or editor Play sessions are authorized by this research request.

## Evidence

- C:/UnrealEngine/Games/AZ/Saved/ProceduralAudit/feet-live.json: source feet nodes, pins, helper graphs and RigVM definitions.
- C:/UnrealEngine/Games/AZ/Saved/ProceduralAudit/fullbody-live.json: source layer, constraint updater and FBIK rig.
- C:/UnrealEngine/Games/AZ/Saved/ProceduralAudit/fullbody-interaction-assets.json: imported interaction assignments.
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_MoverAnimInstance.h: active parent/state surface.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_AnimInstance.cpp: legacy foot helpers, not inherited by the current hero.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_StrikeInteraction.cpp: existing pair search/alignment and discarded per-search result context.

This is a proposed adaptation, not a claim of runtime parity or completed integration.
