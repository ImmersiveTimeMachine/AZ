---
name: reference-blender-socket-exec
description: "Fast way to drive the user's open Blender (blender-mcp socket 127.0.0.1:9876) + traps found 2026-10-10 (Ctrl+Z reverts script edits, fan triangulation lies, boolean coplanar failure)"
metadata:
  node_type: memory
  type: reference
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-13T21:16:08.242Z
---

The user's Blender (5.2, `C:/Program Files/Blender Foundation/Blender 5.2/blender.exe`) runs the blender-mcp addon on 127.0.0.1:9876.
Send `{"type":"execute_code","params":{"code": <file text>}}`; reply `{"status","result":{"result": stdout}}`.
Client: `Art/Wall_TwoWindows_Boarded/v20261010_SemanticUV/scripts/bl.py <script.py>`.
Fast pattern: dump mesh to JSON → compute in numpy outside → write back with foreach_set → read back and compare; check exports with `blender -b --factory-startup --python check.py -- file.fbx`.
Traps:
- Script edits are not undo steps; the user's Ctrl+Z silently reverted them twice. Call `bpy.ops.ed.undo_push(message=...)` after each edit, assert vertex/face counts at the start of every script, and ask the user not to press Ctrl+Z while scripts run. `bpy.ops.ed.undo()` from the socket returned FINISHED but did nothing → restore by rebuilding from the dumped JSON.
- Non-convex n-gons: fan triangulation in validators/rasterizers gives false overlaps and can mask holes; use Blender's `loop_triangles`.
- Exact Boolean union of coplanar-touching parts kept duplicate faces and made non-manifold edges; cut the smaller face along the bigger face outline (bisect_plane) and delete the covered piece instead.
- tessellate_polygon loses area on polygons with collinear vertices → use a 2D even-odd point-in-polygon test.
- (2026-10-13) The socket prints only stdout: use `print(...)`, a `result = ...` variable returns nothing.
- (2026-10-13) Background Blender 5.2 crashes (EXCEPTION_ACCESS_VIOLATION) in `cycles prefs.get_devices()/refresh_devices()`; run background Cycles renders with Blender 5.1 (`.../Blender 5.1/blender.exe -b --factory-startup`, sees the RTX 4080 via OPTIX; opens 5.2 files). Use --factory-startup: the user's Substance addon load_pre handler errors in background.
- (2026-10-13) New objects: `matrix_world` is not valid until `view_layer.update()`; compose the matrix yourself (T @ R @ S) instead of reading it right after setting location/scale.
Unreal side: unreal-mcp CaptureViewport returns ~6 MB base64 into context; prefer Rider `ue_execute_python` and screenshots written to disk.
