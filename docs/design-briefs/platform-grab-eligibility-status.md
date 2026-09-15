# Platform grab eligibility fix

Implemented and built September15,2026 UTC after Artur's screenshot showed an infected below a platform triggering the standing capture/escape pair on the player above. Live Coding applied the final patch in editor PID37864 at03:39:32.052 UTC. Gameplay acceptance is pending Artur's test.

## Cause and fix

The attack/grab selection used planar distance, and grab activation did not check elevation or scenery before rooting the victim and allowing the pair to overlap. Both the motion-matching alignment and legacy close-in discarded Z, so the paired animation could start across a floor/platform.

The shared, grab-specific `UAZ_GA_ChalkieGrab::CanStartGrab` now requires:

- Two grounded Mover capsule bodies with query collision.
- Foot-height difference no greater than45cm: normal40cm step allowance plus5cm floor slack. This measures capsule feet rather than centers, accommodating capsule-height differences.
- At most230cm planar separation, matching the existing rushing-target reach.
- Clear10cm-radius lower-body and upper-body corridors through scenery that blocks the character. Static and movable blocking scenery count; the two participants, their attachments and other pawns are excluded from these scenery checks.

Both interaction-search and legacy close-in destinations additionally need reachable walkable support, a body/head capsule corridor and an unoccupied final capsule. The corridor allows the normal step-height bottom slice while retaining capsule radius and head clearance. Existing one-grabber token and pack separation remain responsible for other infected.

Eligibility is checked during the grab roll, before attacker commitment/rooting/collision-ignore, again at deferred catch and before sending the victim event, and on the authoritative victim before its explicit lock, cancellations and paired-animation setup. Refusing the grab leaves ordinary melee selection intact. This does not implement airborne catches or new vertical paired animations.

A final review found that PSI could still set the leader's catch playback rate to0.8–1.2; the victim inherited it. The pair is now pinned to1x, honoring Artur's standing playback requirement. Motion matching may select the entry pose but cannot retime this player animation.

## Files

- `C:/UnrealEngine/Games/AZ/Source/AZ/Public/AbilitySystem/Abilities/AZ_GA_ChalkieGrab.h`: shared native helper declaration; no reflected fields added.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_ChalkieGrab.cpp`: physical eligibility, alignment clearance, start/commit gates and1x pair playback.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.cpp`: authoritative victim-side gate before explicit lock/animation work.
- `C:/UnrealEngine/Games/AZ/Source/AZ/Private/AI/AZ_BTTask_ZombieAttack.cpp`: grab-only selection gate; ordinary melee fallback preserved.

## Verification and next check

Root independently reviewed the geometry and action-selection changes. The implementing agent checked per-instance cleanup and confirmed the pre-catch zero-velocity move does not change Mover mode. Static whitespace checks and Rider analysis passed. The final coordinated C++ build returned `Result: Succeeded` in15.09s and explicitly compiled all changed grab files plus the HUD source. Live Coding reports successful AZ.dll patch creation at03:39:32.052 UTC. The source is live in the current editor; include it in a normal build before a later restart.

No animation assets, level geometry, collision presets or existing traversal code changed in this fix. No agent-started/stopped PIE or automated tests. Artur performs gameplay testing:

1. Stand on the raised platform with infected directly below; no capture/escape animation should begin.
2. Put solid scenery between actors on similar elevations; no grab through it.
3. Allow a normal close-range grab on the same floor, then escape; confirm the existing sequence still works at1x.
4. Repeat near an ordinary step/slope and while jumping or climbing onto a platform during approach.

The45cm standing-pair tolerance is deliberately conservative. Runtime observations can refine it without letting metadata or horizontal proximity override physical reach.
