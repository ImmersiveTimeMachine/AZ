**Reusable SmartActors traversal surface data — implemented and saved**

September 14, 2026 Toronto / September 15 UTC. User authorized implementation under `/Game/AZ/Blueprints/SmartActors` after confirming four preceding code fixes. The new layer describes surfaces and feeds the existing traversal detector; it does not grant abilities or implement a new climb-over animation.

**Delivered**

- `UAZ_TraversalSurfaceProfile`: reusable runtime DataAsset with permitted mantle/hurdle/climb actions, standing/top-contact and crossing semantics, descriptive Gameplay Tags, edge endpoint margin, and MeshBounds / CustomBounds / AuthoredEdges geometry.
- `UAZ_TraversalSurfaceData`: runtime UAssetUserData that can be attached to a StaticMesh or a primitive component. Component data overrides the mesh default, including explicit disabled or invalid data. Only absence of both falls back to existing GASP splines.
- The detector now resolves explicit surface data, selects an eligible front edge and declared opposite edge, retains physical height/support/destination checks, and filters actions using surface permissions. Explicit unsafe-top profiles provide zero planted-foot support. Actual measured support is capped by a declared paired depth where available; a single authored front lip still uses real support measurement.
- Read-only `DescribeSurface(component)` reports the effective provider, profile, flags, tags and world-space edges. Rejections distinguish missing, disabled, malformed and geometrically unsuitable providers.
- Bounds conversion supports upright yaw, nonuniform scale and mirrored XY. Tilted/inverted-Z placement and explicitly configured ISM/HISM are refused with a diagnostic in this first version.

This storage is runtime asset data, not `EditorAssetLibrary.set_metadata_tag`. The latter is used only to identify the authoring script's ownership of its generated profile. Existing StaticMeshActors need no replacement or new Blueprint parent.

**Saved fence configuration**

| Item | Path / value |
|---|---|
| User mesh | `/Game/AZ/Blueprints/SmartActors/SM_fence_3` |
| Shared profile | `/Game/AZ/Blueprints/SmartActors/DA_TraversalSurface_Fence3` |
| Placed actor | `/Game/AZ/Maps/L_001.L_001:PersistentLevel.StaticMeshActor_13`, label `SM_fence_3` |
| Mesh size | Approximately 330.45 × 30.82 × 236.31 cm |
| Geometry | CustomBounds matching the measured mesh bounds; four paired top edges |
| Actions | Mantle false, Hurdle true, Climb false |
| Semantics | Standing/top-foot-contact false, crossing true |
| Extra endpoint margin | 5 cm, added to the querying capsule radius |
| Tags | Surface.Type.Fence; Surface.Traversal.NoStandingTop; Surface.Traversal.RequiresClimbOver; Surface.Hazard.PointedTop |

The rendered preview confirms a pointed iron top. Bounds describe the outer obstacle envelope; they do not certify a grippable hand anchor or standing support. At its current 236 cm height the fence exceeds the existing hurdle cap, and its thin pointed top is unsuitable for the current platform-ending climb clips. **Recognized surface is not equivalent to a supported fence-crossing action.** A future climb-over implementation must provide a compatible contact/animation/landing sequence rather than shrinking the declared height or inventing top support.

**Using the system**

1. Create a Traversal Surface Profile in `/Game/AZ/Blueprints/SmartActors`, or duplicate a suitable existing profile. Choose permissions and geometry appropriate to the asset. Tags describe it; the explicit permissions and physical checks control current action eligibility.
2. On a StaticMesh, add `AZ_TraversalSurfaceData` under Asset User Data and select the profile. All actors using that mesh inherit its data.
3. For one placement's exception, add the same user-data class to its primitive component and assign an override profile or disable it. Save the map for component overrides. A disabled component override must not fall back to mesh defaults.
4. For an irregular surface use authored local-space edges with horizontal outward normals and optional reciprocal opposite indices. Do not use arbitrary decorative splines as an implicit production provider. The unchanged legacy path remains available for the imported GASP blocks.

No ActorComponent tick, per-frame ability granting or new player-ASC availability tags were added. The detector remains an on-demand query; future UI/AI can consume the surface metadata and candidate result without changing the shared movement executor.

**Validation and preservation**

