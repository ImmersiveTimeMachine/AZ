---
name: feedback-open-the-file-i-must-edit
description: "STANDING RULE: whenever Artur is asked to change something by hand, open that file/asset for him first — the Blueprint in the editor, the C++ file in Rider."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-19T04:11:45.499Z
---

**Never hand Artur an asset path or a file name and leave him to find it. Open it.**

- Blueprint / AnimBP / data asset / any UE asset → open it in the Unreal editor:
  `unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])`
- C++ / any source file → open it in Rider with `mcp__rider__open_file_in_editor` (pass the line when the
  spot is known).

Then say what to change, in the already-open thing.

**Why:** stated as a rule on 2026-09-19 — *"если ты хочешь, чтобы я где-то что-то поменял, открой этот
файл для меня… Если это будет C++ файл, то открой его."* Hunting for an asset by path is friction I can
remove with one call, and this project has thousands of assets.

**How to apply:** it is a REFLEX, not a favour to offer. Any time a reply contains "поставь", "измени",
"посмотри в" plus a path or an asset name, the open call goes out in the same turn, before or alongside
the instruction. Applies to things I cannot edit myself — AnimBP graph nodes (Python struct-array writes
do not stick and compiling an AnimBP from Python crashes the editor), and anything he prefers to do by
hand.

Related: [[feedback-deliver-result-not-analysis]] — do as much as I can myself; when something genuinely
must be his, make it one click.
