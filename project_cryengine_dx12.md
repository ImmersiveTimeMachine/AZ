---
name: project-cryengine-dx12
description: "CryEngine 5.7.1 DX12 crash history, the masking guards, and the current state; the ТЗ for DX12 stabilisation plus Noesis (path A)"
metadata:
  node_type: memory
  type: project
  originSessionId: 8f8d09b9-3f5e-408a-8d76-c4e4c3910763
  modified: 2026-10-18T18:18:14.484Z
---

On 2026-10-16 Artur decided to use DX12. **The current goal is the NoesisGUI integration on DX12** via "path A": the Noesis SDK D3D12 device running on CryEngine's DX12 objects.
- Integration ТЗ: `2026-10-15/realtime-voice-chat/outputs/cryengine-dx12/TZ_Noesis_DX12_v01.md`.
- DX12 stabilisation is a SEPARATE, later task with its own ТЗ: `TZ_DX12_v01.md`. Do not make it the focus ([[feedback-keep-stated-goal-scope]]).

**Crash classes:**
- **C1. Texture creation**, on Chalk only.
  - Crash point: `CDeviceTexture::Create` / `RT_CreateDeviceTexture`.
  - The layout read on the render thread is garbage: w=h=d=57311 (0xDFDF) with a null format. Affected: `$LDR-Output-*$RT_ENV_2D*` textures.
  - This looks like a texture lifetime bug between enqueue and execution.
- **C2. Mapped upload**: `MappedWriteToSubresource` → `StreamBufferData`.
  - Seen in TPS water and in GameSDK tiled lights.
- **C3. The prebuilt Launcher engine** also crashed on DX12 (2026-04-03).

**Guards** (they prevent the crash but hide the root cause):
- 2026-04-04: `DeviceResources.cpp`. It was only compiled in on 2026-10-15; the DX12 DLL dated from 2026-04-03 until then.
- 2026-10-15: `DX12Resource.cpp`, `DeviceObjects_D3D12.cpp`, `CCryDX12DeviceContext.cpp`.
- Diagnostics tags: `[DX12 mapped upload failure]`, `null pixel format`, `[TextureLayoutDiagnostic]`.
- `ShadowMap.cpp` has a hardcoded 4096 shadow clamp (since 2026-04-03).
  - Every requested size must stay ≤ 4096. Otherwise the shadow and RSM passes get depth and colour targets of different sizes, and `DeviceRenderPass.cpp:138/140` SetDepthTarget asserts.
  - Fixed in config on 2026-10-18: `C:/CryEngine/system.cfg` `e_ShadowsPoolSize` 8192→4096; `Engine/Config/CVarGroups/sys_spec_Shadows.cfg` `r_ShadowsCacheResolutions` capped at 4096. Backups: `*.bak-20261018-claude`.
- `C:/CryEngine/system.cfg` (04-04) is a hand "tuning" file: 4K fullscreen, 8 cascades, `e_ShadowsResScale` 80 (that is the slope bias, so it is now commented out) and several invented cvars. Distrust it.
- **SVO Troposphere bug:** `D3D_SVO.cpp` TropospherePass writes MRTs of different sizes: AIR_MIN/AIR_MAX at W/air and SHAD_MIN_MAX at W/base. With `e_svoTI_ResScaleAir` ≠ `e_svoTI_ResScaleBase`, only the top-left air/base part of the screen gets sun light; Artur's «Shadow from Sun» rectangle was 3/4, then 2/3. Fix: `Game.cryproject` `e_svoTI_ResScaleAir=2` (= base). Verified 2026-10-18.

**State after the CryRenderD3D12 build of 2026-10-15 20:34:**
- About 25 DX12 sessions (TPS, AZ, MCPChecks) with 0 crashes and 0 diagnostics.
- Chalk, TPS water and Woodland have not been re-tested since.

**How to apply:**
- Do not call DX12 "fixed" until the ТЗ A1 matrix passes.
- The debug layer is enabled with `r_EnableDebugLayer` (1, or 2 for GPU validation).
- DRED is not implemented.

Related: [[project-cryengine-mcp-review]], [[reference-cryengine-gamesdk-assets]].
