---
name: AZ v2 — multi-pawn class design (hero + future vehicles: car/motorbike/helicopter)
description: Architectural commitments for scaling the v2 character system to multiple possessable pawn classes. Hero on foot is first; cars, motorbikes, and helicopters are planned (long-term roadmap, 2026-05-11). Covers input architecture (PC vs pawn split), camera per pawn, seat/possession, ASC layering, Mover modes per pawn, cross-pawn tag taxonomy, network authority, and the folder layout to lock before file count grows.
type: project
originSessionId: 6c1c8fd6-056f-42e0-87d3-c943f4c8cf3d
---
The v2 character system is designed for **one hero pawn now, many vehicle pawn classes later** (cars, motorbikes, helicopters confirmed long-term, 2026-05-11). These rules are locked in before implementation so the first vehicle is not a refactor.

## Input architecture — PC vs pawn split

**PlayerController owns** (stable, pawn-agnostic):
- IMC stack lifecycle (gameplay / menu / dialogue / dead — push/pop per game state)
- Cross-pawn shortcuts: pause, screenshot, photo mode, scoreboard
- Ability input routing via `UInputConfig` DA → ASC (Lyra-style)
- On `OnPossess`: query pawn for its IMC, push it; on `OnUnPossess`: pop

**Pawn owns** (per-class input surface):
- `SetupPlayerInputComponent` for its movement IAs
  - Hero: Move / Look / Jump / Aim / Crouch / Sprint
  - Car: Steer / Throttle / Brake / Handbrake / Gear
  - Motorbike: Steer / Throttle / Brake / Lean
  - Helicopter: Cyclic / Collective / AntiTorque / Throttle
- Cached input state members that `ProduceInput_Implementation` reads
- `GetDefaultMappingContext()` accessor (or `IInputContextProvider` interface) so the PC stays pawn-agnostic

**Why this split:** new vehicle = new pawn class + new IMC + new `SetupPlayerInputComponent`. **Zero PC changes.** PC doesn't grow `Cast<>` ladders to learn each pawn's surface. AI parity: pawn-side cache is the universal input surface — AI BT writes directly; player Enhanced Input writes via `SetupPlayerInputComponent`; both feed the same fields read by `ProduceInput`.

**Why NOT all-on-PC (Lyra-style):** Lyra's pattern works when one pawn has many input *modes*. AZ has many pawn *classes* with non-overlapping input surfaces. All-on-PC scales as PC bloat: every new vehicle grows PC bindings + cast ladders.

## Pawn / mode / anim layering per pawn class

| Layer | Hero (now) | Vehicles (future) |
|---|---|---|
| Pawn C++ class | `AAZ_PawnMoverHeroCharacter` | `AAZ_PawnMoverVehicleCar` / `Motorbike` / `Helicopter` |
| Mover mode set | SmoothWalking + Falling + Flying | WheelGround + Airborne + Drifting (car/bike); Lift + Hover + Forward + AutoRotation (heli) |
| AnimInstance | `UAZ_PawnMoverAnimInstance` (chooser+PoseSearch hero ABP) | None on chassis (wheels via AnimDynamics, rotors via tick-driven bone rotation); driver pose runs on the *hero* AnimInstance with `State.InVehicle.Driver.*` layer tags |
| Trajectory predictor | `UMoverTrajectoryPredictor` | Not present — vehicles don't motion-match |
| Camera | 3p spring-arm + camera | Chase/hood/bumper (car); cockpit + chase (heli); each pawn owns its own camera subobjects |
| ASC | Player ASC on PlayerState (cross-pawn) | Vehicle ASC on the vehicle pawn (subsystem damage attributes) — **two ASCs, separate concerns** |

**Driver pose rule:** the hero in a driver seat is still animated by the *hero* AnimInstance. New chooser routes (`State.InVehicle.Driver.Car`, `.Motorbike`, `.Helicopter`) pick in-seat anims via the same hierarchical CHT_v2 system. No separate "driver AnimInstance per vehicle type."

## Possession & seat system

**Open decision — pick before first vehicle:**
- **(a) Seat-as-component** on the vehicle pawn: vehicle is one pawn, seats are `USeatComponent`s, PC stays possessing the vehicle pawn, "current driver" is a pointer
- **(b) Seat-as-pawn:** each seat is its own pawn; PC unpossesses hero → possesses seat pawn; chassis is a static actor

**Recommended: (a) seat-as-component.** Clean Mover/networking — vehicle owns physics + authority, seat just resolves "who's driving / in this slot." Driver seat has input authority via PC possession of the vehicle pawn; passenger seats are mesh-attach + camera-only.

