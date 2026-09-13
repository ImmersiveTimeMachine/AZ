# Camera smoothing for terrain steps

2026-09-13 UTC. User requested a smooth camera rise when stepping onto objects. Implemented camera-only step-height correction; gameplay review remains with the user. No PIE or tests were started by Codex.

## Behavior

Mover applies StepUp/StepDown and floor snap immediately and excludes those corrections from its published relevant velocity. The camera now compares capsule-bottom travel with expected Mover velocity travel, projects the residual onto gravity up, and accumulates a bounded opposite offset. Ordinary ramp travel remains in movement velocity and is therefore largely excluded from the correction. Only grounded-to-grounded intervals add terrain smoothing; an existing tail can finish in air.

Crouch has separate private interpolation state. Its immediate capsule-height recenter does not become a stair because detection samples the capsule bottom. Final spring-arm TargetOffset combines crouch compensation and the vertical step offset before the spring arm's collision solve. XY movement, look input, aim zoom/FOV, directional framing and animation playback remain unchanged.

Defaults, verified live on AZ_BP_PawnMoverHero_MHC:

- Smooth Camera Steps: enabled.
- Camera Step Smoothing Half Life:0.08seconds (roughly0.35seconds for95% settling).
- Camera Step Max Offset:40cm.

These fields are under AZ|Camera|Ground Height in the hero Blueprint's Class Defaults. Larger half-life gives a softer/longer glide. Floor clearance bounds negative correction, and a short sphere sweep limits the shifted pivot near ceilings/geometry before the ordinary boom sweep. No global positional camera lag was enabled.

## Lifetime and moving bases

First view, controller/boom changes, large frame gaps, gravity-direction changes and real teleports reset the state. Crouch's expected height recenter is excluded from teleport handling. Teleport handling uses simulation-frame identity so consecutive teleport frames are distinguished.

The final body patch samples movement-base transforms per camera frame. It uses Mover's LastFoundDynamicMovementBase snapshot, which describes the transform actually applied to the pawn, then maps the previous foot point through the affine base delta. It does not consume OnBasedMovementApplied's simulation-frame batched payload every rendered frame. On departure/base identity change, new terrain detection is suppressed for that interval while the old correction decays and the baseline refreshes, avoiding double-counted imparted base velocity. Entering a base from static ground can still smooth its initial step.

The extra base/teleport-frame sample lives in a weak-keyed C++ cache so the post-build correction required no further class-layout restart; dead keys are pruned, reset removes the owner's entry. Promote it into the pawn during a future deliberate closed-editor layout build. The originally declared based-movement callback and pending field remain inert for compatibility with the user's already-built class.

## Files and validation

- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter_Camera.cpp: camera step/crouch composition, collision bounds and camera-frame base handling.
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_PawnMoverHeroCharacter.h: tuning and camera state.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp: calls the helper from the existing local camera update.

Compile-only check succeeded17.13seconds without applying new reflected fields to the old editor. User's normal build succeeded15.13seconds; DLL timestamp01:25:51.223 UTC and reopened editorPID48712. Final body patch compiled7.06seconds and AZ Live Coding applied01:38:47.145 UTC. Live readback confirmed enabled/.08/40. Whitespace checks passed. Camera composition/collision review passed; the base callback timing and departure edge found in review were corrected before handoff.

Saved receipt: C:/UnrealEngine/Games/AZ/Saved/CameraStep/defaults-readback.json. Compile-check log: C:/UnrealEngine/Games/AZ/Saved/CameraStep/compile-check.log. Use a normal build before a future restart to retain the final body patch in the main DLL.

User review suggestions: step onto/off static blocks and stairs, crouch while crossing steps, aim during elevation changes, test low ceilings and moving platforms. None were run by Codex.

## Follow-up diagnostics: user still sees a sharp rise

The user asked for logs and whether capsule motion is the cause. Existing CameraStep records show20–33cm step-down corrections, but no matching upward detections in the latest0.5-second half-life run. These original records were emitted before decay/collision, so they cannot establish the correction actually applied to the view. Do not infer a new smoothing-speed fix from those lines alone.

