---
name: project_perf_baseline_2026-10-09
description: "★★★ First real profile (2026-10-09): standalone L_001, hero + 7 Chalkies, 1080p. GPU-bound ~9-10 ms; Mover+NetworkPrediction 0.43 ms (NP's own share 0.15); felt stutter = GC / runtime Niagara compile / PSO compile, NOT movement or anim. Tool Tools/perf/az_perf_capture.py."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-09T20:22:52.486Z
---

**Context.** Artur asked (2026-10-09) whether to rebuild the hero as fully single-player (strip all network code, maybe back to CMC) because "things feel tangled and performance drops". Agreed plan: MEASURE first, decide after. This is the measurement.

**Tool:** `python Tools/perf/az_perf_capture.py capture --label X --seconds 40` (hands-off) or `--manual` (play, close the window), `analyze <run>`. It runs UnrealEditor.exe -game (Development, uncooked) with `-trace=default,counters -statnamedevents`, waits for LoadMap, then exports headlessly with UnrealInsights (`-AutoQuit -NoUI -unattended -ExecOnAnalysisCompleteCmd="@=file.rsp"`). Runs in `Saved/Profiling/AZPerf/<stamp>_<label>/` (summary.txt, stats_*.csv, callees_game.csv, trace.utrace).
Traps:
- Insights drops backslashes in response-file paths → use forward slashes and absolute paths.
- Without `-unattended`, an "AudioModulationInsights failed to load" dialog blocks every export until someone clicks OK.
- STAT `FrameTime` is TWO scopes per frame → count frames by `FEngineLoop::Tick`.
- GPU timers come doubled (2 per frame) and appear in every thread export → export them alone with `-threads=NoCpuThread`.
- ExportTimingEvents does not output GPU events.

**Numbers (Development -game, editor open in background, 1920x1080 windowed):**
- idle (hero stands, 7 Chalkies without target): 10.2 ms avg / 98 fps.
- play (unarmed + W2 rifle + pistol, fire, reload, Chalkies chase / melee / grab, 132 s): 9.0 ms avg / 111 fps, p99 11.2 ms.
- **GPU-bound**: GPU ~9-10 ms; the game thread is busy ~4.7 ms (ReflexSleep 2.2 + waits). Main GPU costs:
  - Lumen screen probes ~1.6 ms;
  - TSR ~1.2 ms;
  - hero grooms ~1 ms GPU + ~0.35 ms hair ray-tracing geometry + CPU hair work on workers.
- Game thread costs:
  - Slate / HUD 1.37 ms: WBP_AZ_CompassModule paint 0.73, of which WorldMarkers BP tick 0.43;
  - anim on the game thread 1.0 ms (hero ABP 0.42, face 0.26, 7 Chalkies 0.28);
  - workers: anim eval 2.2 ms, RigLogic 0.43 (2 evaluations per frame), foot Control Rig 0.29;
  - **Mover + NetworkPrediction for all 8 pawns 0.43 ms; NP's own bookkeeping (IndependentTick exclusive) 0.15 ms.** The rest is real movement work (sweeps, floor checks) that CMC would also do.
- **Felt stutters (8 in 132 s, 50-190 ms):**
  - GC 44-57 ms, of which ~10 ms is `PyUtil::CollectGarbage` (Python plugin hook, editor binary only).
  - `UNiagaraSystem::RequestCompile` on first activation: 36 ms for the firearm shot (`BP_AZ_GA_FirearmFire.Multicast_PlayFirearmShot`), 67 ms for `BP_AZ_TestDestruction_Glass` (chaos break FX).
  - Runtime PSO compile 190 ms (`PSOPrecache: Unknown`, translucency of the glass).
  - Two GPU stalls of 100 / 52 ms with no pass attributed (likely driver-side first-use compile).
- Window move between monitors + DLSS Frame Generation (Streamline) swapchain re-create = 1-3 s hitches. Artifacts, not the game.
- Also: hero meshes have URO off and VisibilityBasedAnimTickOption=AlwaysTickPoseAndRefreshBones everywhere (Chalkies too). `az.Cam.Debug` is looked up via FindConsoleObject every frame (engine warning).

