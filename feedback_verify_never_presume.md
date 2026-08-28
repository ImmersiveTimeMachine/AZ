---
name: feedback-verify-never-presume
description: "★ USER RULE (2026-08-27, MUST): never presume — every factual claim must be backed by a fresh direct read of the AUTHORITATIVE source, named in the answer. Includes the authoritative-source table and the 7 presumptions that cost a full day."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-28T04:21:47.506Z
---

# MUST RULE: never presume — check, then state what you checked

**User, 2026-08-27:** *"dont presume alwise check, so make this as a must rule"*

Issued after a day in which every wrong turn came from asserting something plausible instead of reading
the authoritative source. Not a style preference — a hard gate on what may be said.

## The rule

1. **No claim without a fresh read.** Before stating any fact about code, assets, config, or build state,
   read the authoritative source **in this session**. Memory files, earlier turns, and subagent reports are
   leads, not evidence — memory is a point-in-time snapshot and may be stale.
2. **Name the source.** Say what was read (`file:line`, property, log line, export). A claim with no cited
   source is a presumption and must not be written.
3. **Verify a change LANDED, don't infer it from the call returning success.** Applying ≠ effective.
4. **Say "not verified" out loud** when something could not be checked. Never fill the gap with a plausible
   value.
5. **Relaying a subagent or memory claim = presuming**, unless independently re-read. Two audits agreeing
   is still not a read.

## ★ Authoritative source table (this project)

| Claim about | Authoritative source | NOT authoritative |
|---|---|---|
| AnimGraph pin value / links | `ue_export_blueprint_nodes` T3D (`DefaultValue=`, `LinkedTo=`) | the node's serialized struct via Python — the compiler overwrites it with the PIN default (`AnimBlueprintCompiler.cpp:465-492`) |
| Which anim branch renders | compiled TwoWayBlend `Alpha` (pin) + `bAlwaysUpdateChildren` | the details-panel struct |
| Live Coding compile landed | `LiveCodingConsole.log` contains **both** `Manual recompile triggered` AND `Patch creation for module …-AZ.dll successful` | the console command returning success; `LiveCoding.Compile fired` in the editor log |
| CLI build succeeded | `Result: Succeeded` in the build output | exit code 0 (the wrapper exits 0 on failure) |
| New UFUNCTION/UPROPERTY is live | `hasattr(unreal.X, 'snake_name')` in the editor | that the file compiled |
| Asset property value | `get_editor_property` on the **BP CDO** | the C++ default (BP children serialize their own copy) |
| Anim notifies on a clip | `AnimationLibrary.get_animation_notify_events` (+ `ObjectIterator` cross-check) | `get_editor_property('notifies')` — protected, raises; a raise is NOT "no notifies" |
| Whether a code path ran | a log line emitted from inside that path | that the code exists and looks reachable |
| Movement/rotation rates | the CDO value read live | any remembered number |
| Why a decision was made | a log emitted **at the decision point**, in the same frame phase | a log printing the decision's *result* next to state sampled elsewhere in the frame — see presumption 8 |
| Dead C++ symbol | Rider `get_file_problems` ("never used in Blueprint or C++") + `safe_delete preview=true` | ripgrep — it cannot see Blueprint/asset usages of a reflected symbol |
| Behaviour of a C++ class that HAS a BP child | the C++ **and** the BP graph (`ue_export_blueprint_nodes`) **and** the BP CDO | the C++ alone — see the rule below |

## The 7 presumptions that cost the day (each was plausible, each was wrong)

1. `[CmcRatio] eff` reported GREEN → called locomotion "clean". `eff` divides by a play rate that was
   never applied; **`raw` was 1.6/0.4**. Measured a quantity, not the outcome.
2. Read Alpha `0.0` from the node struct → told the user twice the MM branch renders. The **pin** was
   `1.000000`; the spine had been rendering all along.
3. Assumed the spine's BlendStack was the one wired in. The **empty duplicate** was wired; the warped one
   was orphaned. Only a connectivity dump showed it.
