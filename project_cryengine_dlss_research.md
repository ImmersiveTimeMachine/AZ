---
name: project-cryengine-dlss-research
description: "DLSS for CryEngine AZ: Streamline 2.14.16 manual hooking; steps 0-3 (DLAA, upscale, Reflex, frame generation) + settings menu Graphics section DONE 2026-10-20; report path, file map, traps"
metadata:
  node_type: memory
  type: project
  originSessionId: 6f9fae2d-6b10-47fe-a778-b863c13d3aad
  modified: 2026-10-17T18:53:01.537Z
---

On 2026-10-17 Artur asked what it takes to add NVIDIA DLSS (SR first, ideally Frame Generation + Reflex) to the CryEngine 5.7.1 DX12 renderer. The work was research only: nothing was implemented and no engine file was changed.

- **Report (Russian):** `C:/Users/Artur/AppData/Local/Claude-Data/claude_pzs8sxrjxfjjc/side-session-notes/local_80d494ed-8c3d-4a49-8e0a-7f5c4787c5fc/dlss-research.md` (section 10 = progress log). It has file:line references, a step plan with effort estimates, a risks table and links.
- **Direction (Artur):** «Для CryEngine Streamline — лучший выбор». The order is sl.dlss → sl.reflex + sl.pcl → sl.dlss_g. Integration is manual hooking inside CryRenderD3D12, the way the UE plugin does it. Direct NGX is only a fallback.
- **SDK:** Artur downloads it himself (he has NVIDIA access). Use streamline-sdk-v2.14.16.zip from GitHub releases: its `bin/` holds the signed production DLLs and the dev DLLs. Never take DLLs from shipped games.
  - His UE plugins already hold SL 2.14.1 and NGX 310.9.1 at `C:/UnrealEngine/Engine/Plugins/Plugins/{DLSS,StreamlineCore,...}`, without `sl.dlss.dll`.
  - The UE plugin source is NVIDIA proprietary. Read it as a reference only; do not copy it.
- **Hardware:** RTX 4080 SUPER, driver 617.42. That covers SR, DLAA, FG 2x and Reflex; MFG is not available. Frame Generation also needs HAGS enabled.
- **Key hook points:**
  - DLSS replaces `m_UpscalePass` in `ResolveSubsampledOutput` (`DriverD3D.cpp:1323`). Render resolution comes through `r_CustomRes`.
  - A new "DLSS" AA mode keeps the jitter and turns off CE's own TAA.
  - New compute pass for motion vectors (camera from depth plus object velocity).
  - After the DLSS call, use `ResetCachedState(true, true)` (it records compute work).
  - FG needs a separate Noesis UI layer.
- **Stock bug found:** the swap-chain recreate call passes isMainContext/isFullscreen swapped (`SwapChain.cpp:76` vs `SwapChain.h:71`, call at `RenderDisplayContext.cpp:456`). Fix it before FG.

**Why:** a later session will implement this. It must start from the report and not redo the research.

**How to apply:**
- Read the report first.
- Get Artur's go per step, as with all engine changes.
- Keep the DX12 risks in mind: [[project-cryengine-dx12]] crash classes and the TD-009 exit fixes in [[project-cryengine-noesis-ui]].
- Changes to the UI hook belong to the UI agents.

**Step 0 DONE 2026-10-20** (CryRenderD3D12.dll 20:14). SDK at `C:/CryEngine/Code/SDKs/Streamline` (gitignored, downloaded from GitHub Releases: the git repo has no DLLs). Configure deploys DLLs to `bin/win_x64/Streamline` and `.../development`.
- **Files:** `RenderDll/XRenderD3D9/DeviceManager/D3D12/Streamline_D3D12.{h,cpp}` is the engine side. `StreamlineLoader_D3D12.{h,cpp}` holds the SL SDK calls.
- **Hook calls:** `DX12Device.cpp` calls `AZStreamline::OnDeviceCreated` after `D3D12CreateDevice`. `D3DSystem.cpp` ShutDown calls `AZStreamline::Shutdown()` before `ReleaseCoreCommandListDX12`.
- **CVars:** `r_Streamline` and `r_StreamlineDev`, default 0.
- **Verified:** DLSS / Reflex / PCL / DLSS-G all supported on the 4080, in run `test_out/claude_dlss_step0b`.
- **Not verified:** slShutdown on exit. Console `quit` from l_000 crashes earlier, in OnSystemEvent of an unloaded DLL; the same crash happens WITHOUT Streamline, so it is not ours; handed over in the coordination log.
- **Traps paid for:**
  - SL headers clash with the renderer PCH, and sl_security.h needs `_WIN32_WINNT` 0x0A00. Keep SL includes in the `/Y-` loader file only.
  - The engine macro `EXCLUDE_FILE_FROM_MSVC_PRECOMPILED_HEADER` is broken (it appends ";/Y-"); use `set_property(... APPEND_STRING ... " /Y-")`.
  - `+cvar` command-line args run only after device creation, so startup CVars are also read via ICmdLine.
  - My own MSBuild `nodeReuse` workers block `agent_lock.py` for 30 minutes; build with `-nr:false`.
  - A direct launch with system.cfg `r_Fullscreen 1` (4K exclusive) hits the Present assert at DriverD3D.cpp:3670; test windowed with `+r_Fullscreen 0 +r_Width 1920 +r_Height 1080`.
