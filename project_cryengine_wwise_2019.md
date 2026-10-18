---
name: project-cryengine-wwise-2019
description: "CryEngine AZ audio — Artur chose Wwise 2019.1 (the engine's own version) over SDL Mixer on 2026-10-17; plan, facts and what waits on him"
metadata:
  node_type: memory
  type: project
  originSessionId: a151daa0-c065-4c1d-bfd0-23373f1d30eb
  modified: 2026-10-18T05:32:46.105Z
---

On 2026-10-17 Artur decided to connect **Wwise 2019.1** to CryEngine AZ. The current audio is SDL Mixer, which he had accepted as is. Porting to the latest Wwise (2025.1) is deferred as TD-011.

Artur 2026-10-17 23:40: Wwise 2019.1 targets PS4 only; a PS5 build will need the Wwise upgrade (TD-011). He said to deal with it later, not now.

The plan is in `C:/CryEngine/AZ/docs/implementation/wwise-2019-plan.md`. TD-011 is in `tech-debt.md`, and the plan is announced in `agents-coordination.md`.

**Facts checked by reading files:**
- The prebuilt `bin/win_x64/CryAudioImplWwise.dll` is the Launcher's file and contains the string "Wwise v2019.1.7".
- The Crytek Asset DB "CRYENGINE Wwise Project" is also Wwise 2019.1.7, so no code port is needed.
- The CMake cache has `AUDIO_WWISE=OFF`, so configure needs `-DAUDIO_WWISE=ON` explicitly.
- The Wwise CMakeLists links every plugin lib without checking (premium and third-party: Convolution, Reflect, SoundSeed, McDSP, iZotope, Crankcase, Auro, Resonance, Motion, `ASIOSourceSink`). A missing lib means LNK1181, so Artur must install all plugins.
- MSVC above 1919 maps to `x64_vc150` libs.
- 282 of our 288 ACE controls exist under the same names in the GameSDK `audio.pak` Wwise setup (`audio/wwise`, 53 MB). Only 6 triggers need new Wwise events: JointedGlass A/B, Bottle Fire_Loop/Break A/B, `bullet_impact`.
- SDL Mixer in CE has no environments/reverb: `ConstructEnvironmentConnection` returns nullptr. It has no random variants, and its parameters only change volume. This is the main benefit of Wwise for the school horror.

**Strata** (Audiokinetic library) is REAPER projects with ready WAVs. It needs no Wwise and works with SDL Mixer too.

**Why:** Artur integrated Wwise in CryEngine long ago and wants reverb zones, random variants and dynamic music.

**State 2026-10-17 23:12 — BUILT, not switched:**
- Wwise 2019.1.11.7296 is installed at `C:/Audiokinetic/Wwise_2019.1.11.7296`. It has `WwiseCLI.exe`; there is no `WwiseConsole` in 2019.1.
- `Code/SDKs/Audio/wwise` is a junction to that install, and CMake is configured with `AUDIO_WWISE=ON`.
- Two engine edits were needed, both recorded in `engine-patches/README.md`:
  - CMake lib name `CrankcaseAudioREVModelPlayerFX` → `...Source`;
  - the `Common.h` guard allowed only 2019.1.0–.7 (C1189 error), now up to .11.
- Profile builds of `CryAudioImplWwise.dll` ("Wwise v2019.1.11") and `EditorPlugins/ace/EditorCryAudioImplWwise.dll` finished with 0 errors and 0 warnings.
- Backups and logs are in `AZ/outputs/wwise_2019_backup`.
- Trap: a configure regenerates the autogen `.res` files, so the next build relinks DLLs such as `EditorCommon.dll` without compiling.
- The Crytek Wwise project at `F:/CRYENGINE/CRYENGINE Launcher/Assets/crytek/cryengine-wwise-project/1.0.0/` has **no Originals (WAV)**. Use the prebuilt GameSDK banks; generate only our own `az_custom` bank without touching the buses.

