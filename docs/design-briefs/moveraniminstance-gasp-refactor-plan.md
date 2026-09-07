# `UAZ_MoverAnimInstance` → GASP-shaped refactor — design plan

**Status: REV 2, 2026-09-07 — rewritten after an adversarial review returned FLAWED on rev 1.**
Design only, awaiting go. Rev 1's errors are listed in §9 rather than quietly deleted.

Sources read live (editor PID 35824, engine 5.8.3): GASP
`/Game/GameAnimationSample/Blueprints/SandboxCharacter_Mover_ABP`; our `AZ_ABP_MoverAnimInstance` and
`AZ_ABP_MoverHero_MHC`; `Source/AZ/{Public,Private}/Animation/AZ_MoverAnimInstance.*`,
`AZ_LocomotionStateMachine.*`, `AZ_LocomotionTypes.h`, `AZ_CmcAnimInstance.h`,
`Character/AZ_PawnMoverHeroCharacter.cpp`, `AbilitySystem/Abilities/AZ_GA_MeleeAttack.cpp`.

Line numbers in this document are **post-Shipping-fix** (that commit shifted everything below line 66
by +7).

---

## 0. Why our state machine is C++ and not an AnimGraph SM

Asked directly, and worth recording because it constrains everything below.

It was a dated decision (2026-05-25, `project_v2_architecture`), and the binding reason is in
`AZ_LocomotionStateMachine.h`:

> *"Pure C++ (NOT an AnimGraph State Machine node — that would run per-machine on the anim worker
> thread, the network non-determinism that caused the proxy-jump bug)."*

Recorded rationale: (1) v1's `AZ_ABP_Mover` already had SM node + transition rules + per-state graphs,
and v2 existed to escape that; (2) source of truth in C++ — one breakpoint, testable headless,
AI-subclassable; (3) one owner for "what phase am I in"; (4) a new phase is an enum value + a branch +
chooser rows, not AnimGraph surgery.

**Crucially, GASP's SM does the same job ours does.** Its "State Controller" is *purely logical* — it
outputs no pose and only drives the BlendStack via `OnStateEntry_*`. We did not diverge architecturally;
we implemented the same state controller in C++. Re-adopting theirs would be a re-implementation with a
known determinism cost and no functional gain. **This refactor does not touch that decision.**

The one place their shape genuinely differs is **the push**: GASP pushes on *state entry*
(`OnStateEntry_*`), we push *every tick and filter*. See F5 — it is the most interesting open question
this comparison produced, and it is explicitly **not** scheduled here.

---

## 1. What GASP 5.8 actually does (verified)

### 1.1 EventGraph

```
BlueprintInitializeAnimation → DelayUntilNextTick → InitializeMoverPredictor()

BlueprintUpdateAnimation → Branch(HasOwningActor AND HasMover)
    → Update_CVarDrivenVariables()
    → Update_PropertiesFromCharacter()
    → Branch(UseThreadSafeUpdateAnimation)  false → Update_Logic()
    → Update_InteractionConstraints()

BlueprintPostEvaluateAnimation → Set ForceFootPlacementReset=false → Branch(HasOwningActor) → DebugDraws()

[collapsed] "SM Transition Debug Events": 14 × AnimNotify_Transition:* → Add to String History Array (max 3)
```

CDO facts, both verified and both cutting against the obvious assumption:

- **`UseThreadSafeUpdateAnimation` = `False`.** GASP ships on the game thread. Parity is **not** an
  argument for going thread-safe. Out of scope.
- **`LocomotionSetup` = `1`** — the SM + Chooser + BlendStack path. Our v2 doctrine is Epic's shipped
  default for this character.

### 1.2 `Update_Logic` — five calls, fixed order

`Update_Trajectory → Update_EssentialValues → Update_States → Update_AimOffset → Update_AdditiveLean`

Trajectory first: their `IsMoving`, direction and turn angle all read predicted samples.

### 1.3 The seam — and its real limits

`Update_PropertiesFromCharacter` (6 nodes) sends two BPI messages into two struct variables:
`Get_PropertiesForAnimation → CharacterProperties`, `Get_PropertiesForRagdoll → RagdollProperties`.

**Correction to rev 1, which overstated this.** GASP's ABP is *not* free of engine reaching:

- `InitializeMoverPredictor` does `TryGetPawnOwner → GetComponentByClass` and sets `Mover` / `HasMover` /
  `NetworkRole`, then constructs the predictor object.
- `Update_InteractionConstraints` sends a **third** BPI message, `Get_MMIResult`.
- `Update_EssentialValues` calls `Get Offset Root Transform` (gated on `OffsetRootBoneEnabled`) and
  `Get Delta Seconds` — so their `CharacterTransform` depends on the very OffsetRootBone node we have
  neutralised.

