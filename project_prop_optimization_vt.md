---
name: project-prop-optimization-vt
description: 2026-10-14 shared skill unreal-prop-optimization + Tools/unreal_prop_optimize; VT enabled project-wide; M_AZTransfer_29dd… converted to virtual samplers (3 props)
metadata:
  node_type: memory
  type: project
  originSessionId: 4ed8bc7e-61ec-42da-a018-4c39edd7dc0d
  modified: 2026-10-15T03:15:11.308Z
---

User asked (2026-10-14) for a skill so every agent can check/optimize imported props: skill `unreal-prop-optimization` (.agents/.claude/~/.codex copies), tool `Tools/unreal_prop_optimize/az_prop_optimize.py` (inspect/apply).
Done: cloakroom unit (SM_Meshy_Quebec_School_Cloakroom_Unit…006_v001) box collision + colour/normal max 2048; parent `_AZTransferMaterials/M_AZTransfer_29dd864d35734b3f` samplers → Virtual*, all 9 textures of its 3 instances VT (also window bay + FreshCharts window). Window bay (SM_Meshy_weathered_school_window_bay…005_v001) same fix via apply(max_size=2048, collision='box'). FreshCharts window: VT only, not checked further.
User view: environment = Nanite (decimated to 0.3–2M), no high→low bake except skinned; texture memory is the real cost.
**Why:** project enabled r.VirtualTextures + MegaLights same day to match DerelictCorridor quality.
**How to apply:** after every Blender→Unreal transfer run the tool; the transfer tool still makes non-VT parents ([[reference-launcher-add-to-project]] same session).