4. Assumed missing `BlockTransition` explained the cutting. 39/56 clips already had it — falsified.
5. Assumed the pawn turned at 180 °/s (from a subagent). Measured `RotationRate.Yaw = **360**`.
6. Recommended lowering the stop play-rate floor. Stops already had their own `0.2` floor and the
   correction is **deliberately disabled** for stops — the advice was nonsense, caught only by reading
   `ComputeDynamicPlayRate`.
7. Reported changes as live after `LiveCoding.Compile` returned success. **No compile ever ran** — the
   console log had zero `Manual recompile triggered`. The user tested an unchanged binary twice.

8. **The instrument itself lied by one frame.** `[CmcSel]` prints from `Update_Logic` (step 1 of the frame),
   so it paired LAST frame's published selection with THIS frame's freshly recomputed SMState/accel. It
   showed the impossible `stop @ SM=LocomotionLoop accel=1.00`, and **three** hypotheses were built on it
   and died (chooser bypass, gate ordering, normalization set). A comment in our own code asserting the
   AnimGraph updates before `Update_Logic` was also false — the engine order is
   `NativeThreadSafeUpdateAnimation` (`AnimInstanceProxy.cpp:1350`) THEN `UpdateAnimationNode` (`:1395`).
   **Fix the instrument before theorising again**: log at the decision point, in that phase, or not at all.

## ★★ USER RULE (2026-08-28): if the C++ has a BP child, CHECK THE BP TOO

**User:** *"one more rule if c++ has BP check the BP also"*

**Why:** a Blueprint child is not a passive container of defaults — it can **override virtuals and negate
the C++ entirely**. Reading only the C++ and declaring the logic correct is a presumption.

**The incident that produced this rule.** "Space does nothing" on the CMC hero. Every C++/asset link was
audited and every one was correct: SpaceBar → `AZ_IA_RT_Jump` → `Input.Action.Jump` → `BP_AZ_GA_PawnJump`
(InputTag exact match) → granted in `StartupAbilities` → `CanActivateAbility` needs only
`IAZ_JumpRequester`, which the base implements → `JumpMaxCount=1`, `NavAgentProps.bCanJump=true`.
Five separate static checks, all green, and the bug was in none of them.

`BP_AZ_GA_PawnJump` **overrode `K2_ActivateAbility`** with:
`Cast To AZ_BP_PawnMoverHeroCharacter` → **Cast Failed** → `K2_EndAbility`.
On a CMC pawn the cast fails, the ability ends in the SAME frame, `EndAbility` calls
`SetJumpPressed(false)` → `StopJumping()` → `bPressedJump=false` before `CheckJumpInput` ever ran. The C++
did everything right and the Blueprint undid it microseconds later.

**What to check, every time:**
1. **Overridden events** — dump the graph (`mcp__rider__ue_export_blueprint_nodes`, graph `EventGraph`)
   and look for `bOverrideFunction=True`. A `K2_*` override REPLACES or wraps the native path.
2. **Concrete casts** — `Cast To AZ_BP_PawnMoverHeroCharacter` and friends. When C++ is made
   interface-based, the BP layer is where the old concrete cast survives (spike doctrine rule 3,
   "no fourth generation of casts"). **Grep every GA Blueprint for these before trusting a port.**
3. **CDO overrides** — `find_default_value_overrides`, or read the BP CDO. BP children serialize their
   own copy; C++ default changes do not propagate.

**Corollary:** "the C++ is correct" is never an answer to "why doesn't it work" when a BP child exists.

## Cheap habits that would have caught all seven

- Dump AnimGraph **connectivity** before theorising about an ABP (`list_function_nodes(abp,"AnimGraph")`).
- After any "fix", state the **number that must move** and re-measure it.
- Prefer the log line that proves the path executed over the reasoning that says it should.
- When two sources disagree, find the one the ENGINE reads at runtime and quote it.

Related: [[feedback_build_paging_file_parallelism]] (never kill LiveCodingConsole; verify both log lines),
[[feedback_seam_trace_before_pie]], [[project_cmc_mm_content_verdict]].
