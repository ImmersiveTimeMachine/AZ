---
name: feedback-consider-local-model
description: "★★★ USER RULE (2026-09-25): whenever I split a task into steps, consider delegating mechanical steps to the local LM Studio model; check it is running first, otherwise ask Artur. Procedure = skill local-model-delegation."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-25T23:29:01.563Z
---

Before executing any non-trivial task, while splitting it into steps, decide which steps the LOCAL model can run
(`.agents/agent_script.py`, LM Studio, default Bonsai-27B) from a precise spec, then verify its result myself.

**Why:** Artur wants to save Claude tokens: "перед каждой задачей когда ты будешь ее разбивать всегда принимать во
внимание возможность использования локальной модели конечно проверь перед этим что она запущена или меня спросить".
Field test the same day: Bonsai built all 7 whole weapon meshes from `docs/agent-tasks/whole-weapon-meshes.md`,
7/7 verified.

**How to apply:** load skill `local-model-delegation` (preflight, what to delegate / never delegate, spec format,
run command, verification). Mechanical long work -> delegate; design, debugging, destructive ops, git, builds and
1-3-step chores -> myself. If LM Studio or the model is not up, ask him - do not load models on his GPU uninvited.
Say in one line which steps I delegate and why, so he sees the split. See [[reference-local-agent-lmstudio]].
