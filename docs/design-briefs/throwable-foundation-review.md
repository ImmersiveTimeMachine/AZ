# Throwable foundation review for Claude

September 16, 2026 local date. Static review of the new throwable headers/CPPs and additive collision profile. No gameplay code or assets were modified by this review; no build or PIE/tests were initiated.

**Continue with the ability and inventory transaction after correcting the foundation issues below.** The shared definition/profile/solver/projectile split is appropriate. Correct channel naming, first-impact marker position and inert preparation are useful steps, but they do not yet prove usable hero integration, collision parity or an atomic release.

**Final mapping explicitly confirmed by the user: hold RMB to aim, release RMB to throw, click LMB to cancel.** This supersedes the original two-click design and the intermediate alternatives. `WaitInputRelease` is suitable when routed to the correct active spec. Cancellation before physical release must disarm it; releasing RMB after LMB cancel must never throw or restart. Do not revert to historical click controls.

## Fix before wiring the playable action

### 1. Solver host does not match the actual hero

`UAZ_ThrowLaunchSolver::BuildSolution` accepts `const ACharacter*` and uses `ACharacter::GetCapsuleComponent()`. The active `AAZ_PawnMoverHeroCharacter` derives from **APawn**, not ACharacter. The future ability cannot pass it directly; `Cast<ACharacter>` would return null and produce NoDefinition on every attempt.

Use the actual hero/Mover interface or explicit mesh, capsule and movement inputs. Do not convert the character architecture to fit this helper. Current UE APawn::GetVelocity can delegate to IMovementInterface; it is not necessarily zero. Explicit Mover velocity remains the clearest expression of the intended source.

Sources: `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Throwables/AZ_ThrowLaunchSolver.h:41`, `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowLaunchSolver.cpp:29`, `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_PawnMoverHeroCharacter.h:58`.

### 2. Collision parity correction is still missing

The solver's body-to-hand clearance sweep and native prediction both use bare Ch2 traces. The runtime ThrowableProjectile profile ignores Ability, Pickup, Interactable and Projectile object types; those ignores are not supplied to the prediction sweep. Correcting `ECC_Projectile` to `COLLISION_PROJECTILE` fixes the channel name, not the response mismatch.

Implement the shared response-aware query contract from the Phase0 review, including the sphere's intended response container, ignore actors/components and trace complexity. An object that blocks Ch2 but whose object type the throwable ignores must not veto preview/launch. Conversely, do not blindly include trigger/corpse bodies through an object query when runtime would ignore them. Include explicit Vehicle handling.

Sources: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowLaunchSolver.cpp:91` and `:124`, `C:/UnrealEngine/Games/AZ/Config/DefaultEngine.ini:220`. Engine evidence and a concrete counterexample are in `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-phase0-review.md`.

### 3. Calibrated preview and live grip use different origins

Preview transforms a stored **bone position vector**. Live release transforms `GripOffset.GetLocation()` through the live bone transform. Any nonzero local grip translation therefore shifts the live projectile origin without a corresponding preview shift. A position-only calibration cannot reconstruct the offset when the release bone is rotated.

Store the per-clip release bone/grip **transform**, or a fully composed calibrated projectile-origin transform with an explicit coordinate-space contract. Compose the same grip offset in both preview and release. Include orientation for the held-to-flight visual handoff. The current live translation calculation is reasonable; GripOffset rotation by itself is not a separate spherical-origin error.

Also, when `bUseLiveGrip=true`, a missing socket/bone currently falls into the preview-anchor branch and may return Valid. **Reject this case explicitly.** Do not spend a unit and substitute an estimated origin when the authoritative grip lookup failed.

Sources: `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Throwables/AZ_ThrowPresentationProfile.h:104`, `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowLaunchSolver.cpp:55`.

### 4. Activation ownership is claimed after callbacks

`AAZ_ThrowableProjectile::Activate()` sets `bActivated=true` after `Movement->Activate(true)`. The engine component activation broadcasts synchronously, so a listener can reenter or destroy the actor while the guard still reports inactive.

Claim activation/transaction ownership before callback-capable operations. Validate the staged actor, components and definition before inventory consumption; handle destruction and failure explicitly. The current void, silently rejecting Activate API is not a sufficient contract for “inventory already spent, now launch.” The completed transaction must distinguish pre-commit rollback from post-commit world destruction without refunding or duplicating an item. Do not connect consumption to an actor that can reject activation silently after the spend.

Sources: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowableProjectile.cpp:58` and `:101`; engine synchronous broadcast: `C:/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:2815`.