**Enter/exit flow:** parameterized `GA_EnterVehicle` on hero → unpossess hero → possess vehicle → attach hero mesh to seat socket → push vehicle IMC. `GA_ExitVehicle` reverses. **One GA per direction** (not `GA_EnterCar` / `GA_EnterBike` — parameterize like the rest of v2's ability discipline).

## ASC layering across pawns

- **Player ASC on PlayerState** (current AZ pattern, keep) — owns player attributes (health, stamina, inventory tags); **persists across pawn-switch**
- **Vehicle ASC on each vehicle pawn** — owns vehicle subsystem health (engine, wheels FR/FL/RR/RL, fuel, rotor blades), grants vehicle abilities (Honk, Headlights, Boost) only while occupied
- **Damage routing:** vehicle hits → vehicle ASC; driver damage from crash propagates via cross-ASC `GE_CrashDamage` → applies to player ASC's HealthAttribute through PC pointer

**Why two ASCs:** a destroyed car shouldn't kill the player. Vehicle subsystem state (disabled engine, blown tire) is vehicle-scoped — survives across drivers; player health is player-scoped — survives across vehicles.

## Tag taxonomy — cross-pawn additions

Extends the v2 tag taxonomy in `project_v2_architecture.md`:

```
Pawn.Type.Hero
Pawn.Type.Vehicle.Car
Pawn.Type.Vehicle.Motorbike
Pawn.Type.Vehicle.Helicopter

State.Possession.OnFoot
State.Possession.InVehicle.Driver
State.Possession.InVehicle.Passenger

State.InVehicle.Driver.Car           (drives chooser layer for driver pose)
State.InVehicle.Driver.Motorbike
State.InVehicle.Driver.Helicopter

Vehicle.System.Damaged.Engine
Vehicle.System.Damaged.Wheel.FR / FL / RR / RL
Vehicle.System.Damaged.Rotor.Main / Tail
Vehicle.System.Disabled.<subsystem>   (when subsystem health < threshold)
```

## InputConfig layering

- **BaseInputConfig DA** — always active: pause, screenshot, photo mode, scoreboard, menu nav (cross-pawn)
- **Per-pawn IMC** — owned by the pawn class; PC pushes on `OnPossess`, pops on `OnUnPossess`
- Both layers stack in EnhancedInput's local-player subsystem; PC controls the stack

## Network authority — vehicles use Mover too

- Vehicles are **Mover pawns**, not classic `bReplicateMovement`. Driver = autonomous proxy (client-predicted, server-corrected via NetworkPrediction); server validates damage + collision; passengers = simulated proxies (mesh-attach replication).
- This keeps one networking story across the project: Mover + NetworkPrediction for movement, ASC for tags & abilities, no per-pawn-class custom rep code.

## Camera architecture per pawn

- Each pawn class **owns its own camera subobjects** (spring arm + camera, or per-mode camera configs)
- Pawn declares a "default camera mode" tag (`Camera.Mode.ThirdPerson`, `Camera.Mode.Cockpit`, `Camera.Mode.ChaseCam`, etc.) that a camera mode system reads
- Avoid per-pawn ad-hoc camera code in BeginPlay — make it data-driven via the mode tag so vehicles can swap views (cockpit ↔ hood ↔ chase) without rewriting C++

**Lyra reference:** Lyra's `ULyraCameraMode` + `ULyraCameraComponent` is a good template — each pawn has a default mode, abilities can push/pop modes (aim-zoom pushes ADS, releases on aim-off).

## Folder layout — lock now, before file count grows

```
Source/AZ/Public/Pawn/Common/             — IInputContextProvider, ISeatOccupant, shared interfaces
Source/AZ/Public/Pawn/Hero/               — AAZ_PawnMoverHeroCharacter (move here from Character/ when first vehicle lands)
Source/AZ/Public/Pawn/Vehicle/            — AAZ_PawnMoverVehicleBase, Car, Motorbike, Helicopter (future)
Source/AZ/Public/Movement/Modes/Hero/     — SmoothWalking, Falling, Flying
Source/AZ/Public/Movement/Modes/Vehicle/  — WheelGround, Airborne, Drifting, Lift, Hover, Forward, AutoRotation
Source/AZ/Public/Seat/                    — USeatComponent, ESeatRole, seat replication
```

Existing files (`AZ_PawnMoverHeroCharacter` in `Character/`, modes in `Character/`) stay where they are until first vehicle work — moving them now is unnecessary churn. Lock the *target* layout so the move is a single PR when triggered.

## Asset / Content folder layout (mirrors source)

```
Content/AZ/Blueprints/Pawn/Hero/          — BP_AZ_Hero, BP_AZ_PawnMoverHeroCharacter
Content/AZ/Blueprints/Pawn/Vehicle/Car/   — vehicle BPs per subtype
Content/AZ/Blueprints/Pawn/Vehicle/Bike/
Content/AZ/Blueprints/Pawn/Vehicle/Heli/
Content/AZ/Input/IMC/                     — IMC_Hero, IMC_VehicleCar, IMC_VehicleBike, IMC_VehicleHeli, IMC_Menu, IMC_Cinematic
Content/AZ/Input/InputConfig/             — DA_InputConfig_Base (cross-pawn abilities), DA_InputConfig_Hero (hero abilities), DA_InputConfig_Vehicle (vehicle abilities)
```

## Cinematic / Sequencer routing

- Player cinematics: Sequencer takes over PC, applies `Mode.Cinematic.*` tags via player ASC, pawn rotation rule reads them (already in v2)
- Vehicle cinematics: same pattern — `Mode.Cinematic.Vehicle.*` tags on vehicle ASC drive camera mode + animation behavior
- Single-track cinematic decision: Sequencer always binds the PC (not pawn), the PC decides which ASC to tag based on current possession

## Things explicitly NOT decided yet (defer to first vehicle work)

- **Vehicle physics backend:** pure Mover modes vs ChaosVehicle interop. Decide when the first car is built. Pure Mover is simpler/consistent; Chaos gives free suspension/tire-friction realism.
- **Multi-passenger camera UX:** passenger free-look vs locked; can-shoot-from-seat; passenger HUD changes
- **Vehicle-class-specific damage cascades:** engine→wheel cascade behavior, rotor → autorotation transition logic
- **Vehicle networking edge cases:** prediction tolerance for high-speed collisions, drift state correction
- **Multi-pawn save/persistence:** which vehicle state persists across save points (ownership, damage state, fuel)

## CVar / promotion path (extends v2)

- Each new pawn class lands behind its own dev CVar (`AZ.Vehicle.Car.Enable`, `AZ.Vehicle.Heli.Enable`) — default false during development, on after soak test
- Same promotion discipline as v2 hero: validate end-to-end, soak in playtests, then default on
- Vehicle classes added incrementally — car first (easiest physics), then bike, then heli (hardest)