- Normal AZEditor build check returned `Result: Succeeded` (2.13 s, target up to date). Main AZ DLL timestamp is 2026-09-15 01:52:25 UTC, newer than the edited source; both new reflected class symbols were present and the restarted editor exposed them to Python.
- Independent source review passed provider precedence, signed-scale normal conversion, edge validation, width margins and preservation of the legacy path.
- Profile and mesh saves succeeded at 02:04:28 UTC. On-disk packages contain the profile reference/user-data class and descriptive tag names.
- Runtime provider readback on the existing fence returned `Resolved`, four paired edges, expected flags and all four tags. A representative GASP block returned `NotConfigured` from the new layer, preserving its legacy spline route.
- Before/after snapshots match actor identity/label/transforms, mesh bounds, material references, body setup and collision settings. No map was modified or saved; final dirty map/content package lists were empty.
- Preserved the preceding four fixes: Jump exclusion from Held input; `MeasureTopSupportDepth` instead of unlimited support; the empty-entry-sample warning; and `bRequireGroundedEntry`. No changes were made to the traversal executor, animation assets or locomotion/camera behavior in this step.
- No automated tests were added/run and no PIE/editor gameplay tests were started. User gameplay acceptance remains pending. The existing unrelated startup warnings were not changed.

The first shutdown requested from Python failed and the user restarted the editor. A normal Windows close subsequently succeeded. Two editor instances later held the same mesh file, producing Windows sharing violation 32 on save; closing only the extra instance we launched resolved the lock. The user's active editor/data were retained. Native compilation was verified before final asset authoring.

**Files and receipts**

- [C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_TraversalSurfaceData.h](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_TraversalSurfaceData.h)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalSurfaceData.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalSurfaceData.cpp)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_TraversalComponent.h](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_TraversalComponent.h)
- [C:/UnrealEngine/Games/AZ/Config/Tags/TraversalSurfaces.ini](C:/UnrealEngine/Games/AZ/Config/Tags/TraversalSurfaces.ini)
- [C:/UnrealEngine/Games/AZ/Tools/smart_actor_surfaces_setup.py](C:/UnrealEngine/Games/AZ/Tools/smart_actor_surfaces_setup.py): `main('audit'|'author'|'verify')`, run inside Unreal Python; authoring is idempotent and backs up the target packages first.
- [C:/UnrealEngine/Games/AZ/Saved/SmartActors/verified.json](C:/UnrealEngine/Games/AZ/Saved/SmartActors/verified.json)
- [C:/UnrealEngine/Games/AZ/Saved/SmartActors/before-author.json](C:/UnrealEngine/Games/AZ/Saved/SmartActors/before-author.json)
- [C:/UnrealEngine/Games/AZ/Saved/SmartActors/after-author.json](C:/UnrealEngine/Games/AZ/Saved/SmartActors/after-author.json)
- Baseline source/mesh backup: `C:/UnrealEngine/Games/AZ/Saved/SmartActors/Baseline_20260915_014406/`. Asset backups also exist under `C:/UnrealEngine/Games/AZ/Saved/SmartActors/AssetBackup_*`.

Authoring note: standalone `Config/Tags` files use repeated `GameplayTagList=` entries; the initial `+GameplayTagList` form did not register these tags. After correcting the file, a normal tag-settings refresh registered them. Generic settings objects required native PascalCase property names. GameplayTag/Container fields are read-only to Python; use their native `import_text` serializer after registration and verify `export_text` retains the names.

**Follow-up: Jump did not activate after Held exclusion**

The user's next check reported that ordinary Jump was unavailable. The previous Held exclusion was incomplete: `UAZ_AbilitySystemComponent::AbilityInputTagPressed` only forwards the press/replicated event to an existing ability; it does not activate an inactive spec. `AAZ_PlayerController::AbilityInputTagPressed` explicitly pulses fresh-press activation for crouch, aim/reload/fire, but had no Jump branch. Removing Jump from Held therefore removed its only activation route.

Added one `AbilityInputTagHeld(InputTag, false)` activation pulse for Jump in the controller's Started handler. Jump remains excluded from repeating Triggered/Held callbacks, so this restores one attempt per physical press without enabling retries/buffering. Release and the existing jump/traversal ability lifecycle are unchanged. Source: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:599](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:599).

UBT succeeded in 8.87 s; Live Coding applied the body-only change at 2026-09-15 02:11:17.671 UTC in the active editor (PID37692). No restart is needed now; include it in the next normal build before a future editor restart. No tests or PIE were started. User checks next: one press on open ground, one at an existing traversable block, then hold/release/repress. Current active-editor log is `C:/UnrealEngine/Games/AZ/Saved/Logs/AZ_2.log`; AZ.log belongs to the extra instance that was closed.
