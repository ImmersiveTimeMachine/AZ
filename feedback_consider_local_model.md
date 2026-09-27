---
name: feedback-consider-local-model
description: "★★★ USER RULE (2026-09-25): whenever I split a task into steps, consider delegating mechanical steps to the local LM Studio model; check it is running first, otherwise ask Artur. Procedure = skill local-model-delegation."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-26T21:40:33.128Z
---

Before executing any non-trivial task, while splitting it into steps, decide which steps the LOCAL model can run
(`.agents/agent_script.py`, LM Studio, default Bonsai-27B) from a precise spec, then verify its result myself.

**Why:** Artur wants to save Claude tokens: "перед каждой задачей когда ты будешь ее разбивать всегда принимать во
внимание возможность использования локальной модели конечно проверь перед этим что она запущена или меня спросить".
Field test the same day: Bonsai built all 7 whole weapon meshes from `docs/agent-tasks/whole-weapon-meshes.md`,
7/7 verified.

**Missed on 2026-09-26 - user called it out:** I ran ~12 batch passes over 420-446 clips myself (finger bake, grip
curves x2, left-grip fix) without deciding who should run them. His words: before any big job that touches many clips
/ assets, decide whether I do it or hand it to a weaker model ("более слабой модели... какой-нибудь своей") - i.e. the
LOCAL model OR my own cheaper subagent (Sonnet, see [[feedback_model_routing_policy]]; Haiku banned) - "и нужно его
постоянно выполнять". So EVERY time, before launching a batch: write one line "кто делает: local / Sonnet / я, почему",
then execute that way. Batch = the same script over many assets in chunks, report checking, repetitive saves.
Design, solvers, first run on 1-8 assets to validate, debugging -> me; the bulk rerun of a validated script -> delegate.

**How to apply:** load skill `local-model-delegation` (preflight, what to delegate / never delegate, spec format,
run command, verification). Mechanical long work -> delegate; design, debugging, destructive ops, git, builds and
1-3-step chores -> myself. If LM Studio or the model is not up, ask him - do not load models on his GPU uninvited.
Say in one line which steps I delegate and why, so he sees the split. See [[reference-local-agent-lmstudio]].

**User rule 2026-09-26:** when LM Studio is off, do NOT do the mechanical steps myself - delegate them to my most economical subagent (Sonnet; Haiku stays banned). I analyse, write the exact spec, verify.