The honest statement is: **one struct carries per-frame character state; setup and a few node/engine
reads stay live.** That is still a large improvement on ours, but "one seam, zero engine reaching" was
wrong and the plan no longer claims it.

### 1.4 `Update_States` — one macro, six instances

`ExecutionSequence[6]` → six `Update State Values` macro instances over MovementMode, RotationMode,
MovementState, Gait, Stance, MovementDirection. Pins: `NewState`, `CurrentState`, `LastFrameState`,
`RecentState`, `TimeInState`, `LastStateTime`, `RecentTimeLimit` (**0.2** MovementMode, **0.1** the other
five). `MovementState`'s `NewState` = `Select(IsMoving)`.

---

## 2. What we have (measured, corrected)

### 2.1 Our ABP EventGraph — 5 real nodes

`BlueprintUpdateAnimation → EvaluateChooser2(CHT_v2_CharacterAnimations) → SetBlendStackAnimFromChooser`

Both the chooser eval and the push run in `BlueprintUpdateAnimation` — game thread, and **after**
`NativeUpdateAnimation` in the same frame (`AnimInstance.cpp:792/797`).

### 2.2 `AZ_MoverAnimInstance.cpp` — 1986 lines

| item | value | note |
|---|---:|---|
| `NativeUpdateAnimation` | **881** lines (191–1071) | ~14 interleaved concerns |
| ⤷ its `#if !UE_BUILD_SHIPPING` block | 276 (650–925) | |
| ⤷ ⤷ dead `bCrouchDiagnostics` branch inside it | **155** (769–923) | rev 1 said "~200" |
| `SetBlendStackAnimFromChooser` | **739** lines (1193–1931) | rev 1 said 757; `:1933-1949` is a comment banner |
| ⤷ its six `#if` blocks | 192 | |
| `UpdateAnimation_Cmc` | 76 (1073–1148) | **dead on current content** — see F4 |
| debug total | **468**, ≈ 24 % | rev 1's 494 wrongly counted the two load-bearing maps |

`bDebugTrajectory` CDO default is **`True` on both Mover ABPs** — the debug path is ON by default, not an
opt-in. That matters for every "compare the logs" check below.

### 2.3 Coupling

Direct reads into `AAZ_PawnMoverHeroCharacter`, `UMoverComponent`, `UCharacterMoverComponent`,
`UAZ_PawnMoverComponent`, `UAZ_PawnMovementMode_RMAction`, `AController`,
`UAZ_Inv_CommonUI_EquipmentComponent`, `UAZ_ObstacleSensorComponent`, `UAbilitySystemComponent`,
`UMoverTrajectoryPredictor`, `AAZ_CmcCharacterBase`, `UCharacterMovementComponent`, plus the grabber
`AActor` and its `USkeletalMeshComponent`. Rev 1 said "eight"; it is more.

**And the coupling is bidirectional** (rev 1 missed this entirely — see F5).

---

## 3. Gaps

### G1 — No pawn→anim seam
One struct + provider interface, filled by the pawn. Note §1.3: some reads **must stay live** —
`GetCurveValue` (contact curves), `GetCurrentActiveMontage`, the trajectory predictor object, and the
grabber's `USkeletalMeshComponent` (needed by `ResolveGrabIKTarget` for
`GetClosestPointOnPhysicsAsset`). The seam is therefore *"one struct for character state"*, not
*"one seam for everything"*.

### G2 — The update is a monolith
881 lines, implicit ordering. This is the core of what the user asked for.

### G3 — Side effects inside the anim update
`QueueLayeredMove`, `CancelFeaturesWithTag` (three sites), an `FCoreDelegates::OnEndFrame` lambda that is
**live whenever `bDebugTrajectory`** (i.e. always, per §2.2) and whose handle is never removed.

### G4 — State history — **REVISED, rev 1 had this backwards**
Rev 1 claimed trackers would replace the push cache and make P0-2/P0-3 impossible. **That is wrong and
would have re-introduced both bugs.**

`LastPushed*` are **committed-push** caches, written only at `:1796-1806` after every abort path has
returned (`:1291`, `:1319`, `:1342`, `:1603-1608`, `:1633`, `:1640`, `:1706`). A tracker's
`bChangedThisFrame` compares against the last **frame**. On any aborted push the tracker reads
"unchanged" next frame and never retries — that *is* P0-2. Likewise the serial
(`:1316-1322`, `.h:484-490`) exists because raw state-equality wedged transitions — that *is* P0-3.

