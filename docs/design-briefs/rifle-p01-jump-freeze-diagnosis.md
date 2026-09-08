# Rifle jump freeze / repeat-jump block — 2026-09-07

User observed a frozen airborne pose and inability to repeat the jump. Two separate causes are established from actual gameplay, not the stale editor preview stream.

## Recorded baseline

In `C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log`, user PIE began22:20:55UTC. Rifle JogRU:

- 22:22:22.376: Walking→RMAction; `AS_P01_Jump_JogRelaxedRU_Takeoff` selected.
- 22:22:22.627: RMAction→Falling.
- 22:22:23.126: Falling→Walking; matching P01 landing selected.
- 22:22:23.371: landing→jog loop.
- 22:22:24.863: `[CmcJump] land-complete event never arrived within 2.5s`.

The next JogLU repeats successful physics/animation landing, followed by the same watchdog. The ability owns and blocks `Movement.Jumping`, so it stays active and rejects another activation until the timeout when completion is missing. The active original unarmed landing assets contain `UAZ_AnimNotify_SendGameplayEvent` / `Event.Movement.LandComplete` partway through recovery; all ten newly derived P01 landing assets lacked this event (they carried only BranchIn). Live original events and `[CmcJump] land complete (notify)` logs establish the actual contract. The GA's comment claiming an AnimInstance handback publisher is stale.

The separate visual freeze is the authored constant tail, not a stopped animation clock: the JogRU clip begins holding one pose at0.350s. The first jump spent about0.40s in that hold on a longer fall; eighteen log samples hold the same bones while the playhead advances normally. Other shorter flights hold for only0.024-0.036s. Shortening the overall4s asset would not repair a constant last pose. Evidence: `Saved/RifleAnimationContent/jump-freeze-timing-audit.json`.

## Changes staged separately

First active change: restore the gameplay event on the ten P01 landing sequences only. Moving beat=min(length/2,length-0.18); idle beat=min(length/2,0.35), ahead of the native standing-land0.40s early movement-resume gate. Preserve all bone/root data, curves, flags, BranchIn and notification tracks. Normal repeat jumps should produce `[CmcJump] land complete (notify) -> ending jump ability` without the2.5s watchdog. An arbitrarily early moving-land interruption can still precede a positive-time event; this is existing native behavior and must not be presented as universally solved by clip notifies.

Second, independently measured content change: replace only the static airborne tail with relative motion from the six existing P01 air animations. They are2s cycles, have zero root motion, and have endpoint seams below0.015degrees. Applying their absolute LU pose to RU would change leg rotations by up to112degrees, so a candidate must retain each existing held base and apply relative air motion. Root tracks and takeoff timing must stay unchanged; measure grip and landing-entry differences before activation. Initial measurements: `Saved/RifleAnimationContent/p01-air-variation-audit.json` and `p01-air-tail-proposal-audit.json`.

Keep these runtime changes separate for the first user validation, per the project's one-variable-per-test-cycle rule. Do not start PIE or automated tests. The user owns editor testing and lifecycle. Current build already succeeded22:19UTC; no build is needed for the notify-only change.

The notify-only fix is APPLIED and USER-VERIFIED in the22:41:48–22:43:02 run: ten completion events, zero watchdogs. P01 JogLU completion22:42:33.700 was followed by a fresh takeoff34.114 (0.414s later,1.318s after the preceding takeoff); two other repeat gaps were0.328s and0.255s. All ten modified landing sequences retained identical raw-bone/native-root hashes and BranchIn windows. Receipt: `Saved/RifleAnimationContent/p01-land-notify-prepare.json`.

## Additional cause of low walk-hop hovering

That user run exposed a separate PHYSICS hang on WalkRU: RMAction22:42:54.867 toFalling55.853 (0.986s), thenWalking55.879. Native root extraction at the exact logged playheads establishes the detector defect: z15.264cm at0.245s,12.624cm at0.269s (first descending tick),9.963cm at0.2935s (second descending tick). The detector required CURRENT net rise>10cm at the second descending tick; it forgot the earlier valid rise and waited until its1s safety expired. The constant root tail then held the capsule above the floor without gravity. Evidence: `Saved/RifleAnimationContent/walk-apex-key-trace.json`.

Corrected the existing accumulator to retain its qualification once it crosses10cm, while preserving the two non-positive ticks, existing threshold, timeout, animation root tracks and object layout. A `[JumpRise]` handoff line records apex/safety, elapsed time and qualified rise. Expected new walk-hop result: `handoff=apex` around0.3s, followed byFalling→Walking, instead of the one-second safety path. Live Coding build/verification is pending. The animated-tail candidate remains separate and unassigned; it addresses long physics falls, not this RMAction timeout.

## Additional observations awaiting separate repair

A post-restart audit found31 meaningful enum-cell regressions in previous ground-transition work: the C++ setter changed numeric Value but retained a copied ValueName, which UE5.8 PostLoad restores. The enum persistence correction is being prepared separately; these ground rows do not explain the measured airborne pose hold or missing completion event. Preserve all unrelated current code/graph changes.
