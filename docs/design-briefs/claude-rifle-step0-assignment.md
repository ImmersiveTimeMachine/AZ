# Claude assignment: rifle step 0, weapon-rig and magazine-content audit

Paste the following task into Claude. This is an independent part of step 0, not an implementation assignment.

## Task

Work in C:/UnrealEngine/Games/AZ. Read AGENTS.md and the project skills/memory before inspecting. The checkpoint is 229f9b9 on the current spike/cmc-backport branch. Do not switch branches or alter the Git index: other local work remains in the shared workspace.

We are locking content and contracts for a rifle with detachable magazines, using the existing Mover + CHT + state machine + BlendStack/MM architecture. Fists remain a weapon profile. Rifle has exploration/lowered and aiming/fight modes, both standing and crouching. The inventory owns real rifle and magazine identities/round counts. The weapon actor has its own skeletal rig/animations, coordinated with the character action; it does not own another ammunition counter.

Read the current proposal:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-inventory-magazines-plan.md

Also read the step-0 contract draft for shared vocabulary and pending decisions:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-contract.json
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-content-contract.md
These are documentation only. Exact animation beat times and input-policy choices remain open; do not silently decide them in the weapon audit.

Your ownership is **weapon-rig, mechanical animation, sockets and detachable-magazine visual verification only**. Codex owns character-animation selection/retarget verification, gameplay/item contracts, input policy, and the main step-0 report. Do not duplicate those tasks.

## Known facts to verify, not assume

- Gameplay weapon BP: /Game/AZ/Blueprints/Weapon/AZ_BP_Rifle (AAZ_Weapon).
- Active visible component: WeaponMesh3P; mesh /Game/AZ/Assets/M16/SKL/M16_Skeleton.
- Weapon skeleton: /Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton.
- Previous read found trigger, selector, charginghandle, catch, ejector and magazine bones; no assigned weapon AnimBP and no matching AnimSequences in the registry.
- LeftHandGrip/LeftHandGripAim exist; a Muzzle socket was not found.
- The character uses a separate MetaHuman body rig. Character RifleAnimsetPro clips do not automatically animate the M16 mechanisms.

## Deliverables

Write only:
C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-weapon-rig-audit.md

The report must contain:

1. Exact BP/component/mesh/skeleton paths and the current animation configuration. Identify unused 1P/base mesh components versus the active visible component without changing them.
2. Mechanical bone hierarchy and which moving parts can actually be animated. Verify usable geometry/bone weighting where exposed; bone names alone are not proof of deformation readiness.
3. Search existing assets for rifle-mechanism fire/reload/charging/empty-state animations or compatible donors. Distinguish character clips from weapon clips and compatible clips from merely similar names. Report searches and unknowns; do not invent absent content.
4. Socket inventory and proposed attachment contract for Muzzle, CasingEject, Magwell, GripLeft, and magazine hand/world presentation. Reuse existing sockets where correct. Report actual transforms as evidence, and label any new transform as a proposal requiring visual validation.
5. Determine whether a usable separate magazine mesh exists. Explain whether the built-in magazine geometry can be hidden/replaced cleanly, and how to avoid displaying two magazines. A visual prop is not an extra inventory item. No mesh extraction/authoring yet.
6. A weapon-presentation table for ShotAccepted, MagOut, MagStowed, MagPresented, MagInCommitted, MechanismReady and ActionEnded/Cancelled. These are semantic contract labels, not newly registered GameplayTags. Describe the mechanical pose/visibility required for each. Do not invent magazine transfer times from character clips; Codex owns that timeline mapping.
7. Missing work classified as asset authoring, rig/socket adjustment, runtime integration, or user visual verification. Include exact source evidence and a small ready/needs-preparation/unknown matrix.

## Boundaries

- Read-only editor/source inspection. Do not edit/create/save/retarget/reimport assets, alter Blueprints, add sockets or write C++/Python authoring utilities.
- Do not start PIE, inject input, run automated tests, build, Live Code, restart the editor, or play preview animations without asking the user first. Read-only metadata/bone/curve queries are allowed.
- Do not commit or stage. Do not edit the main plan or another agent's report. Do not send messages to external services.
- Prefer current live editor data and local source; memory may be stale. Use full absolute filesystem paths and exact /Game asset paths in the report.
- If a visual check is needed, state precisely what the user must inspect; do not mark it verified from a name or screenshot thumbnail.
- Use the existing AZ utilities before claiming a fact cannot be inspected. Keep tool output bounded. Do not add new runtime hooks to perform this audit.

Finish with a concise handoff: verified reusable assets, concrete missing items, proposed socket/mechanism contract, and checks still requiring the user's preview. This report will be merged into step 0 before any runtime wiring is authorized.