**State 2026-10-17 23:22 — step 4 data done, bank blocked by license:**
- `Assets/Audio/wwise` holds the GameSDK ACE and banks. In `g_atl_preloads`, `c_global_voc_a/d` were dropped because the Launcher GameSDK has no localized Wwise banks.
- `Assets/Audio/wwise_project` is the Crytek project plus `az_custom` work units (Actor-Mixer, Events, SoundBanks) and `Originals/SFX/az_custom`.
- `wwise/ace/az_custom.xml` holds the 6 triggers; `bullet_impact` maps to `Play_p_pro_impact_bullet_hit_mat_metal`.
- WwiseCLI fails with "Trial mode: 1974 of 200 media". Artur must register the project (free Indie) and add the key via Project → License Manager.
- Trap: the Crytek project's SoundBankPaths for Windows is `..\wwise\assets` (our GameSDK folder), so always pass `-SoundBankPath` to a temp dir. Then copy only `az_custom.bnk` and merge its node into `SoundbanksInfo.xml`.

**State 2026-10-17 23:40 — SWITCHED to Wwise, not yet heard:**
- Artur deferred the license ("200 is enough for now").
- `tools/audio/build_wwise_az_custom_bank.py` builds `az_custom.bnk` from a trimmed temp copy (3 media), checks the event and bus IDs, and installs the bank plus the `SoundbanksInfo` merge with backups.
- `Game.cryproject` has `"s_ImplName": "CryAudioImplWwise"`. Rollback: delete that line; backup in `outputs/wwise_2019_backup`.
- Project cvars apply before audio init (`ParseProjectFile` at `SystemInit.cpp:1826`; unregistered cvars are deferred in `m_configVars`).

**State 2026-10-17 23:55 — HEARD: all sounds work on Wwise except the smoke grenade:**
- The smoke grenade never had sound: GameSDK has none, and our code excluded it from bounces and played only particles on emission.
- Artur chose the frag sounds for now. Bounces use materialmap row `smokegrenade` (a copy of `explosivegrenade`). The pop uses row `smokegrenade_explode` → MFX `azsdk_grenade:smoke_pop`, audio only (`Play_p_pfx_expl_grenade`), called from `SmokeManager` at emission.
- `ParticleGrenade.cpp:91` no longer excludes smoke. These are Codex files; the change is logged in the coordination log at 23:55. Game.dll 23:53:38 contains it; not heard yet.
- A real hiss later only needs the trigger in `smoke_pop` changed (a data edit).

**State 2026-10-18 01:20 — profiler on, first reverb zone written but not exported:**
- `Game.cryproject` has `s_WwiseEnableCommSystem 1`.
- `l_000/layers/audio.lyr` (from `tools/audio/make_l000_audio_layer.py`) holds an AreaBox over Classroom_v02 → AudioAreaEntity `room_small`. Its format comes from the GameSDK Woodland `audio.lyr`.
- The level discovers new layer files (like `chalkies.lyr`), but the game needs an l_000 headless export.
- EXPORTED 10-18 01:20 at Artur's request; level.pak has `Environment="room_small"`. **ACCEPTED by Artur 10-18 ~01:35** ("всё работает прекрасно"): Wwise audio, smoke grenade and classroom reverb.
- TRAP: the CE 5.7 AudioAreaEntity is a C++ component. The legacy GameSDK `<Properties audioEnvironmentEnvironment=...>` is ignored and exports `Environment=""`. Write a `<Components><Component typeId="a8ec43da-c8b0-46a8-9418-2c1c8c56f2d1" Environment=... EnvironmentDistance=.../>` block instead.
- Every `-BatchMode -export` ends with a "Pure function call" (0xC0000409) after "Terminate Process after export". This happens with SDL too and is harmless; the export is complete.
- codex-katimavik is importing classroom v08 into l_000; the zone may need moving.
- Available GameSDK environments: cave, cliff_delays, forest, hangar, open_ground, room_small, tower.

**How to apply:**
- Next: Artur checks the smoke grenade. Look for `AZ_SMOKE_EMIT ... audio=1` and `AZ_MATERIAL_IMPACT row=smokegrenade`.
- Tuning levers: `az_custom` (fully tunable), `Init.bnk` mix (buses, reverb, ducking; regenerable without originals or a key), reverb zones on the school level, replacing GameSDK sounds (their originals are missing).
- To edit our own sounds, edit `wwise_project` az_custom work units, then rerun the tool.
- Once Artur adds the key, generate from the full project instead.

Related: [[reference-cryengine-build]], [[reference-cryengine-gamesdk-assets]].