**Corrected position: trackers are purely additive.** They buy `_Recent` / `_LastStateTime` as
authorable chooser columns. The commit cache and the serial stay exactly as they are. The rev-1
"run both gates in parallel, disagreement is the finding" test is deleted — it would have disagreed by
construction on every abort, which is a known semantic difference, not a finding.

### G5 — Debug inline
Two dead pieces confirmed: the `bCrouchDiagnostics` branch (155 lines, `constexpr false`), and the
`TransformUpdated` hook inside it — which is never even *installed*, so its `GMeshMoveDumps >= 0`
early-return is a second, redundant reason it can't fire.

---

## 4. Target shape — GASP's names, our logic

### 4.1 Where orchestration lives

Recommendation: **C++**. `NativeUpdateAnimation` calls the named methods. GASP is pure BP and had no
choice; we get compile-checked ordering instead of a wire that can be lost, and the one pair that must
stay in the graph (`EvaluateChooser2 → SetBlendStackAnimFromChooser`) stays there regardless. Same names,
same order, readable side by side. (If you want it literally visible in the EventGraph, each stage
becomes `BlueprintCallable` — a small delta from this plan.)

### 4.2 The name map — with the ordering contract rev 1 got wrong

| GASP | Ours | Takes over |
|---|---|---|
| `Update_PropertiesFromCharacter` | same | tags / equipment / controller snapshot + the `Cached_*->` character-state reads → one struct |
| `Update_Trajectory` | same | 345–365 |
| `Update_EssentialValues` | same | velocity, speed, intent `bIsMoving`, MovementMode, **the hybrid-jump flag cache** (`bHybridJumpActive`, `bRMActionIsJumpRise`), stance, gait, 4-/8-way direction, turn angle, planted foot, `RotationOffset` |
| `Update_States` | same | reaction latch, `StateMachine->Tick`, air foot/intent latch, + the new trackers |
| `Update_AimOffset` | same | aim offset, relaxed pose, combat-ready alpha |
| `Update_AdditiveLean` | same | 616–649 |
| `Update_InteractionConstraints` | same | grab hand-IK + shake |
| *(GASP has none)* | `Update_MovementRequests` | RM queue, the RMAction-exit cancel + `LastRawMoverModeName` write, transition teardown |
| `DebugDraws` | same | the debug blocks, on `PostEvaluateAnimation` |

**Ordering contract — three compile-clean traps rev 1 walked into:**

1. `bHybridJumpActive` / `bRMActionIsJumpRise` (`:407-424`) are **inputs to the SM** (`:983`,
   `SMIn.bHoldTakeoffPhase`). They belong in `Update_EssentialValues`, **before** `Update_States`. Rev 1
   put them in `Update_MovementRequests`, which runs after — a stale/uninitialised takeoff hold. A frame
   context does not catch this, because `bHybridJumpActive` is a member.
2. `LastRawMoverModeName = ModeName` (`:438`) **must follow** the compare at `:434`. Split naively across
   two stages, the apex `RMAction→Falling` cancel silently never fires and the rise's move clip-drives
   the capsule on the ground until `DurationMs` expires (the P1-15 class).
3. Today the RM **queue** (`:314-341`) precedes the **teardown** (`:1058-1062`). Inside
   `Update_MovementRequests` the order must stay **queue → jump-cancel → teardown**; reversed, an
   abandoned transition's move is queued after the cancel and survives.

All three compile cleanly and fail silently. They are the reason step 2 is split in §6.

### 4.3 Selection — GASP has names for this too

`Update_LocomotionTransition` (push gate + one-shot locks), `Update_MotionMatching` (candidates +
search), `Update_MotionMatching_PostSelection` (committed-push bookkeeping).

---

## 5. Findings

### F1 — `Update_AdditiveLean` reads a one-frame-stale `SMState` — **real, but cosmetically invisible**
The lean gate reads `ChooserContext.SMState` at `:638`; the SM writes it at `:1006`. True.

**Rev 1 overstated the effect.** `LeanAlpha` has **0 bindings in either ABP** (`LeanAmount` has 2), and
`:644` says so: the graph's additive alpha is `Abs(LeanAmount.X)`. With `Vector2DInterpTo` speed 10 at
60 Hz the whole ramp shifts by one frame ≈ **16.7 ms**, and on a straight stop `Lat ≈ 0` anyway. Not
visible. Fix it because the ordering is wrong, not because it looks wrong — and prove it with a
per-frame `LeanAmount.X` log across the edge, not "a look".

