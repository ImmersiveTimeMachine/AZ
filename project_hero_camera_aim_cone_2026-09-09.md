---
name: project_hero_camera_aim_cone_2026-09-09
description: "★★ 2026-09-09 hero camera: every camera mode gained RotationOffset (applied to the CAMERA, not the boom) + per-direction SocketOffset deltas blended by travel velocity; aiming gained a 60 deg free-look cone (body holds inside it, is dragged at the edge past it) instead of chasing the camera every frame. Built OK. Defaults are no-ops until authored. AO_Rifle_Aim's +/-90 yaw samples are the hard cap on the cone."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-09T23:04:57.165Z
---

# Hero camera framing + aim free-look cone (2026-09-09)

Full record: `docs/design-briefs/hero-camera-framing-and-aim-cone.md`. Built editor-closed,
`Result: Succeeded`. **All new values default to zero/neutral — nothing changes until authored in the pawn
BP defaults**, so an "it looks the same" report after this build is expected, not a regression.

**Where the camera actually lives.** `UAZ_PawnCameraMovementComponent` holds the stance STRUCTS but is only
instantiated by the legacy `AZ_HeroPawn`; the Mover/MetaHuman hero does all camera work in
`AAZ_PawnMoverHeroCharacter::UpdateCameraForMode` (local controller only). Adding logic to the component
alone is dead code for the current hero — a full session nearly went that way.

**Added to `FAZ_CameraStanceConfig`** (so all five modes get it — Explore/Strafe/Aiming/Grabbed/GrabOutcome):
- `RotationOffset` (FRotator) applied to the **Camera**, never the boom: the boom carries control rotation
  AND aims the collision probe, so rotating it fights both.
- **Per-key aim framing — REWRITTEN after the first PIE (two strikes on the first cut).** Now
  `CameraAimingDirectional` on the pawn (aim only), one FVector per KEY (W/S/A/D), keyed to
  `CachedMoveInputIntent` in CAMERA space, **ABSOLUTE** socket offsets with **(0,0,0) = unauthored → idle
  SocketOffset**, diagonals AVERAGE (never sum), fed into the stance's own SocketOffset interp. No knobs.
  The first cut failed on two axes the user hit at once: (1) blended by velocity in ACTOR space, so with
  the body parked in the aim cone a camera turn with W held swept the blend through the diagonals and the
  camera slid sideways on every look; (2) DELTA semantics — the user authored "start from idle" by copying
  SocketOffset (0,70,0) into all four slots, which doubled the offset. **Rule: a per-direction camera value
  is absolute and keyed to the KEY, never to body-relative velocity.**

**Aim cone.** `AimFacingConeDeg = 60` (AZ|Movement|Facing, clamp 0-90). The aiming branch of
`ProduceInput_Implementation` used to set `OrientationIntent = camera yaw` every frame. Now the body holds a
**FROZEN heading** `AimHoldYaw` (latched on aim entry); the camera pushing past the cone advances it to
`CamYaw - sign*Cone` (camera pinned to the cone EDGE, a drag); root motion turning the body >30° re-latches
it. 0 restores the old behaviour. Movement stays camera-relative (diagonal strafe loops while off-axis).
**★ RULE — never feed a facing spring "the body's current yaw" as a hold target.** The first cut did; the
walking mode's facing is a spring-damper WITH angular-velocity state, so a target that moves with the body
sees zero error and only damps → the body coasts after every drag and the target coasts with it = "I rotate
the camera and it drifts". Hold = frozen state (the strafe latch already did this for the same reason).
**Telemetry:** `[v2 Cam]` line (`az.Cam.Debug`, 4 Hz while turning): `bodyRate` non-zero with `ctrl` still =
drift; `sock` moving with `in=(0,0)` = framing bug. Before this there was NO camera trace in the log.

**Hard limits / numbers worth keeping:**
- `AO_Rifle_Aim` samples span only **±90° yaw** (L90…R90) — that is why the cone clamps at 90; past it the
  aim offset has nothing left and the pose breaks.
- Facing spring (`AZ_PawnMovementMode_Walking.cpp:189-205`) ramps `StrafeFacingTime` 0.10 s (≤45°) →
  `StrafeTurnFacingTime` 0.50 s (≥135°) — that ramp was the "body is bound until it faces forward" feel.
- Body drift from the aim-idle pose while moving (measured, 8 aim loops): torso **+29 cm forward**,
  **16 cm lateral**; head 36/22. Aim idle's own breathing is 0.3–0.6 cm. These size the directional offsets.
- **Per-stance spring-arm lag** (second build, also Succeeded): `bEnableCameraLag` / `CameraLagSpeed` /
  `CameraLagMaxDistance` on the stance struct; **default OFF in every stance** — the hero's boom was authored with
  `CameraLagSpeed = 0` (= no lag in the engine) and the first cut's struct defaults (8/50, the C++ boom
  values) silently gave Explore/Strafe a 50 cm trail they never had. The stance OWNS the boom's lag props
  (rewritten every tick); values on the boom component never apply. Two engine facts behind it: the fade runs through `CameraLagSpeed` (toward
  `LagDisabledSpeed` 1000) because the bool is a hard switch that would snap a 50 cm trail to exact in one
  frame; and **`CameraLagMaxDistance = 0` means UNLIMITED, not off** — `USpringArmComponent` only clamps when
  it is `> 0` (`SpringArmComponent.cpp:158`), so fading the cap to zero gives infinite trail. Rotation lag is
  left alone (it smooths the player's own look input).

See [[project_rifle_mh_native_migration_2026-09-09]], [[feedback_mover_visual_component_two_writers]]
(camera-side only — Mover owns the mesh transform, never move the mesh to fix framing).
