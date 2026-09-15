**Mantle transition diagnosis — user-run capture, September 14, 2026**

The user confirmed that the main complaint is **starting the climb after Space**, rather than returning to locomotion afterward, and that the camera problem occurs **during the climb onto the barrier**. Standing works by the user's report. Current scope here is diagnosis and instructions for the implementing AI; no gameplay source/assets, settings, PIE or tests were changed/run.

Reviewed gameplay interval: approximately 16:01:31.752–16:02:34.184 UTC in [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log). Current mantle source predates the 15:02:04 UTC DLL, and the editor started at 15:14 UTC. The observed behavior is consistent with the current source.

**What the log establishes**

There are 10 successful mantle starts: five Walk, five Run, all Relaxed. No standing mantle or Neutral comparison occurs in this particular capture; this does not contradict the user's standing test elsewhere. Successful activation changes the Mover mode on the next frame, 9–11 ms later. The current logging begins after successful activation and contains no raw Space timestamp, rejection reason or initial pawn-to-lip distance. Therefore it cannot measure total input-to-success latency or identify which exact attempt was the user's late press.

| Variant | Count | Time in Traversing |
|---|---:|---:|
| Walk Relaxed L | 3 | 1.877–1.878 s |
| Walk Relaxed R | 2 | 1.278–1.284 s |
| Run Relaxed L | 1 | 0.949 s |
| Run Relaxed R | 4 | 0.982–0.986 s |

These are whole active traversal intervals, not measurements of the time before the climb begins. The approximately 0.6 s Walk L/R difference warrants per-clip entry/contact measurement; it does not justify forcing equal durations.

**Primary issue: entry ignores distance and outgoing locomotion phase**

The current detector chooses approach by instantaneous speed: Stand below 60, Walk 60–250, Run 250–2000 cm/s. It has maximum reach/distance per band, but no calibrated minimum distance or distance-dependent montage entry time. Montage selection is a direct approach/style/foot lookup. The task starts at the default beginning, at rate 1.0.

Consequently, a close and a distant press at the same speed can play the same entire approach. Motion Warping fits displacement but does not remove the time spent performing the approach steps. Compression/lurch/hesitation on a close press is a strong explanation for why pressing earlier looks better, but individual visual attribution still needs entry-distance logging.

The capture also proves speed alone can select a moving mantle from a stopping pose: at 16:02:32.498, `WalkFwdStop_LU` is about 0.389 s into its stop, movement input is zero, speed is 82 cm/s, and the selected mantle is Walk Relaxed R. See [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9915](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9915).

Implementation direction:

1. Record a result for every fresh Jump press: request ID/time, selected action or failure reason, raw intent, movement/reaction/locomotion state, actual speed, approach history if used, ledge distance/facing, selected montage and entry time.
2. Select an entry using distance **and** speed, outgoing pose/foot phase and current intent. Do not replace the current speed-only lookup with a distance-only rule that indiscriminately chooses a standing clip at full running velocity.
3. Audit each candidate's root distance to contact over its legal pre-contact entry interval. Output both montage and entry time. A closer press can use a later compatible entry or a genuinely suitable shorter approach. Preserve required warp windows/notifies and contact setup; do not skip straight into an unsupported contact pose.
4. Keep playback at 1x. Use calibrated spatial correction limits. If no safe pose/distance fit exists, return an explicit result rather than warping an arbitrary clip over any distance.
5. A small traversal Chooser can own candidate filtering and data selection, with a pose/distance evaluation step as needed. Moving selection into CHT without providing this missing context does not solve the problem.

Sources: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp:172](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp:172), [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Mantle.cpp:116](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Mantle.cpp:116), [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.cpp:36](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.cpp:36).

**Confirmed input-priority bug**

`TryStartMantle()` returns false for an already-playing impact reaction, with a comment saying the press is dropped. `GA_PawnJump` interprets every false as ordinary-jump fallback and sets Jump pressed. This does not enforce the user's choice that a committed reaction finishes.

Return distinct outcomes such as Started, NoCandidate and BodyBusy/Consumed. Only genuine no-candidate may route to normal Jump. A committed reaction consumes/rejects the press, without buffering a late action. An accepted timely mantle should suppress newly pending cosmetic reactions; it does not cancel an already committed reaction. Avoid clearing all cached movement input as a narrow reaction reset.

Sources: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp:287](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp:287), [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PawnJump.cpp:66](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_PawnJump.cpp:66).

