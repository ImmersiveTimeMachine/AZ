---
name: project-cryengine-episode-scene-research
description: "2026-10-17 research: episode + scene + restrictions mechanism for CryEngine CHALK; report link, recommended option, engine traps"
metadata:
  node_type: memory
  type: project
  originSessionId: 3cd4de71-a690-4b83-aefe-21bac5c9b3f5
  modified: 2026-10-17T18:34:27.801Z
---

Research only (2026-10-17), nothing implemented. Artur asked for a scenario mechanism: episodes and scenes that restrict movement, weapons, interactions, UI, input and camera, play cutscenes, chain, save, and start any scene for tests or as the game start (test level l_000).

Report: https://claude.ai/artifact/8DtB3dFE53PiwqSherZsjZ (HTML in `2026-10-15/realtime-voice-chat/outputs/cryengine-episode-scene-research/reports/`, notes with source lines in `research_notes/`).

Recommended option В: a new plugin AZStory with XML episodes and scenes, an owner-keyed restriction manager (union of tags, dump/reset), facts, an AZSave participant that stores ids and facts (locks are recomputed on load, never saved), `az_story start` plus a command-line start, a TrackView bridge modelled on GameSDK CCinematicInput, and FlowGraph `AZ:Story:*` nodes later for staging only.

Engine traps found:
- 5.7.1 does not hide the player or HUD and does not block input for cutscenes; the flag is `NoUI`, not NoHUD.
- Action filters are plain bools.
- Layer switching is a no-op while `UseLayersActivation=0`; l_000 has 0.
- `Engine:LayerSetSwitch` always outputs Failed.
- The MCP can author only Math and Logic flow nodes.

**Why:** Artur will decide the next step after reading the report. There are four open questions: who authors scenes, whether an episode equals a level, saves inside scenes, and cutscene skipping.

**How to apply:** read the report before any episode, scene, cutscene or restriction work in CryEngine. Hooks in Codex files (movement, weapons, camera) need an announcement first.

Related: [[project-cryengine-noesis-ui]], [[reference-cryengine-test-level-l000]], [[feedback-keep-stated-goal-scope]].