### F2 — `IsMoving()` collision — **binding is live, but it does not gate anything**
`UAZ_MoverAnimInstance::IsMoving()` exists (`.h:323`, `.cpp:1979`), returns `false`, and the binding
**is live** in `AZ_ABP_MoverHero_MHC` (`bClampToTranslationVelocity = IsMoving`, plus the four
`Get_OffsetRoot*`). I previously told the user these had been stripped — that was true *before* the
getters were built; recompiling resolved them. The node is present at HEAD as of `ae1df0c`, so the
header comment claiming "in no committed graph" (`.h:276-277`, `.cpp:1935`) is now **stale and should be
corrected**.

**But the ordering conclusion was wrong.** Nothing forces us to reuse the identifier — every stage in
§4.2 uses `ChooserContext.bIsMoving`. So the OFR cleanup is **independent**, not a prerequisite. Do it
whenever; just commit or revert the modified `.uasset` first so the starting state is unambiguous.

### F3 — `GetLastInputCmd()` read twice per frame (`:379`, `:455`). Vanishes with the seam.

### F4 — Step 4's CMC payoff does not exist — **verified**
`BP_CMC_Hero`'s mesh uses `AZ_ABP_CmcAnimInstance_C` (parent `UAZ_CmcAnimInstance`). The only ABPs
deriving from `UAZ_MoverAnimInstance` are `AZ_ABP_MoverAnimInstance` and `AZ_ABP_MoverHero_MHC`.
**No CMC pawn uses this class**, so `UpdateAnimation_Cmc` (`:1073-1148`) is dead code. Rev 1's "the CMC
pawn gains the full pipeline for free, testable on `spike/cmc-backport`" was false. Step 4 is Mover-only.

### F5 — The coupling is bidirectional — **rev 1 missed this entirely**
- `AZ_PawnMoverHeroCharacter.cpp:823` — `ProduceInput` reads `AnimInst->IsPlayingImpactReaction()` to
  lock movement during a flinch.
- `AZ_GA_MeleeAttack.cpp:199` — reads `HeroAnim->ChooserContext.bIsMoving`, which after `:1043` is the
  **air-latched** value, not the raw one.

Both are outputs the refactor must hold stable. Neither is mentioned in rev 1's failure axes.

### F6 — The one-frame RM handoff is settled by construction, no log needed
`NativeUpdateAnimation` then `BlueprintUpdateAnimation`, both game thread
(`AnimInstance.cpp:792/797`); both ABPs push from `BlueprintUpdateAnimation`. The flag set at `:1914` is
therefore consumed at `:314` **next frame**. Corollary: the "two halves because Mover may only be
written from the game thread" rationale (`:306-312`) is **stale** — the push already runs on the game
thread. Whether removing the frame is an *improvement* still needs measuring (see risk 5).

### F7 — Shipping build was broken — **FIXED, committed separately**
`GLastPushSMStateByInstance` / `GPushCountByInstance` were declared under `#if !UE_BUILD_SHIPPING` but
used unguarded on the committed-push path. Development was unaffected, which is why it went unnoticed.
Both moved outside the guard; a mechanical audit now shows 0 guarded symbols with unguarded uses.

---

## 6. Sequencing (revised)

**Step 0 — Baseline.** One PIE pass of the standard loop with the debug HUD (already on by default).
Pass-fail signal is a **normalised sequence diff** of `[v2 Pick]` / `[v2 Snap]` / `[v2 MMPool]` — strip
frame counters, world time and timings first. Two PIE runs are never byte-identical; "byte-identical" is
not a usable criterion and rev 1 was wrong to use it.

