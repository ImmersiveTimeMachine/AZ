# NextGenDestruction prototype

Latest user acceptance: reduced grenade debris scatter now works satisfactorily. Melee destruction works, but approach/strike feel is constrained by existing near-wall clearance; user explicitly defers refinement. Do not disable protection now. Next major block is world-object interaction, design first; see `C:/UnrealEngine/Games/AZ/docs/ai-memory/project_world_interaction_next.md`.

User authorized a test integration on September 22, 2026: understand the pack, connect accepted firearm hits, place wood/concrete/glass/ceramic samples in L_001. Production environment integration comes later. User performs PIE testing; no automated tests authorized.

## Implemented, built, placed and saved; user firing check pending

After restart, editor connected on L_001 with no PIE. Previous copied BPs were NOT on disk despite save returning true; initial save was dirty-only during the unresponsive editor state. Recreated all four copies with force-save (`save_loaded_asset(bp, False)`), verified actual uasset files (about 612KB each), configured CDO DataAssets and successfully spawned instances. Four actors now saved in L_001; no dirty map/content packages. Profile verified on a transient component: QueryAndPhysics, Pawn Ignore, Visibility Block. No PIE or firing test run by agent.

User reports editor hung during concurrent retargeting and is restarting it. Placement tool returned cancellation with no per-actor output. Do not attribute hang to destruction conclusively. Editor process exited; ordinary AZEditor build compiled both AZ_GA_FirearmFire.cpp and AZ_MenuRouteWidget.cpp, linked DLL, Result: Succeeded (14.83 seconds). User opens editor himself. Do not send editor mutations until restart complete.

- `Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmFire.cpp`: narrow native adapter dispatches `BulletImpact(HitInfo: FHitResult)` only to actors implementing `/Game/NextGenDestruction/Blueprints/Interfaces/BPI_Destruction.BPI_Destruction_C`. Validates reflected signature, initializes parameter memory, preserves accepted hit. Called once after successful ammo debit and existing GAS character damage. No second trace, predicted callback, or cosmetic-RPC destruction. Generic dust and floating bullet decal suppressed on handled hits; material retained, decal size zero.
- `Config/DefaultEngine.ini`: missing vendor `IgnoreCharChaos` collision profile added; QueryAndPhysics/Destructible, ignores Pawn and Camera, blocks Visibility. Restart editor to load config before acceptance.
- Four separate configured copies saved under `/Game/AZ/Blueprints/Destruction/Examples/BP_AZ_TestDestruction_{Wood,Concrete,Glass,Ceramic}`. Vendor assets unchanged. Each CDO DataAsset assigned and verified before saving.
- Original L_001 backup: `Saved/DestructionIntegration/L_001.before-destruction-20260922.umap.bak`. Do not delete untracked assets or this backup.

## Placement state

Initial four-actor placement request was cancelled during shutdown. Restart inspection confirmed zero existing examples. Subsequent placement completed exactly four actors. Folder `AZ_Test_Destruction`; labels `AZ DESTRUCTION TEST - Wood`, `Concrete`, `Glass`, `Ceramic`. Do not duplicate on continuation.

Final locations (cm): wood (-900,800,2), concrete (900,800,2), glass (900,-800,78.102375), ceramic (-900,-800,2). Static-mesh world bounds verified clear within 130cm horizontally. Floor top is Z=0. Actor bounds aligned bottom to Z=2; glass has centered pivot and was lifted accordingly. All four GeometryCollections assigned and block Visibility. Map save returned true, actual file updated to 1,421,088 bytes, dirty lists empty.

## Verified pack contract and limitations