### 5. Replicated movement does not initialize the remote projectile visual

Only authority sets the per-instance mesh and visibility. Definition is not replicated, the mesh component is not enabled for property replication, and `bActivated` has no OnRep initialization. A client copy remains hidden/unconfigured despite actor movement replication.

Add a coherent replicated activation/presentation snapshot and an idempotent visual initializer that handles arrival ordering. Clients present the accepted projectile; they do not independently spend, damage, detonate or create another authoritative projectile. Advanced client prediction can remain later work, but ordinary visibility should be part of the foundation.

Sources: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Throwables/AZ_ThrowableProjectile.cpp:65`, `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Throwables/AZ_ThrowableProjectile.h:76`.

## Smaller corrections and upcoming integration requirements

- `TimeToImpact = Result.LastTraceDestination.Time` uses the end of the simulation step. With a blocking hit, use the actual appended hit sample time, e.g. guarded `Result.PathData.Last().Time`.
- Validate finite/ranged aim, transforms, velocity, speeds, radius, gravity and inheritance at runtime. Editor ClampMin metadata is not validation. Bound both horizon/frequency and their combined step count; the current lower bounds alone permit unbounded/nonadvancing work. A real zero-gravity solution must either be rejected or explicitly implemented; the current comment says zero never occurs, but the code does not enforce that and native prediction interprets zero as world gravity.
- Keep stable provisional Close/Far selection during aim, then lock at commit. The comment in `AZ_ThrowableTypes.h` still describes choosing only at commit and should be corrected before it drives the ability implementation.
- Pistol cue times/anchors remain **provisional pistol calibration**, not universal measured defaults for every profile. Put them in a clearly identified profile/preset, validate cue ranges against its montages, and measure unarmed separately. Kinematic extrema are not proof of the precise release instant.
- Project-owned montages/notifies are the correct authoring location. The first delivered stone slice should still have its unarmed retarget unless the user explicitly changes that scope; a pistol-family technical prototype is not completion of the intended unarmed presentation.
- `HandleStop()` currently clears lifespan and broadcasts settlement regardless of `bRecoverable`. With no handler, the object persists indefinitely; flight timeout destroys it without a defined ownership disposition. Establish recoverable, disposable, expired and grenade-fuse outcomes when adding the transaction. A stopped grenade remains a live projectile outcome until detonation, not automatically a pickup.
- The projectile currently has no inventory GUID/state payload. Add the reserved one-unit payload before consumption is connected, so recovery does not depend on an ability that has already ended. Grenade fuse/damage and knife embed are visibly unfinished future phases, not existing implementations that this review claims have failed.

## Hold/release input details

Activation happens once on **RMB Started**. Keep the ability active during hold and use the canonical RMB release to commit. LMB cancels before physical release. Do not admit it to the Held retry path: cancelling while RMB remains down must not restart preparation. Correctly initialize the spec's InputPressed state so WaitInputRelease does not immediately observe a false “already released” condition.

Cancellation must disarm the pending release and require a fresh aim-button press. Releasing the still-held button after cancel, menu capture, item switch or interruption must not launch. If release arrives during Start, latch one intent for the same action and branch at the verified ready seam. Continue consuming both ordinary RMB Aim/SecondaryAttack routes where they conflict, and preserve quick-select mouse-release suppression. The other mouse button must cancel, not also fire/punch/aim a weapon.

## Build status and recommended order

The pasted status said nothing had compiled. During this review, the current UBT log already showed **Succeeded in29.46s**, explicitly compiling both throwable CPPs; that build was performed by another process/agent, not this reviewer. This demonstrates compilation of the snapshot, not correct host/API integration or gameplay acceptance. Recheck current timestamps before claiming another build is needed or that the source remains unbuilt.

Correct the host/query/grip/activation contracts first, then continue the action and inventory transaction, preserving exactly-once release and the current user control choice. Batch a coherent reflected build/restart when needed, but do not postpone integration checks merely to minimize build count. Use project build rules, keep source-pack assets intact, and leave PIE/gameplay testing to the user unless explicitly authorized.
