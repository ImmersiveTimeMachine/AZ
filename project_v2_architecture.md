---
name: AZ v2 architecture — Chooser+PoseSearch animation × GAS gameplay × Mover physics (with co-op-ready foundation)
description: The v2 ("perfect-iteration") architecture for the AZ character system. Splits responsibilities between GAS (gameplay logic), Chooser+PoseSearch (animation selection), and Mover (physics/rotation). Tags bridge GAS ↔ chooser; trajectory bridges input ↔ PoseSearch. Includes parameterized-ability discipline, latency-sensitive event pattern, AI-parity virtual hooks, nested chooser hierarchy, and a co-op-ready network foundation. Replaces GASP-parity goal with a "best of both worlds" pairing.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
The v2 character system is built around one principle:

> **GAS owns gameplay logic. Chooser+PoseSearch own animation selection. Tags bridge them. Trajectory is a thin continuous data pipe that bypasses the gameplay layer.**

GASP-parity is no longer a goal — GASP was a learning vehicle. v2 keeps GASP's chooser+PoseSearch+BlendStack pipeline (best for animation) and pairs it with Lyra-style GAS+tags (best for gameplay logic). Closest production reference is **Lyra**, with a more controllable anim selector (chooser-as-filter) than Lyra's pure motion matching.

**Why:**

GASP gives high-quality animation (chooser narrows to a database, PoseSearch picks the best pose by trajectory match, BlendStack cross-fades, root motion drives capsule). But it has no gameplay-logic structure — sprint, aim, dodge, status effects, AI parity, replication, cancellation/cooldown rules all have to be hand-rolled.

GAS gives composable gameplay logic — abilities, tags, cancellation/cooldown/cost, replication-for-free, AI parity, status effects, designer tunability. But it has no animation-selection layer.

Used together, each gets the other's strengths without overreaching:
- AI activates GA_Sprint → tag set → chooser picks sprint DB → AI runs with player-quality animation
- GameplayEffect applies Status.Slowed → mover reduces target speed → trajectory shows slower movement → PoseSearch picks slower anims (zero anim-side code)
- Sequencer sets Mode.Cinematic.Strafe → mover locks rotation rule → chooser picks strafe-mode anims (one tag, both layers respond)
- Tags replicate via ASC + trajectory predicted client-side → identical pose selection on both ends, no per-anim rep code

**Production reference points:**
- **Lyra (Epic):** closest sibling. GAS + motion matching + BT for AI. v2 differs by adding a chooser layer over PoseSearch for designer-controllable anim filtering.
- **Uncharted / TLoU2 (Naughty Dog):** custom, no GAS. Rotation rule pattern (orient-to-movement free + strafe combat) and chooser-style state filtering are conceptually identical.
- **Fortnite:** GAS + custom anim schemas; no chooser+PoseSearch. Different anim path, similar gameplay layer.

**How to apply:**

**Layer responsibilities:**

| Layer | Owns | Reads from | Writes to |
|---|---|---|---|
| GAS abilities + tags | Discrete intent (sprint, aim, dodge, TIP, pivot) — start/end events with cancel/cooldown/cost; **bookkeeping for replication and AI replay** | Input, gameplay events | ASC tag container |
| Mover mode (`UAZ_SmoothWalkingMode_v2`) | Capsule physics, RM bridge, continuous rotation smoothing. **Virtual `ResolveRotationTarget()` so AI subclass overrides "target = camera" with "target = face-goal".** | Input vector, ASC tags (gating) | Mover state |
| AnimInstance C++ (`UAZ_MoverAnimInstance`) | Trajectory generation, IsMoving derivation, **SMState derivation**, chooser context builder. Class name updated 2026-05-25 (was `UAZ_AnimInstance_v2` / `UAZ_PawnMoverAnimInstance` — both removed; consolidated to one class). | Input vector, ASC tags, pawn state, Mover sync state | `FAZ_v2_ChooserContext` (single struct fed to chooser; see [project_local_plugin_patches.md](project_local_plugin_patches.md) for chooser MCP tooling) |
| ABP v2 (small) — `AZ_ABP_MoverAnimInstance.uasset` | Chooser eval, push to BlendStack. **No AnimGraph State Machine node** — see clarification below. | AnimInstance ChooserContext | BlendStack |
| CHT v2 (`CHT_v2_CharacterAnimations`) | Anim selection from (SMState, Stance, Gait, MovementDirection, OwnedTags). **Single flat table** (2026-05-25 decision, mirrors GASP's `CHT_MoverCharacterAnimations` with 372 rows in one file — replaces the earlier "hierarchical CHT_v2_*" plan; one table is simpler, scales fine, and matches GASP's proven shape). | `FAZ_v2_ChooserContext` | PoseSearch DB choice OR direct asset |
| PoseSearch | Best-pose-by-trajectory within picked DB | Trajectory + DB | Pose to BlendStack |