**Secondary confirmed issue: moving exit restarts locomotion**

All nine moving exits select an ordinary Walk/Run forward-start at time zero. Eight return to the loop after another 0.617–0.627 s; one is interrupted by Falling. Example: Walk exits at 16:01:49.323 with speed 201, plays WalkFwdStart, and reaches the loop at 49.949 with speed 124. Run exits at 16:01:55.125 with speed 390, plays RunFwdStart, and reaches the loop at 55.743 with speed 310.

The SM deliberately returns IdleLoop during Traversing. Its next grounded dispatch therefore treats held movement as starting from idle. CHT selects the start requested by that state; this is not evidence that the chooser is randomly stealing the action. Current successful handoff releases movement and fades the montage, but provides no explicit moving-resume pose/state contract.

Do not solve this by letting the entire normal locomotion SM run unrestricted under the montage. That can run starts/stops invisibly, arm their timers and root-motion requests, and finish a stop before the actual body has been released. An already-running hidden cycle is also not automatically phase-matched to the mantle.

Use an explicit traversal exit context or constrained preparation of the target loop. Match the actual outgoing mantle pose and prime the successor near handoff. Held input plus momentum resumes the appropriate cycle/transition; released input plus residual momentum gets a visible stop when movement is released; settled state goes idle. Keep all locomotion root-motion producers and side effects suppressed while traversal owns movement. Runtime search cost is a secondary consideration, not the reason for keeping a false Idle state.

Sources: [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:139](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:139), [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:1964](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:1964), [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:7634](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:7634), [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:7908](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:7908).

**Camera and landing anomaly: separate evidence, potentially visible in the same test**

There are 147 `CameraStep` records, none within a Traversing interval, and no detailed `CameraStepTrace`/`CameraStepEnd`/`ProceduralFeetTrace` records. Actual final camera/PlayerCameraManager motion during the mantle is unmeasured. Grounded terrain smoothing does not accumulate new step correction while Traversing, although an existing correction tail can decay then. Because the user locates the complaint during the climb, the separate landing anomaly below must not be presented as its established cause. Measure the warped capsule path, rendered body, spring-arm collision/lag and final view during Traversing before choosing a camera correction.

At 16:02:23.746, the ordinary jump/landing path reports root motion 4.8 cm XY and -3.54 cm Z over a 0.00127 s interval, producing 3784 cm/s XY and -2791 cm/s Z. Falling changes to Walking; subsequent CameraStep records propose correction from -18.43 cm to the -40 cm clamp while feet stay at Z=2.15. Similar sequences repeat. This is published movement/filter evidence, not proof the pawn actually travelled at 37 m/s or that the view applied the full logged offset.

The camera filter compares actual capsule-bottom travel with integrated Mover velocity. Downward published velocity against a floor-constrained capsule can look like a positive terrain step and accumulate a false downward camera correction. Investigate root-motion consumption/substep timing and the distinction between applied versus proposed movement; do not mask it by arbitrary velocity clamping or increasing camera half-life.

Use existing `az.Cam.Debug 1` for the next user-run reproduction: one mantle and pause on top, separately followed by one ordinary jump/drop. Correlate final camera/PCM/boom, actual capsule movement, mode transitions and root-motion drive identity. Disable verbose tracing afterward. No agent-started PIE or tests.

Sources: [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9557](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9557), [C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9562](C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log:9562), [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter_Camera.cpp:221](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter_Camera.cpp:221).

**Scope recommendations for the next iteration**

- Prioritize entry selection and the reaction/fallback result bug, since starting the climb is the user's main complaint. Correct the proven moving-exit restart as a separate, small handoff change.
- Do not delay every normal Jump by a 200–300 ms buffer. First fix entry selection. A future buffer should be specific to a known nearby traversal candidate, explicitly bounded, revalidated, and never turn a reaction-blocked press into a delayed mantle.
- Air catches are a separate traversal feature with different airborne detection, clips and movement ownership. Defer them; they are not needed to fix a grounded mantle whose entry starts at the wrong phase.
- Preserve standing completion, 1x playback, authored actors first, explicit Relaxed/Neutral setting, and the agreed committed-action policy. Do not add low step-over/vault/hurdle execution during this correction.
- Keep the landing velocity/camera investigation separately identifiable, while including it in the user-run observation pass because it can affect the perceived experience.

No gameplay source or animation assets were modified during this diagnosis. The draft's existing uncommitted changes were preserved.
