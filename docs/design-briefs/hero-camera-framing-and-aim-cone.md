# Hero camera: directional framing + aim free-look cone

**Date:** 2026-09-09 · **Branch:** `spike/cmc-backport` · **Status:** built + **PIE-verified 2026-09-09 19:49** via `[v2 Cam]` (aim cone holds/drags/settles with zero drift; camera position constant under rotation); per-key aim offsets still to be authored

Two changes to the Mover hero, both driven by the same complaint: while aiming, the character does not
hold still on screen and the body cannot look one way while facing another.

---

## 1. The measurements this rests on

Anchor drift from the pose the body holds in **aim idle**, worst per axis across the 8 MetaHuman-native
aim loops (component space, +Y forward, −X right), measured 2026-09-09:

| anchor | forward | lateral | vertical |
|---|---|---|---|
| pelvis | +20.9 | 10.3 | −29.9 |
| **spine_05** | **+28.7** | **16.0** | −32.4 |
| head | +36.1 | 22.4 | −36.0 |

Every moving clip leans **20–36 cm forward**; jog L/R sways **16–22 cm** sideways. The −30 cm vertical is
the crouch stance, not drift. The aim idle's own breathing is only **0.3–0.6 cm** (crouch aim 1.4 cm), so
this is animation content, far above any noise floor.

Second, independent source: the boom is built with `bEnableCameraLag=true, CameraLagSpeed=8,
**CameraLagMaxDistance=50**` (`AZ_PawnMoverHeroCharacter.cpp:82-84`), so every acceleration slides the
character up to half a metre across the screen. **Addressed — see §4.**

---

## 2. Per-key aim framing (rewritten after the first PIE)

The first cut was **wrong in two ways** the user hit immediately (2026-09-09, "when I rotate the camera
position changes"; "directional offsets should work only in aim for W,S,A,D"):

1. **Keyed to velocity in actor space.** With the body parked inside the aim cone, turning the camera with
   W held swept the blend through the diagonals, and the camera slid sideways with every look.
2. **Delta semantics.** The entries were *added* to `SocketOffset`. The natural way to author "start from
   the idle framing" (copy `SocketOffset` into every direction, which is exactly what the BP had: (0, 70, 0)
   in all four Explore slots) doubled the offset while moving, and diagonals summed two entries to 1.41×.

Also, the per-stance lag defaults (8 / 50 cm) overwrote the boom's authored `CameraLagSpeed = 0` (= no lag
in the engine), silently giving Explore/Strafe a 50 cm trail they never had.

**Now:**

- **`CameraAimingDirectional`** on the pawn (AZ|Camera|Modes), one `FVector` per key: `Forward` = W,
  `Backward` = S, `Left` = A, `Right` = D. **Aiming only.** The per-stance field is gone.
- **Keyed to raw input in camera space** (`CachedMoveInputIntent`: X = W/S, Y = D/A, zeroed on release).
  A key means the same thing whichever way the body points, so a camera turn never changes the framing.
- **Absolute.** Each entry *is* the socket offset while that key is held, in `CameraAiming.SocketOffset`'s
  space (X toward/away, Y screen right, Z up). **(0,0,0) = not authored → keeps the idle SocketOffset** for
  that key, so you only fill in the directions you want re-framed. Two keys (diagonal) give the
  stick-weighted **average** of their entries, never the sum.
- No extra knobs. The resolved target feeds the existing `SocketOffset` interp, so a key press eases into
  its framing over ~0.4 s at the stance's `InterpSpeed` 8.
