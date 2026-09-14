# Animated corpse collision

Implemented 2026-09-13 after the user approved nonblocking animated zombie corpses with body-shaped foot/bullet queries. No ragdoll or looting gameplay was added.

## Behavior

- Death begins through the existing server-authoritative GAS death ability. The capsule immediately ignores Pawn and Camera but retains world collision while the collapse root motion plays.
- The exact successfully started death montage instance is observed until it stops. One further simulation frame is allowed for the last pose/root-motion delta before settlement. This replaces the fixed four-second freeze and the inaccurate division by an editable ragdoll fraction.
- Settlement queues the engine FApplyVelocityEffect with zero linear velocity, additive=false and ForceMovementMode=UNullMovementMode::NullModeName. This clears linear/angular velocity and holds the final transform through Mover's own replicated simulation. Deactivate remains the existing inactive-corpse latch; it is not relied upon to stop the NetworkPrediction backend. Mover is never destroyed or unregistered.
- The upright capsule is then NoCollision. The skeletal mesh uses its existing PhysicsAsset as QueryOnly/PhysicsBody, blocking Visibility and ignoring all other channels including Pawn/Camera. It does not affect navigation and is not a movement floor.
- The foot rig and firearm traces already query Visibility, so they can hit the fallen body while character movement passes freely. These queries also allow later corpse interaction work; the corpse does not yet have looting UI or inventory ownership. Visibility traces cannot see pickup items through a corpse.
- Existing replicated GAS Character.Dying and Character.Dead tags carry the two collision phases. BeginPlay binds weak callbacks and applies any state already received. Only authority freezes animation; observers keep their ASC-replicated death montage running to its held final pose. This does not implement the separately deferred late-join pose reconstruction feature.

## Live asset evidence

Active level during inspection: /Game/Post_ap_city/Maps/Showcase. Seven Chalkies use /Game/AZ/Blueprints/Character/Infected/BP_AZ_Chalkie with /Game/ZombiePackV1/ZombieAC/Mesh/ZombieAC_A/SK_ZombieAC_A and its sibling physics asset. Its 18 valid bodies contain 13 capsules and 5 boxes, enabled with UseSimpleAsComplex. Query-only animated shapes therefore support the complex Visibility sweeps used by both foot placement and firearms. Living meshes remain noncolliding and living capsules ignore Visibility, so living zombies do not become foot-IK surfaces.

No new collision channel, physics asset, AnimBlueprint or rig edit was needed. Receipt: C:/UnrealEngine/Games/AZ/Saved/ProceduralHero/corpse-physics-live.json.

## Code and validation

- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverInfectedCharacter.cpp: phase collision handling, GAS observer callbacks, exact montage completion and safe Mover hold.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Death.cpp: validate montage playback and pass actual collapse duration. The corresponding headers only update documentation/parameter names; no reflected layout was added.
- CLI build correctly deferred to Live Coding while the editor was open. Initial compile caught an AActor::Tags local-name collision; corrected to CorpseTags. Final UBT Result: Succeeded in 17.52 seconds; AZ Live Coding patch and reload succeeded at 04:19 UTC. A normal build before the next editor restart retains the patch in the main DLL.
- Independent source/engine and collision reviews completed. No automated tests were added and Codex did not start PIE or editor tests. User visual check remains pending: kill a fresh zombie, walk across it during/after collapse, confirm corpse stays on the ground and feet adapt, then fire at the body and past the old upright capsule location.

Event logs are [Corpse] collapsing and [Corpse] settled, plus the existing [Vitals] DIED. No per-frame debug tracing was enabled.
