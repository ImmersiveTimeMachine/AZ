# Weapon Grip System (WGS) - design

Status: **draft v1, 2026-09-27** (user + lead agent). Implementation plan with task cards:
`weapon-grip-system-plan.md`. Memory: `project_master_skeleton_2026-09-25.md`,
`project_winchester_integration_audit_2026-09-26.md`.

## 1. Problem and goals

The hero is a MetaHuman (`SKM_MHC_Hero_BodyMesh`, skeleton `metahuman_base_skel`; offline master copy
`SK_AZ_Master` = MH + bones `az_weapon_r` / `az_prop_r` / `az_prop_l`, mesh `SKM_AZ_Master`). Long-gun animation
comes from the RifleMega mocap pack, retargeted 1:1 to master clips `/Game/AZ/Assets/Master/RifleMega/**/AZ_MST_*`
(446 clips). The pack was captured on a thin UE4 mannequin, with ONE pistol-grip finger pose for every clip and no
finger contact; our guns are different models; our hero wears a sweater and a backpack.

Visible failures: fingers float or sink, the palm sinks into the stock wrist, the stock enters the sleeve / forearm,
and (expected, not yet measured) the weapon enters the torso and backpack straps.

**Goal:** every weapon is held naturally in every clip, verified by numbers, editable by eye, cheap at runtime.

**Non-goals (v1):** physics-driven hands, ML grasp synthesis, per-clip hand-made corrections, first-person view.

## 2. Key facts this design is built on (measured)

| # | fact | source |
|---|---|---|
| F1 | While a weapon is held, the hand-weapon relation is CONSTANT in every clip: the weapon hangs on a socket of `az_weapon_r` (child of `hand_r`, identity track outside reloads) and the left hand is IK-pinned to the weapon's `LeftHandGrip`. One grip pose per weapon is therefore valid in every clip. | node code, PIE probe 2026-09-27 |
| F2 | What does vary per clip: arms and body relative to the weapon (wrist bend, elbow height, crouch). | clip review |
| F3 | Reload / draw / shots move the weapon relative to the right hand through the `az_weapon_r` track (up to 33 cm / 85 deg in the Winchester reload). Curves `AZ_Grip_L` / `AZ_Grip_R` (baked into all 446 master clips) release each hand there. | 2026-09-26 |
| F4 | Offline curl-to-contact on exact triangles put every Winchester phalanx at -0.13..+0.8 cm. | `Tools/az_grip_solve2.py` |
| F5 | Runtime fingertip IK: 4 of 5 right fingers land 0.00 cm on their markers in PIE. | PIE probe 2026-09-27 |
| F6 | A right-hand placement tuned by eye put the palm skin 1.62 cm inside the stock wrist -> placements must be checked by numbers. | solver report 2026-09-27 |
| F7 | The stock enters the right sleeve in the Rifle02 hip carry (user screenshots). | PIE 2026-09-27 |
| F8 | The Winchester whole mesh is 8 overlapping part shells (Base + Elit variants): a ray-parity inside test on the merged mesh reports "outside" in overlaps. Use per-part tests and a union. | parts audit |
| F9 | Weapon collision for the arm test is cheap with capsules; triangle queries per phalanx per frame are not needed at runtime (see F1). | design review |

**Consequence (the central decision):** the expensive grasp math (contact of every phalanx, palm seat, trigger
finger) is solved ONCE per weapon - by hand (master grip) or by the offline solver - and the runtime only does what
really changes per frame: weapon vs body, arms vs weapon, left-arm reach, hand release, live marker tweaks.

## 3. Architecture

```
AUTHORING (per weapon, once)          DATA                                    RUNTIME (every frame)
+------------------------------+   +------------------------------------+   +---------------------------------------------+
| A. Master grip pose          |-->| Weapon mesh sockets (by eye, live   |-->| UAZ_WeaponGripComponent (hero, game thread) |
|    UE Sequencer FK rig,      |   |  in PIE): LeftHandGrip, Grip_<L|R>_*,|   |  weapon via CONTROLLER equipment -> inputs, |
|    or Blender, or solver     |   |  Trigger, Muzzle, Col_<n>_A/_B       |   |  status, debug        (pulled by the anim   |
| B. Derive tool: LeftHandGrip,|   | UAZ_WeaponGripData  (semantics)     |   |                        instance)             |
|    finger markers, checks    |   | AS_Grip_<Weapon>    (grip pose)     |   | UAZ_MoverAnimInstance -> node pins           |
| C. Validator over all clips  |   | UAZ_CharacterGripProfile (hero)     |   | FAnimNode_AZWeaponGrip v3, stages S1..S6    |
+------------------------------+   +------------------------------------+   +---------------------------------------------+
```

