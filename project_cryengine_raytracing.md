---
name: project-cryengine-raytracing
description: "DXR ray tracing for CryEngine AZ: analysis DONE 2026-10-21 (report path); DXR 1.1 inline RayQuery + DXC-at-build DXIL; DENY_SHADER_RESOURCE trap on VB/IB banks; sun first (no flashlight in CE port); NRD before DLSS-RR; step plan 0a..6"
metadata:
  node_type: memory
  type: project
  originSessionId: e8dac088-c2ca-41ed-a9c4-8db78fb52b5a
  modified: 2026-10-21T05:17:53.725Z
---

On 2026-10-21 a claude session (`claude-rt`) wrote the DXR analysis for the CryEngine 5.7.1 port (task by the DLSS session, originally for codex). Research only: no engine code changed, nothing built.

- **Report (Russian):** `C:/CryEngine/AZ/docs/implementation/raytracing-analysis.md`. It has file:line hooks, answers to the 10 open questions, the step plan 0a..6 with session estimates, risks and a progress log (section 9). Task: `raytracing-analysis-task.md` in the same folder.
- **Path chosen:** DXR 1.1 inline `RayQuery` in compute passes inside CryRenderD3D12, with no state objects and no Agility SDK. HLSL is compiled at build time by the Windows SDK 10.0.26100 `dxc.exe` (1.8.2502) into a `-Fh` C header that is embedded in the DLL. Never route DXIL through CryFX/CShader. Everything sits behind `r_RayTracing` (default 0).
- **Traps found:**
  - Every VB/IB bank buffer gets `D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE` (`CCryDX12Buffer.cpp:120-121`). That forbids the NON_PIXEL_SHADER_RESOURCE state that BLAS input needs; per the MS docs it is only valid together with ALLOW_DEPTH_STENCIL. Step 0b removes it for buffers.
  - The core command list changes on every submit, so fetch CommandList4 per call.
  - Add `m_nCommands` after native calls, or the list may be omitted.
  - Never put native barriers on engine resources; use `TrackResourceUsage` like `PrepareForDLSS`.
  - Release everything before `CDeviceObjectFactory::ResetInstance` (`D3DRenderThread.cpp:221-253`).
- **Decisions in the report:**
  - RT shadows first, starting with the **sun**. There is **no flashlight in the CE port**: `AZ/Code` has none, the GameSDK `Flashlight` accessory is not wired to any AZ item, and l_000 has no lights.
  - Characters go in via compute skinning enqueued for RT only (option C). Setting `Flags="64"` in the CDF instead breaks bone motion vectors for DLSS.
  - NRD SIGMA/REBLUR before DLSS-RR. RR needs DLSS moved before tonemapping.
  - SVO `e_svoTI_RT_Active` is not a DXR substitute: it is reset by the TOD preset, and l_000 uses IntegrationMode 0.
- **Spring 2026-04-03 attempt:** recommended for deletion; it is Artur's decision. Only the empty `GraphicsPipeline/RayTracing.{h,cpp}` file names are reused. Keep the menuflash `TropospherePass` fix in `D3D_SVO.cpp`.
- **Perf truth:** the "9-10 ms GPU at 1080p" figure in the task is the **Unreal** baseline ([[project_perf_baseline_2026-10-09]]). CE l_000 4K is about 8.2 ms native and 4.8 ms with DLSS Quality. There is no per-pass GPU breakdown yet; `r_profiler 2` gives one.

- **Implementation brief for codex (2026-10-21 09:45):** `C:/CryEngine/AZ/docs/implementation/raytracing-implementation-task.md` (steps 0a -> 0b -> 1, agent name `codex-rt`); raw research files `docs/implementation/raytracing/research/01..06`; TASK line in agents-coordination.md.

**Why:** the next session starts step 0a from the report without re-research.

**How to apply:**
- Read the report first.
- Get Artur's go per step.
- Keep the DLSS traps in mind ([[project-cryengine-dlss-research]], [[project-cryengine-dx12]]).
- `CryAnimation.dll` and `Cry3DEngine.dll` date from 2026-04-03. Do a control rebuild before editing them.