**SM in v2 = derived C++ enum, NOT an AnimGraph State Machine node (decision 2026-05-25):**

The "minimal SM" in the row above refers to `FAZ_v2_ChooserContext::SMState` — a `uint8` reusing the `EAZ_StateMachineState` enum (IdleLoop / TransitionToIdle / LocomotionLoop / TransitionToLocomotion / InAirLoop / TransitionToInAir / IdleBreak / TransitionToSlide / SlideLoop). Computed in `UAZ_MoverAnimInstance::NativeUpdateAnimation` from velocity + last-frame state + Mover events. Zero AnimGraph SM nodes, zero transition rules, zero entry/exit graphs.

The "SM transition fires immediately on threshold events" pattern (kept) now means: the AnimInstance flips the enum value in the same C++ tick as the threshold detection; the chooser sees the new SMState next evaluation; BlendStack cross-fades to the new row's anim. The GA fires in parallel as the bookkeeping layer (replication, cooldown, cancel).

**Why this matters (rationale):**
1. **Avoids v1 cruft** — v1's `AZ_ABP_Mover` SM node + transition rules + per-state graphs are exactly the complexity v2 was designed to escape. Putting an SM back in the AnimGraph re-creates it under a different name.
2. **Source of truth is C++** — `SMState` derivation is a pure function (`NativeUpdateAnimation`) — debuggable as one breakpoint, testable without the editor, AI-friendly (subclass overrides the derivation).
3. **One source for "what phase am I in"** — no risk of AnimGraph SM and C++ phase tracker disagreeing.
4. **Adding a new phase = enum value + derivation branch + chooser rows** — no AnimGraph SM editing.

GAS tags handle orthogonal state (Weapon.Slot.*, State.Aiming, State.Reloading) — `SMState` is purely the locomotion phase. The chooser picks based on the cross-product.

**Tag taxonomy (initial draft):**

```
Intent.Move.Direction.Forward/Back/Left/Right    — set by HeroPawn input handler (not PC) on input (no full ability needed)
Intent.Move.Speed.Walk/Run/Sprint                — set by GA_Sprint or analog stick threshold
Intent.Combat.Aim                                — set by GA_Aim
Intent.Combat.Fire                               — set by GA_Fire

State.Combat.Aiming                              — confirmed aiming (post activation latency)
State.Combat.Firing                              — currently in shot
State.Equipment.Armed/Holstered                  — weapon stowed state
State.Movement.Sprinting/Crouching               — confirmed state (passed gating)

Mode.Camera.Strafe / Mode.Camera.Free            — cinematic / scripted overrides
Status.Slowed / Status.Rooted / Status.NoSprint  — applied by GameplayEffect

Event.Rotation.ThresholdCrossed                  — fired by mover/anim accumulator → activates GA_TurnInPlace
Event.Movement.DirectionReversal                 — fired on movement direction flip → activates GA_Pivot
Event.Movement.LandHard                          — fired on landing above velocity threshold → activates GA_LandRoll
```

Intent vs State distinction: intent = "input wants this," state = "actually happening after gating" (e.g., aim might be requested but blocked by reload).

**Rotation rule (locked):**

- **Always-back-to-camera** target — character forward = camera forward, always (for the player; AI overrides via `ResolveRotationTarget()`)
- **Driver depends on state:**
  - **Idle:** threshold-driven (60° → stand_turn_90, 180° → stand_turn_180). NO smoothing below threshold (no creeping).
  - **Moving:** smoothed interp with per-state `FacingSmoothingTime`
  - **Start of movement:** start anim's root motion does the rotation; tap-and-release works via min-input-hold latch (~0.3s)
- **Per-state smoothing time defaults (validate in PIE):**
  - Aiming: ~0.05s
  - Combat (gun out, not aiming): ~0.15s
  - Exploration (unarmed walking): ~0.4s
  - Sprinting: ~0.6s