The component owns data, validation and status; the pose correction itself runs INSIDE the AnimGraph node (a
game-thread component cannot correct the evaluated pose without a one-frame lag or fighting the animation).

## 4. Conventions

- **Weapon model space** = the weapon skeletal mesh's `root` bone (identity), cm; +Y = muzzle, +Z = up, X = lateral.
  `SK_Winchester` root was fixed to identity on 2026-09-27 (the Blender export had written scale 100 + roll 90).
- **Attach bone** = the bone the weapon hangs on (`az_weapon_r`). Every runtime input is expressed relative to it and
  resolved against the CURRENT pose of that bone (no lag in reloads).
- **Socket names on the weapon mesh:** `LeftHandGrip` (hand_l bone target, i.e. the wrist, not the palm),
  `Grip_<L|R>_<Thumb|Index|Middle|Ring|Pinky>` (fingertip pad targets), `Trigger` (index pad intent), `Muzzle`,
  `Col_<Name>_A` / `Col_<Name>_B` (weapon collision capsule end points). Legacy `StockFront` / `StockButt` = capsule
  `Stock`.
- **Hero sockets:** `RightHand<Weapon>Socket` on `az_weapon_r` of `SKM_MHC_Hero_BodyMesh` AND `SKM_AZ_Master`
  (record in `Tools/hero_sockets.json`); the right-hand placement IS this socket.
- Units: cm; degrees in data, radians in math. Fingertip pad = end of `_03` + 0.85 x (offset `_02`->`_03`).

## 5. Data model

### 5.1 Weapon mesh sockets (geometry; edited by eye in the skeletal mesh editor; read live in PIE)
Sockets are the authoring surface because the user edits them visually and they may hang on moving bones (lever,
pump): a marker on the `lever` bone makes the hand follow the lever action for free.

### 5.2 `UAZ_WeaponGripData` (new `UPrimaryDataAsset`, one per weapon) - semantics
| field | type | meaning |
|---|---|---|
| `Mode` | `EAZ_GripMode` {TwoHandLongGun, HandOnHand, OneHand} | grip family (HandOnHand = pistol, left hand on the right hand) |
| `GripPose` | `UAnimSequence*` | master grip: frame 0 = pose of both hands' fingers; optional frame 1 = flex reference |
| `LeftHandGripSocket` | `FName` = LeftHandGrip | |
| `TriggerSocket` | `FName` = Trigger | index pad intent |
| `Capsules` | `TArray<FAZ_WeaponCapsule>` {Name, SocketA, SocketB, Radius, bAllowShoulderContact} | weapon vs arms / body |
| `FingerIKAlpha`, `FingerIKMaxChangeDeg` | float | per-weapon overrides |

`AAZ_Weapon` gets `GripData`; the current `GripPose` / `Stock*` properties remain as a fallback until migrated.

### 5.3 `UAZ_CharacterGripProfile` (new `UPrimaryDataAsset`, one per character / outfit)
| field | type | meaning |
|---|---|---|
| `Body` | `TArray<FAZ_BodyCapsule>` {Bone, A, B, Radius, Region} | torso / pelvis / thighs / head / backpack in bone space, seeded from the Physics Asset |
| `ClothingInflation` | float (cm) | added to body + arm radii (sweater, straps) |
| `ForearmRadius`, `UpperArmRadius`, `ForearmTestFraction` | float | arm capsules |
| `Fingers` | `TArray<FAZ_FingerAxis>` {Bone, FlexAxis, SpreadAxis, MinDeg, MaxDeg} | per-SKELETON flexion axes and limits (replaces axes derived from the grip pose, which are noisy for straight fingers) |
| `DistalCoupling` | float = 0.67 | DIP = coupling x PIP (anatomy) |
| `ThumbOppositionAxis` | FVector | thumb CMC second axis |
| `PushOutMax`, `PushOutSpringHz`, `ElbowSwingMaxDeg`, `ElbowSwingRateDegPerSec` | float | runtime limits / smoothing |

## 6. Runtime node v3 - stages and math

Inputs: `FAZ_WeaponGripInputs` (attach bone, left target in bone, markers, weapon capsules in bone space, flags
aiming / reloading) + `UAZ_CharacterGripProfile*` (immutable asset, safe on the worker thread) + curves.