**Verdict given to Artur:** network code / Mover is not the performance problem (<2 % of the frame). Movement rewrite or CMC return is not justified by perf. What is felt = first-use compiles + GC (partly dev-build artifacts). Steady-state headroom is on the GPU (Lumen / TSR / grooms) and the HUD compass. Anim simplification is for clarity, not FPS.
Next proposed:
1. Recompile + save the Niagara systems that compile at runtime.
2. Cache the az.Cam.Debug lookup.
3. Profile a cooked/packaged Development build for the real GC / PSO picture.
4. Then the clarity work: dead classes, chooser clean-up — see [[project_architecture_rationale]].

**DONE 2026-10-09 (step 1 only, per Artur):**
- Re-saved the 6 Niagara systems that gameplay references, found by an asset-registry BFS from /Game/AZ + L_001:
  - NS_AR_Muzzleflash_1_ONCE;
  - NS_Breaking_Glass / Concrete / Plaster / Wood;
  - NS_Grenade_Explosion.
- Every one logged "Compiling System … took" on save, so all were stale. Backup `AZ_Backups/2026-10-09_NiagaraResave_161016`. These are vendor packs, not tracked in git.
- Mechanism: PostLoad marks systems whose compile IDs are out of sync as `RequestPendingOnDemand`; in the editor and `-game` the compile then runs on the FIRST activation. `UNiagaraSystem::PreSave` → WaitForCompilationComplete, so a forced `save_loaded_asset(sys, False)` persists synced IDs. Python exposes no compile status.
- Cached the `az.Cam.Debug` lookup as a function-local static in `AZ_MoverAnimInstance_Procedural.cpp` and `AZ_PawnMoverHeroCharacter_Camera.cpp` (Live Coding OK). A new `-game` process loads the DLL from disk, so this reaches standalone only after the next full build.
- Not yet verified in a fresh capture.

**Katimavik classroom (`L_Katimavik_Classroom_PostApoc_v01`), 2026-10-09:**
- Artur sees ~40 fps in PIE. In standalone 1080p the median frame is 13.8 ms (~72 fps). PIE costs more: the editor viewport is bigger (1440p monitor) and the editor itself adds overhead.
- **GPU-bound, ~14 ms GPU** (median per frame; Insights reports GPU timers twice per frame):
  - lights + shadows ~5.3 ms ("UnbatchedLights"). The map has 13 MOVABLE shadow-casting lights: 4 "cool window" rect 256x256 r=950, 4 ceiling fill rect r=1000 (vol scatter 1.0), 3 spots, 2 directional (the "Moon standby" one is visible and casts shadows);
  - TSR 2-4 ms (Epic TSR settings: History.ScreenPercentage 200, Resurrection, ReprojectionField);
  - Lumen ~2 ms; hero grooms ~1.3 ms; sky / clouds / real-time sky capture ~1 ms.
- Proposed (art decision, awaiting go): hide the moon; no shadows on the 4 fill lights.
- Traps:
  - Git Bash mangles `/Game/...` args → `MSYS_NO_PATHCONV=1`.
  - The map had no saved PlayerStart → "FindPlayerStart: … NO PLAYERSTART" → spawn at WorldSettings (origin) → falls. Artur added `PlayerStart_1` (8362,-607,492.5) and saved.
  - Alt+Enter / fullscreen mid-capture = 2.7 s hitch (ProcessKeyDown → ApplyResolutionSettings).
- Anim log findings, NOT map-specific:
  - re-press during a stop picks Loop (SM=2), then a Start/Pivot (SM=3) on the very next frame;
  - holding W while blocked (spd=0) flaps Loop↔Stop;
  - `[CmcJump] land-complete event never arrived within 2.5s` — watchdog ended the jump ability.
  - No Snap / MMFallback.

The decision on "fully non-networked" is still open: Artur's wish, my advice = do it as a cleanliness step later (Mover standalone liaison), not a rewrite.