- **Discrete rotation events are GAS abilities** (parameterized — see below)
- **TIP trigger pattern:** mover/anim ticks accumulator → on threshold crossing, fire `Event.Rotation.ThresholdCrossed` event AND immediately commit SM transition. The ability handles bookkeeping; the SM plays the anim.
- **S backpedal:** always backpedal-facing-camera (consistent with the rule)

**Ability design — parameterize, don't proliferate (CRITICAL):**

The wrong path: GA_TurnInPlace_90_L, GA_TurnInPlace_90_R, GA_TurnInPlace_180_L, GA_TurnInPlace_180_R, GA_Pivot_L_90, GA_Pivot_R_90, GA_Dodge_Fwd, GA_Dodge_Back, GA_Dodge_L, GA_Dodge_R… Lyra has 100+ abilities partly because of this sprawl.

**The discipline:** ONE ability per *event type*, parameterized via EventData. The chooser and SM pick the actual anim from the parameters.

| Ability | EventData | Variants handled internally |
|---|---|---|
| `GA_TurnInPlace` | `{angle, dir}` | 90° / 180° / L / R |
| `GA_Pivot` | `{from_dir, to_dir}` | All direction reversals |
| `GA_Dodge` | `{dir, distance}` | Fwd / Back / L / R / diagonals |
| `GA_Slide` | `{dir, distance}` | Sprint-direction only |
| `GA_Vault` | `{trace_result, vault_height}` | Low / mid / high |
| `GA_Stop` | `{stop_foot_pref}` | L-foot / R-foot / auto-from-trajectory |

This cuts ability count by ~5x and makes adding a new variant a chooser-row change, not an asset creation.

**Latency-sensitive events — GAS for bookkeeping, SM for trigger:**

The wrong path: accumulator → SendGameplayEventToActor → GA activates → GA sets tag → SM sees tag → SM transitions → anim plays. That's 6-7 hops; debugging "why did TIP feel late?" is a nightmare.

**The pattern:** the SM transition fires *immediately* on the same frame as the gameplay event. The ability activates in parallel as the *bookkeeping layer* — it tracks "TIP is in flight," handles cancellation, applies cooldown, replicates state to remote clients, lets AI replay it without re-deriving the trigger.

```
Threshold crossed in C++ tick:
  ├─→ SendGameplayEventToActor(self, Event.Rotation.ThresholdCrossed, EventData)
  │     └─→ GA_TurnInPlace activates (bookkeeping: cooldown, rep, cancel rules)
  │
  └─→ AnimInstance state updated → SM transition fires same frame → anim plays
```

The two paths are independent. Anim doesn't wait for ability activation. Ability cancellation cancels the SM transition (via tag-block on the SM transition rule), but the *initial* play is direct.

This is what production GAS games actually do; Lyra's locomotion abilities are largely descriptive, not prescriptive of frame-1 anim selection.

**AI parity — Mover mode virtual + BT integration:**

The "AI uses same chooser+PoseSearch as player" claim only holds if the Mover mode's rotation rule is parameterized:

```cpp
// UAZ_SmoothWalkingMode_v2 (base)
virtual FRotator ResolveRotationTarget() const;  // defaults to camera-forward (player)

// UAZ_SmoothWalkingMode_AI (subclass)
virtual FRotator ResolveRotationTarget() const override;  // returns face-goal or velocity dir
```

Both share rotation interp, smoothing tables, RM bridge, tag gating. Only the rotation *target source* differs. AI subclass selected at pawn-spawn or via component class on the AI controller.

**BT integration:** AI behavior tree services set tags on the AI's ASC (`Intent.Move.Speed.Run` from a "MoveTo" task, `Intent.Combat.Aim` from an "EngageTarget" task). The chooser, PoseSearch, BlendStack pipeline runs identically — same anims as the player, no AI-specific anim path. Standard pattern; well-documented in Lyra's BT.

**Chooser hierarchy — embrace nested, don't flatten (CRITICAL):**

GASP's existing CHT_AZ_CharacterAnimations is hierarchical: 8 root rows × 3 cols × 52 nested sub-choosers (>100 effective rows). v2 will multiply this combinatorically: state × direction × speed × combat-state × weapon-type. The chooser table editor is unmanageable at 200+ rows in one table.

**v2 chooser file structure (set up at the start, not retrofitted):**

