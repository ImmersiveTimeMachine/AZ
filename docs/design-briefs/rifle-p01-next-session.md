# Rifle P01 — next session

Checkpoint: September 7, 2026 local / September 8 UTC. User ended the session after log and pose validation. No gameplay/source changes were made during this validation; resume diagnosis before tuning.

## Accepted mechanics and build

- Use `docs/design-briefs/rifle-p01-jump-fall-handoff.md` as the current mechanics reference. P01 uses full `rm_W2_*_Jump` takeoffs, then a looping air clip only when the takeoff's remaining time reaches the existing 0.15-second threshold while still airborne. Ledge walk-offs enter air immediately. Real floor contact selects landing.
- This supersedes the earlier held-air-tail draft proposal. Do not resume `Tools/rifle_p01_air_tail_tuning.py` authoring or activate its unassigned drafts.
- **The old Live Coding-only warning is resolved.** Successful regular build compiled `AZ_LocomotionStateMachine.cpp` at 03:06:42 UTC and linked the main `UnrealEditor-AZ.dll` at 03:06:43, before editor startup at 03:06:51. User restarted. Source remains the clip-end gate, not the superseded apex gate.
- Reload confirmed P01 `bUseAirLoop=True`, 304 chooser rows, and both relaxed/aim air sequences `Loop=True`. The existing main chooser remains the sole selector. No new retargeting or IK work.

## Latest manual run

`Saved/Logs/AZ.log`, PIE 03:09:36–03:10:25 UTC, September 8. These line numbers belong to that file before the next rotation; locate its backup after a restart.

- 24 P01 takeoffs and 25 P01 lands (one ledge fall). All 24 paired gait/foot choices agree. No MM fallback, replay, snap, jump watchdog, or safety handoff in this run.
- 23/24 jumps logged landing completion. The last land was interrupted just before PIE ended; absence of a watchdog there does not prove completion.
- Takeoffs and air loops are direct-play. Moving landings use MM within the current BranchIn range 0–0.133333 seconds; the landing DB has a valid cached index. MM also selects locomotion entry after landing.
- Median paired landing costs remain similar to the prior 01:58 run: Jog LU .89→.90, Jog RU 1.165→1.08, Walk RU 1.395→1.385. Stable costs do not establish visual quality.

## Concrete findings for follow-up

1. **Jog LU landing entry has a geometric mismatch.** At 03:09:57.046 (log lines 10210–10211), outgoing full-jump playhead is .5025; MM enters the land at .066667. Actual-MHC sampled leg-pose RMS is **9.16 cm**, with foot errors 16.57/14.58 cm. The already-allowed .100000 entry gives **.63 cm RMS**, with foot errors 1.16/.98 cm. Root independently reproduced these values. Investigate query/history timing and feature costs before forcing an entry or changing weights: this metric is not the PoseSearch objective, and it does not include the final rendered blend or velocity matching.
2. **Jog RU has little time outside the landing blend.** Normal land phase lasts .206–.221 seconds, with a .20-second incoming blend. Jog LU lasts .613–.628 seconds. These durations match clip length minus selected entry minus the .15-second completion threshold. The contrast is content/timing, not random gait/foot selection. A soft impact is plausible, not proven from logs alone.
3. **Releasing movement can discard a landing immediately.** Final Jog LU land at 03:10:24.760 (line 12469) is replaced by direct idle at 24.807 (line 12472): only **47 ms**, speed still **381 cm/s**, movement intent 1→0. `AZ_LocomotionStateMachine.cpp` not-moving `TransitionToLocomotion` case falls through to idle without honoring `bLatchedJustLanded`/`TransitionEndTime` when `bStopOnAbortedStart=False`. The .411429-second completion notify cannot fire before that cut. This is a state-machine interruption, not an MM winner. PIE ended .356 seconds later, so this final cut may coincide with the user's test-ending input release.
4. Earlier shortened LU recoveries at 03:10:02.500, 19.684 and 24.263 are fresh jumps **after** completion events, not spurious MM reselections.

Pose receipt: `Saved/RifleAnimationContent/p01-jog-seam-audit.json`. Method: RAW animation poses, actual `SKM_MHC_Hero_BodyMesh`, retargeting enabled, root motion incorporated without extraction, curves disabled; unweighted pelvis-translation-relative position RMS across six thigh/calf/foot bones. Five allowed landing keys and 30/120 Hz loop phases were compared. RU touchdown selected entry was best (2.59 cm RMS). Land→loop RMS: RU 6.99 cm versus best 6.74; LU 9.18 cm with its selected phase already best. Loop phase adjustment alone therefore offers little improvement in those two samples. These are asset-pose measurements, not a rendered-animation test.

## Previously identified gaps still open

- Air rows 302/303 inherit the inverted `Movement.Sprinting` filter. The P01 profile remains active while sprinting, leaving no matching air row for a sprint fall. No sprint-air reproduction was performed.
- Source review: removing the air-loop profile while already in air can return the SM to takeoff and potentially queue root motion again. Not PIE-reproduced.
- Earlier PIE at 02:02:43–02:03:24 had five standing-jump one-second safety handoffs and two 2.5-second landing watchdogs after Walk LU land→idle cuts. Neither involved the new air-loop state; keep them separate from the clip-end gate.
- Aimed/crouched air and air-loop playback beyond two seconds remain unverified. Several source comments still incorrectly describe an apex gate.
- Do not run the old 302-row enum-repair/chooser preparation scripts against the current 304-row table without adapting and auditing their ownership guards.

## Working rules

User performs PIE; ask before starting PIE or editor tests. Do not add automated tests. Make one behavioral change per manual verification cycle, state the predicted deciding log line first, and compare against what worked in the prior run. Preserve other work in the shared dirty checkout. A zero-advance `AnimPro_JumpIdleStart` log after PIE teardown can be an editor preview instance; check the actual game world/lifecycle before diagnosing a freeze.

User was asked which phase looks wrong (touchdown, landing→locomotion, or takeoff/air); no answer arrived before session end. Do not treat the suggested default as their answer.
