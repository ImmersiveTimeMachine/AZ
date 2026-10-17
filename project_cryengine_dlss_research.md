---
name: project-cryengine-dlss-research
description: "DLSS for CryEngine AZ (research only, 2026-10-17): chosen path Streamline 2.14.16 manual hooking, where the report lives, key hook points"
metadata:
  node_type: memory
  type: project
  originSessionId: 6f9fae2d-6b10-47fe-a778-b863c13d3aad
  modified: 2026-10-17T18:53:01.537Z
---

On 2026-10-17 Artur asked what it takes to add NVIDIA DLSS (SR first, ideally Frame Generation + Reflex) to the CryEngine 5.7.1 DX12 renderer. The work was research only: nothing was implemented and no engine file was changed.

- **Report (Russian):** `C:/Users/Artur/AppData/Roaming/Claude/side-session-notes/local_80d494ed-8c3d-4a49-8e0a-7f5c4787c5fc/dlss-research.md`. It has file:line references, a step plan with effort estimates, a risks table and links.
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