```
CHT_v2_Root.uasset                 — top-level routes by MovementState enum (Grounded/Airborne/Combat/Cinematic)
  ├─→ CHT_v2_Locomotion.uasset     — direction × speed × intent.combat tags
  ├─→ CHT_v2_Airborne.uasset       — jump phase × velocity
  ├─→ CHT_v2_Combat.uasset         — combat-state × weapon-stance × direction
  └─→ CHT_v2_Cinematic.uasset      — sequencer-driven; routed by Mode.Cinematic.* tags
```

Multiple files = better source control diffs, parallel editing by team, scoped iteration ("just touched Locomotion CHT — Airborne untouched"). Add per-state CHTs in the per-state implementation work.

**Co-op / multiplayer support (foundation laid now, full work deferred):**

The architecture is co-op-friendly *if* network model decisions are made up-front. Defer the implementation work, but commit to the model now to avoid retrofits.

**Network model:**
- **Player movement:** client-authoritative with server validation. UE 5.7 Mover plugin's prediction/correction infrastructure handles this — built-in.
- **AI:** server-authoritative. AI's ASC + Mover ticks server-side; relevant state replicates to clients.
- **Tags:** replicate via ASC (default GAS behavior). Both Intent.* and State.* tags propagate.
- **Trajectory:** computed locally per pawn from input (own player) or replicated velocity (remote players). PoseSearch is deterministic given the same trajectory → same anim picks.
- **Root motion:** Mover plugin's `FLayeredMove_RootMotionAttribute` is rep-aware. Anim runs on both ends; capsule motion derives from anim attribute consistently.

**Co-op design rules (commit to these now):**

1. **Inputs producing state changes go through GAS abilities or ASC tag mutations.** Direct pawn-to-pawn function calls don't replicate.
2. **No per-frame replicated state for movement.** Tags + Mover prediction handle it. Don't add custom rep state for "is currently turning" — read it from active abilities.
3. **Trajectory generator must be deterministic given input.** Any randomness in trajectory shaping (smoothing curves, etc.) must be seeded or removed. PoseSearch divergence = capsule divergence.
4. **AI never reads from player camera.** AI's `ResolveRotationTarget()` reads from BT blackboard or perception; player's reads from camera. Already handled by the virtual.
5. **Cinematic / cutscene tags applied via ASC, not direct pawn manipulation.** `Mode.Cinematic.Strafe` set by Sequencer track on the ASC → propagates to all observers.

**What's deferred until co-op work begins:**
- PIE multiplayer testing and rollback validation
- Remote player anim smoothing (PoseSearch on remote pawns may need deadzone for trajectory rep noise)
- Latency compensation for fast events (TIP commit on remote may visually jitter)
- Cheat detection / server validation tightening

**The single architectural commitment:** the v2 character system is built such that **enabling co-op is a configuration + testing exercise, not a refactor.** No "we'll add networking later" code that bypasses GAS or hardcodes camera reads on AI.

**C++ vs BP graph function split (lesson from the GASP port — over-ported):**

C++ owns:
- Trajectory generator (pure math, copy from current — well-validated)
- Per-frame state derivation: velocity, acceleration, IsMoving (= Trj_FutureVelocity + Accel), TIP accumulator
- Tag query helpers (cached ASC tag container ref + queries)
- Cross-system queries (Mover mode → AnimInstance state pull)
- Mover mode rotation interp + RM bridge + `ResolveRotationTarget()` virtual

BP graph functions in v2 ABP (move out of C++ — fires infrequently, BP nodes are first-class):
- Chooser evaluation calls (`Evaluate Chooser` BP node)
- BlendStack-from-chooser bridge (`SetBlendStackAnimFromChooser` and similar — `FAnimNodeReference` operations are BP-friendly)
- OnStateEntry / OnStateExit handlers
- AnimGraph node reference operations (Set Sample Sequence, Set Blend Profile)

**Thread-safety caveat:** per `feedback_animbp_post_event_vs_thread_safe.md`, BP graph functions called from anim worker thread must be thread-safe. Audit chooser eval BP node and `FAnimNodeReference` ops before committing functions to BP. If chooser eval isn't thread-safe-marked, that bridge stays in C++.

Principle: **C++ for hot-path math and state derivation; BP for SM-entry orchestration and AnimGraph node ops.** v2 AnimInstance lands at ~15-25 functions instead of v1's 63.

**Implementation pattern — "perfect each state then move on":**

Order: Idle → Start → Locomotion → Stop → Pivot → Backpedal → Sprint variants → Jump/Fall → AI parity pass → Co-op hookup pass