Added diagnostics only in the camera implementation file, controlled by existing az.Cam.Debug:

- CameraStepTrace (v2) records raw/base-compensated vertical travel, expected velocity travel, candidate residual/limit, current/previous grounded state, base validity, skip/stance flags, reset reason bits and rejection gate. Offset chain is input → proposed → decayed → floor-limited → final. It includes vertical pivot sweep hit/penetration/time/actor/component and final target offset.
- CameraStepEnd samples after the frame, recording actor/head height, actual camera component height, PlayerCameraManager view height and deltas, cached view time, the boom's desired height/collision correction and actual pivot. Same-frame capture identifiers correlate the two records.
- Logging windows open on vertical motion/state changes and continue through the correction tail; idle frames update baseline without flooding the log. The weak-keyed diagnostic map is separate from existing camera state. The end-frame delegate removes itself when tracing is off or no valid game pawn remains.

Reset bits:1init,2base-cache-init,4controller,8boom,16up-direction,32clock-backward,64frame-gap,128teleport,256large-displacement. Gates include reset/disabled/blocked/air/wasAir/baseHandoff/zeroDt/small/tooLarge/accepted.

Diagnostics compiled8.25s; AZ Live Coding patch applied2026-09-13 02:05:53.942UTC. az.Cam.Debug1 executed02:07:13.951UTC. No movement, camera tuning or collision behavior was changed. The editor then became busy/unresponsive in a separate material-edit operation; do not attribute that operation to camera diagnostics without evidence. User should reproduce the same step-up/down and stop Play so these new logs can be analyzed. No PIE/tests were started by Codex. Logging can be disabled with az.Cam.Debug0 after diagnosis, and needs re-enabling after an editor restart.

## Confirmed ascent failure and correction

The user's02:15 capture pinpoints the regression introduced by the conservative base-handoff guard. Frame147787: capsule/foot rise20.05cm, grounded1/1, residual20.05, reset0, no collision hit, but measure0/gate=baseHandoff; actual PlayerCameraManager view rises20.05cm in that frame. Frame147863 similarly rises32.67cm with a32.52cm view rise. Support changes from a tracked floor component to untracked block support (base=None), and the guard skipped the legitimate upward correction.

Descent already worked: frame147814 capsule falls20cm but view falls only0.52cm; frame147887 capsule falls32.67cm but view falls0.97cm. This establishes that the filter/TargetOffset/actual view chain works when the correction is accepted; changing half-life alone cannot fix skipped ascent.

The handoff now remains measurable when the previous support's transform is unchanged and both its linear and at-point physics velocity are effectively zero. There is no base displacement/imparted motion to remove in that case. Real moving-platform departures retain the conservative guard. Diagnostic version3 adds stationaryHandoff to distinguish this case. No camera tuning or capsule behavior was changed; the user's0.5s half-life is preserved.

Compile check passed7.09s. UnrealClaude timed out, so the applied Live Coding compile was triggered through Rider using the engine SystemLibrary console-command API; UBT succeeded7.78s and AZ patch applied02:29:00.332UTC. Source whitespace check passed; tracing remains enabled. User should repeat the same ascent for post-fix confirmation. No agent-started PIE/tests.

### User-run confirmation

The user's02:32 run confirms the stationary handoff correction reaches the actual PlayerCameraManager view. Frame179965: capsule rises20.00cm, stationaryHandoff1/gateaccepted, first-frame view rise0.62cm. Frames180035/180119: capsule rises32.67cm, first-frame view rise0.88cm. Frame180190:20cm rise produces0.45cm view rise. All have no reset, pivot hit or boom collision correction. Descents likewise glide:20cm capsule drops produce0.35–0.44cm first-frame view changes;32.67cm drops produce0.52–0.59cm. Current user half-life0.5s remains unchanged.

Startup falling/landing remains outside the grounded-step filter; the capture includes a spawn fall and startup frame gaps, which must not be confused with the corrected walking steps. Physical capsule recentering remains immediate. Verbose az.Cam.Debug tracing was disabled after reading the successful capture; diagnostics remain available to re-enable. No additional code changes or agent-started tests/PIE were needed for this confirmation.