**Step 1 — Debug extraction.** Scope: the `NativeUpdateAnimation` block only. Delete the 155-line dead
`bCrouchDiagnostics` branch and the never-installed hook. **Explicitly out of scope:** the six `#if`
blocks inside `SetBlendStackAnimFromChooser` — `[v2 Play]` (`:1231-1287`) runs *before* three early
returns, so moving it to a tail `UpdateDebug()` would silently change which frames it reports. Leave it.
*Expected size:* ≈ 1986 → ≈ 1830 (rev 1's "→1500" implied deleting ~480 lines while calling it a move).

**Step 2a — Extract stages, keep current order.** Pure move into §4.2's names, honouring the three
ordering contracts. *Prediction:* normalised diff identical. One variable.

**Step 2b — Swap to GASP's order.** The single change is `Update_States` before `Update_AdditiveLean`
(F1). *Prediction, stated first:* `LeanAmount.X` at the loop→stop edge shifts by exactly one frame and
nothing else changes. One variable.

> **Concrete work queued against 2a and 3:** [additive-lean-rework.md](additive-lean-rework.md) — port the CMC acceleration model into
> `Update_AdditiveLean`, restore the never-written `LeanAmount.Y`, and add `MovementState` /
> `bEnableAO` for the AdditiveLeans layer's unbound gates. Six defects, with a falsifiable check.

**Step 3 — Trackers, additive only.** Add `TAZ_StateTracker<T>` for the six enums. **Do not touch the
commit cache or the serial.** *Prediction:* push count unchanged; new fields unused by any chooser column
yet.

**Step 4 — The seam, Mover-only.** Struct + provider on `AAZ_PawnMoverHeroCharacter`; the live reads of
§G1 stay live and the struct is not POD. Decide separately whether `UpdateAnimation_Cmc` is deleted as
dead code or left untouched — **no CMC behaviour claim attaches to this step.**

**Step 5 — merged into step 2a.** Rev 1 scheduled `Update_MovementRequests` extraction in step 2 *and*
the side-effect move in step 5, which would re-derive the same queue/cancel/teardown ordering twice.
One place, one time.

**Step 6 — Selection split.** Last.

**OFR cleanup — independent** (F2). Not a prerequisite. Commit or revert `AZ_ABP_MoverHero_MHC.uasset`
first, and fix the two stale "no committed graph" comments.

**Build protocol, every step:** each adds members or types, which Live Coding cannot patch (this is why
the file uses instance-keyed file-statics at all). Closed-editor CLI build before each PIE check; both
ABPs recompiled and saved.

---

## 7. Failure axes

| # | Failure | Shows as | Guard |
|---|---|---|---|
| 1 | SM input placed after the SM (§4.2 trap 1) | takeoff hold stale → early InAirLoop, low jump | contract in §4.2; `bHoldTakeoffPhase` logged in step 2a |
| 2 | Edge-latch split (`LastRawMoverModeName`, trap 2) | apex cancel never fires; rise clip-drives on ground | keep compare+write adjacent; assert one writer |
| 3 | Queue/teardown reorder (trap 3) | abandoned transition keeps driving | fixed order stated as a contract |
| 4 | Tracker replaces the commit cache | P0-2 / P0-3 return: wedged transition, "goes straight to stop" | G4 revised — trackers additive only |
| 5 | Step 5 latency sign unknown | consuming in the pawn tick could *add* a frame if the pawn ticks after the Mover component | measure tick order before claiming an improvement |
| 6 | Bidirectional outputs break (F5) | flinch no longer locks movement; melee mis-reads moving | pin `IsPlayingImpactReaction()` and the air-latched `bIsMoving` in the step-0 baseline |
| 7 | Seam under-gathers | compiles clean, foot/montage selection breaks | grep every `Cached_*->`; each is in the struct or deliberately live |
| 8 | Debug moved past evaluation | curve/montage values sampled at a different point; skipped on non-eval frames | step 1 scope excludes the selection blocks |

---

## 8. Explicitly not copied

Dual `LocomotionSetup` path; OffsetRootBone; GASP's AimOffset/AdditiveLean internals; thread-safe update
(their CDO is `False`); GASP's AnimGraph SM (§0); entry-driven push (interesting, unscheduled — an entry
can still abort, so it does not obviously remove the cache).

---

## 9. Rev 1 errors, for the record

1. G4 inverted — trackers cannot replace a committed-push cache; would have restored P0-2/P0-3.
2. Step 4's CMC payoff asserted without checking which anim class `BP_CMC_Hero` uses (F4).
3. `Update_MovementRequests` placed after `Update_States` while holding two of its inputs (§4.2 trap 1).
4. "One seam / zero engine reaching / knows nothing about Mover" — overstated (§1.3).
5. F1's visible effect asserted without checking that `LeanAlpha` is unbound.
6. F2's ordering conclusion drawn from a live binding that gates nothing we need.
7. "Byte-identical logs" as a pass criterion — unachievable, and unfalsifiable as written.
8. Numbers: 757→739, ~200→155, 494/25 %→468/24 %, "eight external types"→more, node counts in §1.
9. Bidirectional coupling (F5) and the Live Coding build protocol both absent.

---

## 10. Open questions

1. **Branch** — we are on `spike/cmc-backport`; memory's ★★★ note says the live line is
   `feature/mover-metahuman`. With the CMC payoff gone (F4) there is no longer an argument for the spike
   branch.
2. **Scope** — steps 1, 2a, 2b are cheap and low-risk and deliver the structure that was asked for.
   Step 4 is the architectural one and now has a smaller payoff than rev 1 claimed. Land 1–2b and
   re-decide?
3. **`UpdateAnimation_Cmc`** — delete as dead code, or leave it?