For each state:
1. Add chooser rows to relevant per-state CHT_v2_*
2. Add/refine intent tags in tag taxonomy
3. Refine SM transition rules in ABP_v2 (small — most states unchanged from v1 topology)
4. Validate in PIE under explicit acceptance test
5. Move to next state only when this one is correct

**v2 file scaffolding (planned, not yet built):**

C++:
- `Source/AZ/Public/Animation/AZ_AnimInstance_v2.h/.cpp` — new AnimInstance, thin (15-25 fns)
- `Source/AZ/Public/Movement/AZ_SmoothWalkingMode_v2.h/.cpp` — new Mover mode base (player rotation rule via `ResolveRotationTarget()`)
- `Source/AZ/Public/Movement/AZ_SmoothWalkingMode_AI.h/.cpp` — AI subclass overriding `ResolveRotationTarget()`
- `Source/AZ/Public/AbilitySystem/Abilities/GA_TurnInPlace.h/.cpp` — parameterized (bookkeeping only; SM plays the anim)
- `Source/AZ/Public/AbilitySystem/Abilities/GA_Pivot.h/.cpp` — parameterized
- `Source/AZ/Public/AbilitySystem/Abilities/GA_Dodge.h/.cpp` — parameterized
- (others follow same parameterized pattern)

Content:
- `Content/AZ/Blueprints/Animation/AZ_ABP_Mover_v2.uasset` — duplicated ABP; small SM, BP graph fns for orchestration
- `Content/AZ/Blueprints/Animation/Choosers/CHT_v2_Root.uasset` — top-level router
- `Content/AZ/Blueprints/Animation/Choosers/CHT_v2_Locomotion.uasset` — per-state
- `Content/AZ/Blueprints/Animation/Choosers/CHT_v2_Airborne.uasset` — per-state
- `Content/AZ/Blueprints/Animation/Choosers/CHT_v2_Combat.uasset` — per-state
- `Content/AZ/Blueprints/Pawn/BP_AZ_Hero_v2.uasset` — test pawn referencing v2 assets (A/B against v1)

**Open / deferred decisions:**

- 4-way vs 8-way direction tags — leaning 4-way for chooser column, full analog goes to trajectory only
- Move-direction tag setter location — confirmed: HeroPawn-side (PC stays unchanged, GAS InputConfig for ability-bound inputs only)
- Per-state smoothing values — lock by feel in PIE
- Min input hold for tap-and-release — start at 0.3s, tune by feel
- Whether to keep any SM in v2 ABP — leaning yes (Idle / Locomotion / Airborne) for clarity, but chooser could handle it alone
- Stop chooser parameter naming — `stop_foot_pref` vs `lead_foot` (cosmetic; pick during implementation)

**Promotion path — DO NOT delete v1 eagerly:**

When v2 is "validated end-to-end":
1. Add CVar `AZ.Chooser.UseV2` (default `false`) — runtime toggle
2. Repoint `BP_AZ_Hero` to v2 chooser + v2 ABP behind the CVar (set `true` for testing, `false` rolls back instantly)
3. Soak v2 in playtests for at least one milestone with the toggle available
4. Only AFTER v2 has been live without rollback events for ~weeks: delete v1 chooser, rename v2 → main, remove the CVar

This is a production-typical promotion path. The cost (one CVar, kept v1 assets) is low; the safety win (instant rollback if a state regresses) is high.

**What this architecture is NOT:**
- Not GASP-parity — explicit divergence in the gameplay-logic layer (GASP has no GAS)
- Not Lyra-clone — Lyra's anim system uses pure motion matching with different orchestration; v2's chooser-as-filter is more designer-controllable
- Not "everything is an ability" — continuous things (rotation smoothing, trajectory) stay in mover/anim, not wrapped as abilities
- Not "every rotation event is an SM state" — TIP / Pivot / Dodge become abilities + chooser branches, not new SM states
- Not "every variant is its own ability" — parameterize via EventData (see Ability design section)
- Not "the ability gates the anim" — SM transitions fire immediately on threshold; ability is bookkeeping
- Not "AI is a separate anim path" — AI uses the same chooser/PoseSearch with a Mover-mode subclass override
- Not "single CHT file" — hierarchical CHT_v2_* files from day 1
- Not "co-op as an afterthought" — design rules (deterministic trajectory, no per-frame rep state, GAS for state mutations) committed now

