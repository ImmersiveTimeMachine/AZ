---
name: reference-painter-mcp
description: "How to drive Substance Painter 12.1.5 through the substance-painter MCP (elliezu, 79 tools): launcher with OCIO, client script, and the 5 traps found 2026-10-12"
metadata:
  node_type: memory
  type: reference
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-13T15:08:50.638Z
---

Painter MCP = `~/.codex/mcp-servers/substance-painter` (upstream elliezu/SubstancePainterMCP), registered for Claude on 2026-10-12 with the user's OK: `claude mcp add substance-painter --scope user` (roots: Art/Painter_MCP/{Projects,Imports,Resources,Exports}; SP_MCP_ALLOW_EXECUTE_PYTHON=0 kept). In a session where its tools are not loaded, call it with `Tools/painter_mcp/sp_call.py <tool> <json|@file>` (venv python of the MCP); multi-step jobs as scripts like `Tools/painter_mcp/painter_wall_layers_v03.py`.
Traps:
1. Painter must be started with `Tools/painter_mcp/Start_Painter_MCP_OCIO.cmd` (remote scripting port 60041 + env OCIO=<Painter>/resources/ocio/substance/config.ocio). API-created projects are LEGACY colour-managed without that env var, and the MCP sets every colour/channel through the OCIO Color API -> "Can't use a non legacy color space with a legacy color managed project". There is no UI setting for it.
2. Colours passed to the MCP (recipe base_color/channels, set_fill_base_color) are sRGB: they go through colormanagement.Color() -> do NOT pre-convert (my old **2.2 note was wrong; verified 2026-10-13 with get_fill_sources: 0.17 -> stored 0.0245 linear).
2b. set_fill_resource re-assigns node.active_channels, which RESETS every other channel source of that fill layer to default (BaseColor 0.5 grey, Roughness 0.3) -> only the last channel set survives. Use ONE bitmap channel per fill layer (this bug made glass v02-v06 milky). Verify with get_fill_sources.
2c. Groups made by create_layer_recipe blend Normal (hide everything below) -> set_layer_properties blending_mode 'Passthrough' per channel on each group.
2d. Stock smart mask 'Dust' = Grunge Concrete Dirty inside: reads as concrete streaks on glass; avoid on glass.
3. Bake preflight refuses non-UDIM projects ("No UV tiles are enabled") -> create projects with project_workflow "UVTile" (UVs in 1001). Bakes ~25 s at 4K low-as-high.
4. search_resources matches words: query "concrete", not "concrete_with_paint_chips"; resource names are "package/graph". User shelf `your_assets` (Documents/Adobe/.../assets) holds ~60 downloaded sbsar (peeling_paint_wall, layered/soiled/damp_plaster_wall, glass, ...).
5. set_fill_resource works only on fill LAYERS (not fill effects inside masks) -> use smart masks (Edge Damage, Dirt Cavities, Dirt Ground, Water/Rust Drips, Concrete Edges, Dirt Leak Dry) on separate layers; recipe item order = top first; template must come from SP_MCP_RESOURCE_ROOTS (copied Unreal Engine.spt there).
Glass 2026-10-13: project `Projects/Classroom_WindowGlass_v01.spp` holds 3 variant groups (Single crack / Meshy style broken / Procedural broken), build `Tools/painter_mcp/painter_window_glass_v08.py`, masks from gen_glass_single_crack_v01.py, gen_broken_glass_masks_v04_meshystyle.py, gen_broken_glass_masks_v03.py + shared glass_finish_v01.py; exports `Exports/Classroom_WindowGlass_v08_<variant>`. USER: a given model (Meshy) is a REFERENCE for style, never copy/stack its pieces; one glass pane = one continuous sheet.
Result 2026-10-12: classroom window wall project `Art/Painter_MCP/Projects/Classroom_WallWindows_v04_UE_OCIO.spp`, textures `Exports/Classroom_WallWindows_v04_UE_v03/` (base concrete_with_paint_chips + mesh-aware edge wear/dirt/damp/drips; facade soiled_plaster_wall). Library materials beat stock smart masks on plain fills by far.