- **Lag defaults OFF in every stance** (matches the BP's authored boom). The stance owns the boom's three
  lag properties from here; nothing authored on the boom component itself survives the first camera tick.

`RotationOffset` is unchanged: per stance, applied to the **Camera** (never the boom, which carries control
rotation and aims the collision probe).

Starting values from the table above: `Forward` wants a larger +X than idle (push the camera back against
the ~29 cm forward lean); `Left`/`Right` want the idle Y shifted against the ~16 cm sway.

---

## 3. Aim free-look cone (60°)

Before, the aiming branch of `ProduceInput_Implementation` set `OrientationIntent` to the camera yaw every
frame, so the body chased the camera unconditionally — you could never look one way and hold the body
another, and a big camera swing dragged the whole character round over ~0.5 s (the walking mode ramps the
facing spring from `StrafeFacingTime` 0.10 s at ≤45° to `StrafeTurnFacingTime` 0.50 s at ≥135°).

Now, with **`AimFacingConeDeg = 60`** (pawn, **AZ|Movement|Facing**, clamped 0–90):

- The body holds a **frozen heading** (`AimHoldYaw`), latched where the body is on aim entry.
- **Inside the cone** — the target is that frozen heading. The residual twist is carried by the aim offset.
- **Past it** — the held heading advances exactly as far as puts the camera **on the cone edge**
  (`CamYaw − sign × 60`), never to the camera itself. A **drag**, not a snap-to-camera realignment.
- **Re-latch** — if root motion turned the body more than 30° away from the held heading (the aim
  turn-start clips carry 90/135/180° of yaw), the hold adopts the body instead of snapping it back.
- **0 restores** the old always-chase behaviour.

**Why frozen, not "current yaw" (the second bug, "I rotate the camera and it drifts"):** the first cut fed
the spring the body's *current* yaw as the in-cone target. The walking mode's facing is a spring-damper with
angular-velocity state (the engine's double spring). Given a target that moves with the body it sees zero
error and only ever damps, so after every drag the body coasted and the target coasted with it. A frozen
heading gives the spring something to settle *on*. Same doctrine as the strafe latch that already existed.

Movement stays camera-relative (`WorldMove` is built from camera yaw), so W is still "forward on screen"
while the body is off-axis; the chooser picks the matching diagonal strafe loop — all 8-way aim loops exist.

**Why 90 is the clamp, not a preference:** `AO_Rifle_Aim`'s samples span only ±90° yaw (L90…R90). A wider
cone runs the aim offset off the end of its blendspace.

---

## 4. Per-stance spring-arm lag

`FAZ_CameraStanceConfig` also carries `bEnableCameraLag` / `CameraLagSpeed` / `CameraLagMaxDistance`, so
each mode gets its own trail weight instead of the single value the boom is constructed with. The ctor sets
**`CameraAiming.bEnableCameraLag = false`**, so ADS is lag-free with no authoring; Explore and Strafe keep
today's feel (the struct defaults mirror the boom's ctor values).

Two engine facts this is built on, both read rather than assumed:

- **The fade goes through `CameraLagSpeed`, not the bool.** `bEnableCameraLag` is a hard switch: flipping it
  snaps the boom from "trailing by up to 50 cm" to exact in one frame. Interpolating the speed toward a large
  value (`LagDisabledSpeed = 1000`) completes the interp inside a frame — which *is* no lag — and arrives
  continuously. The bool on the boom is therefore held true and the stance's intent expressed through speed.
- **`CameraLagMaxDistance = 0` means UNLIMITED, not off.** `USpringArmComponent` only clamps the trail when
  the value is `> 0` (`SpringArmComponent.cpp:158`), so fading the cap toward zero would produce an infinite
  trail — the exact opposite of the intent. The cap is only interpolated while the stance has lag enabled,
  and the property comment carries the warning.

Rotation lag is deliberately untouched: it smooths the player's own look input and should feel identical in
every stance.

## 5. Failure axes to watch in PIE

1. **Cone + strafe clip mismatch** — with the body up to 60° off, "forward" input selects a diagonal loop.
   Correct by construction, but check the feet do not skate on the diagonals.
2. **Steady-state cone creep** — the facing spring is exponential, so while turning fast the effective cone
   sits slightly wider than 60°. Cosmetic; lower `StrafeFacingTime` if it reads as slop.
3. **Directional offset vs boom collision** — the offset moves the boom socket, which moves the collision
   probe. Test hugging a corner while strafing for oscillation.
4. **Aim entry while moving** — the per-key framing engages on the same tick the aim stance does, through
   the same interp, so ADS entry with W held is one move to the Forward framing, not two.
5. **Lag transition on ADS entry/exit** — `CameraLagSpeed` fades between 8 and 1000 at the stance interp
   speed, so the boom's trail collapses over the aim blend rather than snapping. Watch the entry for a
   secondary "catch-up" slide as the trail unwinds; raise the aim stance's `InterpSpeed` if it reads as two
   moves instead of one.

---

## 6. Telemetry

`[v2 Cam]` (`az.Cam.Debug`, default 1): one line at ≤4 Hz, only while the view or body turned or the socket
offset moved. Fields: mode, `ctrl` yaw, `body` yaw, `cam->body`, `hold` (+valid flag), **`bodyRate`** in
deg/s averaged over the interval, boom arm, current and target socket offset, camera relative rotation, boom
lag speed, raw move input. **A non-zero `bodyRate` while `ctrl` is still is the drift**; a moving `sock`
while `in` is (0,0) is a framing bug.

## 7. Pass lines — measured 2026-09-09 19:49 (`[v2 Cam]`, first PIE on the frozen-hold build)

| check | result |
|---|---|
| Aim, camera sweeps 116° inside the cone (cam→body −58 → +58) | body yaw **constant** (−118.6), `bodyRate` 0 on every line |
| Camera pushed past 60° | held heading advances with it; body follows at 45–97 °/s; cam→body pinned 60–69° (spring lag during a fast turn) |
| Camera stops after a drag | body settles on the held heading within **≤0.5 s**, `bodyRate` → 0 and stays 0 — no coast |
| Camera turned back inside the cone after a drag | body does not move (−23.7 held across a 70° return sweep) |
| Camera position while rotating (any mode) | `sock`/`arm`/`camRel` constant, lag speed 1000 — no positional drift |
| Explore idle, camera sweep 317° → 171° | body −42.3 throughout, `bodyRate` 0 |

One pre-existing thing the telemetry also caught, **not** from this work: in Explore, pressing W while turning
the camera picked `AnimPro_WalkFwdStart90_L`, whose root motion swung the body to −145° while the camera-relative
target was already at −54° — a 90° overshoot the facing spring then unwound over ~1 s (rates −257 → +169 °/s).
That is the unarmed turn-start clip fighting a moving target; it existed before the camera changes.

## 7. Pass lines (as designed)

- Aim, stand still, sweep the camera ±60°: **body does not rotate at all**; past 60° it turns and keeps the
  camera pinned at the cone edge; releasing leaves it where it stopped (no snap back).
- Aim, hold W and **turn the camera**: the framing must not change at all (it did — that was the bug).
- Aim and walk/jog in all 8 directions: the character holds roughly the aim-idle screen position once the
  per-key offsets are authored (before authoring, it drifts exactly as measured in §1). Explore is untouched.
- Explore and Strafe unchanged from before the build (their new fields are zero, their lag matches the ctor).
- Aim, then accelerate hard from a standstill: the pawn should **not** slide across the screen the way it did
  (that slide was the boom's 50 cm trail, now off for the aim stance).

---

## 8. Open

- **Anchor lock** — the fully automatic version of §2: pin a body anchor (`spine_05`) to its aim-idle screen
  position each frame, auto-capturing the reference while aiming and stationary. Proposed and deferred in
  favour of the simpler per-direction offsets; the measurements in §1 are what it would consume.
- `UAZ_PawnCameraMovementComponent` still holds the stance structs but is **only instantiated by the legacy
  `AZ_HeroPawn`** — the Mover hero does its camera work in `AAZ_PawnMoverHeroCharacter::UpdateCameraForMode`.
  If the component is ever adopted by the Mover pawn, the blend maths should move to a shared static helper.