- **S0 weights:** `A_L`, `A_R` = GripAlpha x curve `AZ_Grip_L/R` (else DefaultHandAlpha).
- **S1 body push-out.** For each weapon capsule `W_i` (end points from sockets through the attach bone) and body capsule
  `B_j`: `pen_ij = r_i + r_j + inflation - dist(seg_i, seg_j)`. Skip pairs allowed by `bAllowShoulderContact` while
  aiming (stock butt on the shoulder). Three Gauss-Seidel passes: `delta += n_ij * pen_ij` (n = unit vector between the
  closest points), clamp `|delta| <= PushOutMax`; smooth with a critically damped spring (dt cached in UpdateInternal).
  Result: the WHOLE two-hand assembly (weapon + both hands) moves by `delta`; the grip itself does not change.
- **S2 right arm.** Two-bone IK of the right arm to `hand_r + delta`, hand rotation kept, pole = the input elbow side.
  The weapon follows because it hangs on `az_weapon_r` under `hand_r`.
- **S3 left hand.** Two-bone IK to `LeftHandGrip` resolved on the (moved) attach bone. If the goal is beyond 0.99 of
  the arm length it is pulled back along shoulder->goal (no snapping); the reach error goes to the stats.
- **S4 arms vs weapon.** For each arm: the smallest elbow swing about the shoulder->hand axis (1 deg steps, last side
  preferred, rate-limited, <= ElbowSwingMaxDeg) so that `dist(forearm[0..70%], W_i) >= r_forearm + r_i` and
  `dist(upper arm, W_i) >= r_upper + r_i` for every weapon capsule. Hand transforms are re-emitted unchanged.
- **S5 fingers.** Frame 0 of the grip pose, then fingertip IK to the markers: 3 DOF per finger (base flexion, base
  spread about FlexAxis x finger direction, middle flexion; distal = coupling x middle), damped Gauss-Newton with
  backtracking (never ends worse than the grip pose), joint limits from the profile. Thumb: CMC flexion + opposition
  + MCP flexion, IP coupled. Blend by `A_L` / `A_R` (release in reloads).
- **S6 debug + stats** (`az.Weapon.Debug 3`): markers green, pads red, weapon capsules yellow, body capsules cyan,
  push-out vector magenta; last delta, swings and reach error go to the component status.

Order constraints: S1 before S2/S3 (the weapon moves first), S4 after S2/S3 (arms final), S5 last (hands final).
Budget: < 0.05 ms per character (8x12 capsule pairs + 3 IK + 10 x 12 x 4 finger FK).

**Current code (v2, 2026-09-27):** left-hand IK, finger IK to markers (3 DOF, backtracking, live-coded), right-arm
elbow swing vs one stock capsule, debug draw via `az.Weapon.Debug 3`. S1 and the profile do not exist yet.

## 7. `UAZ_WeaponGripComponent` (hero pawn, game thread)

- Resolves the held weapon through the CONTROLLER's `UAZ_Inv_CommonUI_EquipmentComponent` (a pawn lookup silently
  disabled the whole grip until 2026-09-27) and checks it hangs on its Relaxed / Aim socket.
- Validates data: missing sockets, missing grip pose, unknown skeleton -> a status string (no more silent failure).
- `BuildAnimInputs(FAZ_WeaponGripInputs& Out)` is PULLED by the anim instance from `NativeUpdateAnimation` (no tick
  ordering problem); the anim instance forwards it to the node pin and keeps no grip logic.
- Holds the `UAZ_CharacterGripProfile`; exposes stats (from the node, one frame late, display only) and a debug line.

## 8. Authoring the grip - two paths

**Path S - simple master grip (recommended first).** Because of F1, fixing ONE pose fixes every clip.
1. Open `AS_Grip_<Weapon>` (1 frame, skeleton `SK_AZ_Master`) in the animation editor with the weapon attached as a
   preview asset to `RightHand<Weapon>Socket`.
2. Right-hand placement = that socket (edit it on `SKM_AZ_Master` by eye, as the user did on 2026-09-27).
3. Pose both hands' fingers and the left hand on the fore-end: Sequencer "Edit with FK Control Rig" on the grip pose,
   bake back into the same asset. (Blender works too, but FBX round trips cost us a root scale-100 trap; UE avoids it.)
4. Run the derive tool: it writes `LeftHandGrip` (hand_l in weapon space from the posed frame), the finger markers
   (pad positions), and reports penetration / gap per phalanx and palm against the weapon (per-part test, F8).
5. Optional calibration: fix ONE clip's elbow by hand; the tool reads the clearance the user chose and sets
   `ForearmRadius` / `ClothingInflation`, so S4 reproduces that choice in every clip.