- **Test command:** `python -I tools/ui/run_game_test.py <name> <scenario> --map l_000 --agent claude-dlss --extra "+r_Streamline 1 +r_Fullscreen 0 +r_Width 1920 +r_Height 1080"`.
- **Step 1a (DLAA) DONE 2026-10-20** (DLL 20:45; Artur's visual check pending). It runs with `+r_Streamline 1 +r_DLSS 1`.
  - DLSS replaces SMAA/TAA in `CPostAAStage::Execute`, using a Halton-8 jitter. A new `DLSSMotionVectors` pass in `PostAA.cfx` writes R16G16F UV vectors (prev−curr), and DLSS writes into a UAV texture `$DLSSOutput`.
  - With `r_Streamline`, swap chains are created through `slUpgradeInterface(factory)` in `DX12SwapChain.cpp`, because manual hooking needs `presentCommon` on every Present.
  - With `r_DLSS`, `Renderer_Jobs.cpp` writes velocity at any distance and for skinned characters that stand still. Without that, Chalkie heads ghost.
  - Traps: allocate the pass's typed CB in `Init`; `sl::Constants` has no `renderingGameFrames` field.
  - Jitter sent as `(-ndc.x*w/2, +ndc.y*h/2)` (CE proj m23=-1); `r_DLSSDebugJitter` flips it.
- **Step 1b (upscaling) DONE 2026-10-20** (DLL 20:54). `r_DLSS` 2..5 = Quality / Balanced / Performance / Ultra Performance.
  - The output-size override must live in `CRendererCVars::GetCustomResWidth/Height` (`s_azUpscalerOutput`). CRenderOutput recomputes the size from them and asserts.
  - DLSS replaces `m_UpscalePass` in `ResolveSubsampledOutput`.
  - Mip bias is `log2(render/display)`.
  - The mode-switch frame is skipped (NGX 0xbad00005 otherwise).
  - FPS at 4K on l_000: native ~158, DLAA ~135, Quality ~210, Balanced ~237, Ultra Performance ~320.
- **Step 2 (Reflex + PCL) DONE 2026-10-20** (DLL 21:01). r_Reflex: 0 = off, 1 = low latency (the default), 2 = boost.
  - Markers are set in BeginFrame, EndFrame, RT_BeginFrame, before Present in RT_EndFrame, and around the real Present in DX12AsyncCommandQueue.
  - Frame tokens are created only on the main thread and kept in a ring indexed by frame. slGetNewFrameToken is not thread safe.
- **USER REQUEST:** once everything works, put the DLSS mode, Reflex and frame generation in the game's settings menu. Do it with the menu owner (AZNoesis / AZSave settings).
- **Step 3 (DLSS-G + Noesis UI layer) DONE 2026-10-20** (DLL 21:41). Run with +r_Streamline 1 +r_DLSS 1..5 +r_DLSSG 1. r_DLSSG must be 1 at start-up; after that 0/1 toggles at runtime.
  - Swap chain: under SL it is created with CreateSwapChainForHwnd through the proxy factory.
  - Window mode: while FG is requested, exclusive fullscreen is mapped to borderless fullscreen. The engine recreates the swap chain right after creation, and SL rejects that ('Host did not release swap-chain').
  - Stock bug fixed: borderless fullscreen used GetMonitorInfo(m_activeMonitor), and m_activeMonitor is never assigned in stock 5.7.1. Now MonitorFromWindow.
  - ResizeBuffers under SL: keep the proxy's buffer count, format and flags (0x842 includes WAITABLE). Take the size from the engine desc, because the proxy GetDesc returns a garbage width.
  - slInit needs eUseFrameBasedResourceTagging for slSetTagForFrame.
  - Engine debug check 'Source Resource is referring to current swapchain index' is skipped while DLSS-G is loaded.
  - Tags are set with eOnlyValidNow at OnSceneComplete (D3DRendPipeline after the resolves): depth, motion vectors, HUD-less.
  - UI layer: in ExternalUI_D3D12.cpp, Noesis draws into a layer that is composited premultiplied and tagged UIColorAndAlpha; UI recomposition is on.
  - FPS at 4K Quality: 181 without FG; with FG 134 rendered, about 268 on screen.
  - The SL file log is %USER%/Streamline/sl.log (AZ/user/streamline). Use r_StreamlineDev 1 for verbose dev DLLs.
- **Next:** game settings-menu entries (with the menu owner). Then commit only the DLSS hunks: the engine branch is codex/alembic-header-fix and holds many uncommitted changes from other agents, so ask Artur about scope and branch first.

- **Settings menu (10-20 22:00):** section Graphics in AZNoesis NoesisMenuSettings (r_DLSS / r_DLSSG / r_Reflex). These rows persist in %USER%/game.cfg (CSystem loads it before CreateRendererVars, pending LoadConfigVar applies on registration), NOT AZSettings.xml; r_Streamline derived. Read-only r_StreamlineState bits 1/2/4/8/16 tell the menu what runs. Streamline never loads in Sandbox. Trap: CFullscreenPass(flags) ctor leaves m_pGraphicsPipeline uninitialized -> pass the base pipeline (assert FullscreenPass.cpp:167). Tests claude_menu_dlss_a/b/c; game.cfg removed after tests. Next: commit DLSS hunks (ask Artur scope/branch).