**Engine version — UE 5.8 (migrated 2026-05-10, ahead of original "wait for GA" plan):**

The original plan was Path C (stay on 5.7, migrate at 5.8 GA ~Aug 2026). That was abandoned: project migrated to UE 5.8.0 on 2026-05-10. Migration record with the full break catalog and engine patches lives in [project_ue58_migration_2026-05-10.md](project_ue58_migration_2026-05-10.md). The pre-migration prediction in [project_mover_5_7_to_5_8_diff.md](project_mover_5_7_to_5_8_diff.md) was accurate for the Mover signature changes; surprises beyond it (FSharedString in JSON, `LinkSequence`→`Link`, `FScriptMapHelper::FIterator` no-deref, `OverrideMotionMatchingBlendSettings` 2-arg) are catalogued in the migration record.

**What this means for v2 work:**
- Write directly against 5.8 signatures — the typedef-shim / forward-compat rule is **dropped**. New Mover-mode overrides take `const FMoverSimContext& SimContext` as a real parameter; no need for "ignore on 5.7" stubs.
- 5.8 additive features (`UBaseMovementMode::GetGameplayTags`, `FLayeredMoveBase::GetGameplayTags`, `FLayeredMoveGroup::ForEachActiveMoveOfType<T>`) can be used directly — no 5.7 fallback path needed. Use them where the architecture calls for them (tag publishing for chooser, layered-move iteration for ability bookkeeping).
- 5.8 multiplayer SM-desync fix and full networking are now available — co-op work doesn't need a separate engine bump when it begins.
- Engine-side dependency: local patch on `SmoothWalkingMode.h` (added `MinimalAPI` to `UCLASS`) — see [project_local_plugin_patches.md](project_local_plugin_patches.md) §3. Re-apply on engine resync.
- `AnimationToolsBundle` Marketplace plugin is **disabled** in `.uproject` pending a 5.8-compatible build (multiple unrelated 5.7→5.8 breaks in the plugin source — FSmartName deprecated, FApplicationMode tab-factory API removed). Re-enable when seller ships 5.8.

**Mover insulation discipline (unchanged):**
- Mover surface area in current code: ~17 symbols across 5 files (HeroPawn, AnimInstance, SmoothWalkingMode, FallingMode) — small and bounded.
- v2 keeps it bounded: `UAZ_SmoothWalkingMode_v2` + `UAZ_SmoothWalkingMode_AI` are the only Mover-coupled classes; `ResolveRotationTarget()` virtual is the seam.
- Don't let new v2 code grow the Mover surface — if we need a new Mover symbol, it gets one wrapper helper, not direct sprinkling.

**Reference branch:** `feature/rootmotion` — includes anim RM bridge `b347573`, CVar lifetime fix `071227a`, and the 5.8 migration edits done on 2026-05-10.

**Multi-pawn scaling — confirmed long-term direction (2026-05-11):**

The project will grow beyond one hero pawn class. Confirmed roadmap: **cars, motorbikes, helicopters** as additional possessable pawn classes. This forces architectural decisions that shape v2 *now*, not later:

- **Input architecture:** PC owns IMC stack + ability InputConfig (pawn-agnostic); pawn owns `SetupPlayerInputComponent` + cached input fields + IMC accessor. New vehicle = new pawn class + new IMC. Zero PC changes per vehicle.
- **Two-ASC model:** player ASC on PlayerState (cross-pawn, persistent); vehicle ASC on each vehicle pawn (subsystem damage, vehicle-only abilities).
- **Driver pose on hero AnimInstance:** chooser routes via `State.InVehicle.Driver.{Car,Motorbike,Helicopter}` tags. No separate AnimInstance per vehicle class.
- **Seat-as-component** (recommended) on the vehicle pawn — vehicle stays as the one possessed pawn; seats resolve "who's driving / passenger N."
- **Vehicles use Mover too** — same network model across the whole project (no `bReplicateMovement` for vehicles).

Full details + folder layout + deferred decisions in [project_multipawn_class_design.md](project_multipawn_class_design.md). v2 work proceeds on the hero pawn first, but anything that bakes in a "single pawn class" assumption now will cost a refactor later. The doc lists what to avoid.

**5.8 modernization follow-ups (captured 2026-05-25 while wiring `SetBlendStackAnimFromChooser`):**

Survey of the libraries we touch turned up four newer-than-v1 API paths that v2 should consider, but none are blocking the current Idle-first work. Captured here so they aren't forgotten.