**Path A - assisted.** The offline solver (contact energy, section 9) proposes the pose; the user corrects it by eye
(sockets / markers are live in PIE); the derive tool re-bakes. Use it for weapons where hand posing is slow.

## 9. Offline solver (assistant) - math

- Surface: exact nearest-triangle distance per part; sign from per-part ray parity when the part is closed (edge
  manifold check), generalized winding number for open parts; signed distance = min over parts (union).
- Hand skin: 3 points per phalanx (25/50/75 %) offset toward the palm side by the segment radius
  (0.95 / 0.85 / 0.75 cm), palm grid points `PALM_T` below the metacarpal lines, thumb pad.
- Placement (right hand, 6 DOF near the current socket): `E = w_palm * sum softplus(-d_palm)^2 + w_gap * mean(d_palm)^2
  + w_trig * |pad_index - Trigger|^2 + w_dev * (|dt|^2/s_t^2 + |dr|^2/s_r^2) + w_arm * sum_frames penetration(forearm, W)`.
- Fingers (3 DOF each): `E_f = w_pen * sum max(0, m - d(p))^2 + w_con * sum_contact d(p)^2 + w_prior * |q - q_template|^2
  + limits`; damped Gauss-Newton with backtracking, multi-start (template / open / curled) to avoid the wrong basin
  (fingers inside vs outside the lever loop).
- Output = the same artefacts as Path S (grip pose, sockets, markers, report).

## 10. Validation (numbers, not screenshots)

Validator = Python replica of S1-S5 on sampled frames of the validation clip set (idle, 8-dir walk/run, crouch, aim,
turns, draw/holster, shoot, reload). Report per clip: worst frame and value for each metric; review actors placed in
the level for the worst frames (never saved).

| metric | threshold (held frames, A >= 0.99) |
|---|---|
| phalanx / pad penetration | <= 0.2 cm |
| pad gap for contact fingers | <= 0.3 cm |
| palm penetration / gap | <= 0.3 cm / <= 0.5 cm |
| forearm / upper arm vs weapon (with clothing) | <= 0.5 cm in 99 % of frames |
| weapon vs torso / pelvis / thighs (except allowed contacts) | <= 0.5 cm |
| left-hand reach error | <= 0.5 cm |
| push-out step / elbow swing rate | <= 1 cm per frame / <= ElbowSwingRateDegPerSec |
| node cost (`stat anim`) | <= 0.05 ms |

## 11. Already tried / ruled out

| attempt | result |
|---|---|
| Whole-finger opening; knuckle-axis opening (solver v1) | straight fingers / proximal segments stuck |
| Joint optimisation moving the palm freely | palm drifted 2.6-3 cm off the weapon |
| Anatomical search (v3) | straight fingers under the forearm |
| "Close until ANY point touches" | tip-only contacts |
| Baked finger pose only, no markers | breaks on every placement change |
| Placement by eye only | palm 1.62 cm inside the stock |
| Proximity-based right-hand curve | wrong in shell loading -> switched to the az_weapon_r track |
| Grip gather looking for equipment on the pawn | grip silently off in game until 2026-09-27 |
| Blender FBX (meters, bones +Z, apply unit) | root bone scale 100 + roll 90 -> sockets 100x away; fixed with SkeletonModifier |
| Tag registration / config write from a Live Coding harness | editor hang (patch init runs off the game thread) |

## 12. Risks and mitigations

| risk | mitigation |
|---|---|
| Thumb looks wrong with generic axes | dedicated CMC flexion + opposition model, profile axes measured once |
| Sweater / backpack thicker than the Physics Asset | `ClothingInflation`, one-clip calibration (8.5) |
| ADS: push-out knocks the stock off the shoulder | `bAllowShoulderContact` pairs while aiming |
| Push-out changes the aim point | translation only, <= 6 cm, smoothed; the aim ray starts at the camera |
| Lever / pump grips must move with parts | markers on the moving bone (lever, pump) |
| Pistol hand-on-hand | `Mode = HandOnHand`: left fingers target the right hand's skin; later phase |
| Parallel agents editing the same files | additive edits, per-hunk commits, specs list the exact files |

## 13. Open questions (user)

1. Path S first (you pose the master grip in UE; tools derive the rest) - or Path A (solver proposes, you correct)?
2. Clothing: calibrate on one clip by hand, or accept Physics Asset + a global inflation?
3. ADS shoulder contact: should the stock always touch the shoulder when aiming?
4. Order of weapons after the Winchester (Remington 870 has a pump -> first moving-part test)?