- 16 destructible DataAssets, 14 GeometryCollections. Two demo maps including performance considerations.
- Samples: DA_Chair_Wood, DA_Pillar_Small_Concrete, DA_Window_Small, DA_Vase. Their damage radii .3/.4/.5/.5. Window optional static frame configured automatically by actor construction.
- BP_BreakableObject DataAsset property is required; base CDO is None. Configured copies avoid spawning unconfigured vendor actor.
- BulletImpact spawns BP_DestructionField at Hit.Location; default strain 2,000,000, radial velocity200, directional250, torque10. No damage scalar: weapon BaseDamage does not control fracture strength in this prototype.
- Direction helpers use PlayerCameraManager(0). Appropriate only for initial local-player test; not an AI/network/server implementation. Actor replication is disabled.
- Collection/optional mesh block Visibility, which the current firearm traces. Broken fragments switch to IgnoreCharChaos.
- Actor Override Damage Thresholds construction branch is disconnected in vendor graph; collection thresholds are authoritative. Selected collection defaults all [500000,50000,5000].
- Chair DA requests removal-on-sleep but collection asset has it disabled; verify actual component cleanup during user acceptance. Vase uses plaster physical material/VFX; preserve shipped behavior initially.
- User subsequently authorized grenade destruction. Native component-scoped adapter added to AZ_ThrowableProjectile.cpp and GeometryCollectionEngine/FieldSystemEngine dependencies to AZ.Build.cs. Calls after existing GAS damage/LOS, noise and FX in authority-once Detonate. Overlap only Destructible object channel, deduplicate GeometryCollection components whose owner implements vendor interface; apply strain2,000,000 and radial speed1500, squared attenuation to zero at Definition.DamageOuterRadius. Radius in actual cm, not actor scale. No global field, pawn/ragdoll force or unanchoring. Scenery destruction has no LOS test; pawn cover behavior unchanged. Vendor FX/break events remain. Grenade build verification in progress.
- Destruction save/load persistence, replication, AI shot directions and production performance are not implemented or validated.

## Next steps

1. User checks four samples with normal weapon: first and repeat hits, misses/obstructions, ammo, unchanged enemy damage, debris collision and cleanup. Do not start PIE without permission.
2. Read logs after the user's check; tune only from observed results. Full build and editor setup complete; no additional build needed unless code changes.

Grenade verification: Live Coding Result: Succeeded (16.41s), patch creation successful. Independent review found no must-fix; physics commands copy their field data synchronously. Current editor can be user-tested; ordinary closed-editor build still needed to bake grenade change into base DLL before a future restart. Same-tick radial velocity may miss newly released formerly kinematic pieces; verify launch behavior in user Play before adding delayed impulses. No agent PIE/tests.

## Melee extension (user authorized)

- Actual authority-only socket sweep contact routes through OnSweepBlocked. Added explicit physical-contact flag; initial body-to-limb obstruction and starting penetration cannot fracture props. Existing task consumes before callback; ability stops its window and ends activation after contact, preventing repeated destruction or cleave.
- Environment contact queries optionally include exact vendor-interface GeometryCollection components even when debris ignores Pawn. Capsule clearance/ordinary solid-world checks remain unchanged.
- Hero unpaired preflight may accept its earliest predicted destructible contact ONLY within an authored damage window; body path must remain clear, earlier walls still reject. Paired/warped attacks remain conservative. Preflight never applies damage. Cancel windows use same notify-state class and are explicitly excluded by tags.
- Selected strike sockets distinguish kick (foot/ball) from punch; heavy input randomly chooses either variant, so ability name cannot determine type.
- Test tuning: fist35cm strain750k/velocity200, kick60cm strain2m/velocity500. Local field centered at real ImpactPoint, directional velocity from socket segment, radius-multiplied, exact hit GC only. No unanchoring or pawn GAS changes. Body/paired teardown preserved.
- Native-only delegate/function signature changes; no new reflected fields/classes. Build/review in progress. User must validate free-space misses, walls, first hit/repeat activation, fist/kick, and normal enemy combat. No automated or agent PIE tests.

Melee build verified: UBT Result Succeeded (17.21s), AZ Live Coding patch successful. Independent code review found no must-fix. User is closing editor temporarily: PAUSED, no editor calls/builds pending. All authored examples and L_001 were saved before melee (code-only) work. Next: after user resumes with editor closed, ordinary full build to bake grenade+melee into DLL; user opens editor and performs acceptance. Live Coding patches alone do not survive restart.

User play feedback: grenade debris scatters too strongly, like fireworks. After editor restart/resume, reduced only native grenade radial delta velocity from 1500 to 300 cm/s (fivefold); strain2m, squared falloff, radius and GAS character damage unchanged. Build/Live Coding verification pending. Re-evaluate user result before changing strength or adding secondary physics tuning.
Reduced-debris tuning verified: UBT Result Succeeded (15.73s); AZ Live Coding patch successful. Ready for user comparison in current editor. Source saved; this tuning still needs ordinary closed-editor build for future restarts.