1. **Remove `UMoverTrajectoryPredictor` component → use `UMoverComponent::GetPredictedTrajectory(FMoverPredictTrajectoryParams)` directly.**
   - v2 pawn currently has the predictor as a separate `UMoverTrajectoryPredictor` component in `AAZ_PawnMoverHeroCharacter:90`. UE 5.6+ added `GetPredictedTrajectory` on `UMoverComponent` itself.
   - **Trigger:** when we wire trajectory into `FAZ_v2_ChooserContext` (Step 6+ — motion-matched Locomotion). Replace the component + its accessor with a direct `MoverComp->GetPredictedTrajectory(params)` call inside `UAZ_MoverAnimInstance::NativeUpdateAnimation`.
   - **Win:** one less component on every pawn class; trajectory matches the same Mover sim tick used for movement.

2. **Direct-push `UBlendStackAnimNodeLibrary::BlendToWithSettings` (new 5.8 `Experimental`) → eliminate `FAZ_BlendStackInputs` member + inner-graph property bindings.**
   - Current path mirrors v1: `SetBlendStackAnimFromChooser` writes to `BlendStackInputs` struct, BlendStack inner SequencePlayer reads via property bindings (`BlendStackInputs.Anim`, `.bLoop`, `.StartTime`, `.BlendTime`, `.BlendProfile`, `.Tags`).
   - `BlendToWithSettings` takes all those settings as direct arguments + adds `EAlphaBlendOption` and `bInertialBlend` knobs we don't currently expose.
   - **Catch:** signature requires `FAnimUpdateContext` — callable from inside an AnimGraph linked function, NOT from `Blueprint Update Animation` EventGraph. Means restructuring the ABP to call the function inside an AnimGraph "Linked Anim Layer" or "Sample" thunk instead of EventGraph.
   - **Trigger:** when AnimGraph wiring feels heavy (BlendStack inner-graph property bindings are 6 separate pin bindings per node), or when we want inertial blending for hard-cut events (landings, weapon swap impact).
   - **Win:** removes the `FAZ_BlendStackInputs` struct entirely + the BlendStack-inner-graph step from the AnimBP setup. Trades the EventGraph call site for a linked-graph call site.

3. **`UPoseSearchLibrary::OverridePoseHistoryFromOwningMesh` (new 5.8 `Experimental`) → sub-AnimInstance pose history sharing for upper-body linked layers.**
   - Lets a sub-AnimInstance (e.g. an upper-body weapon overlay ABP) share the parent's PoseHistory node by name without re-collecting poses.
   - **Trigger:** Step 6+ when we add an Upper-Body Linked Anim Layer ABP for weapon-specific aim/reload/melee overlays. Both layers run motion matching against the same pose history.
   - **Win:** correct pose history reads in the layered ABP without duplicating the `PoseSearchHistoryCollector` node.

4. **Multi-role `MotionMatch` overloads (new 5.8 `Experimental`, lines 231-280 of `PoseSearchLibrary.h`) → defer until partner / synced moves land.**
   - Several signatures take `TArrayView<UE::PoseSearch::FRole>` + multiple PoseHistories for synced multi-character scenarios (partner animations, lift-and-carry, traversal helps).
   - **Trigger:** if/when CHALK design adds NPC-coordinated interactions (helping a downed teammate, breaching paired anim, traversal assist). Irrelevant for solo-hero locomotion.

**APIs we use today — status check (no changes needed for `SetBlendStackAnimFromChooser`):**

| API | 5.8 status |
|---|---|
| `UPoseSearchLibrary::MotionMatch(AnimInstance, ...)` (single role) | Current, `Experimental` flagged |
| `UPoseSearchLibrary::IsAnimationAssetLooping` | Current, `Experimental` |
| `UBlendStackAnimNodeLibrary::ConvertToBlendStackNode` | Stable |
| `UBlendStackAnimNodeLibrary::ForceBlendNextUpdate` | Stable |
| `GetBlendProfileByName` (engine) | Stable |
| `UMoverComponent::GetVelocity()` | Stable |
| `UMoverComponent::GetTrajectory()` | **Deprecated 5.5** — but we don't call it; `GetPredictedTrajectory` is the replacement |
| `UpdateMotionMatchingState` | Deprecated 5.7 — not called |
| `HasValidCachedState` / `HasValidCachedInputCmd` | Deprecated 5.6 — not called |
