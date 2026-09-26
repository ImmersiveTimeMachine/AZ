---
name: reference-local-agent-lmstudio
description: "The user's local-model agent: C:/UnrealEngine/Games/AZ/.agents/agent_script.py (LM Studio + MCP host + project skills/memory/shell). How it is wired, what was measured, why the default tool set is small. User wants Claude Code untouched; the local model gets light tasks to save tokens."
metadata:
  node_type: memory
  type: reference
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-26T02:36:03.598Z
---

**★ AUTHORIZED (user, 2026-09-25): I may delegate tasks to the local agent directly from our chat and verify
afterwards** ("отсюда ты напрямую можешь ему давать такие задания, если понадобится. И проверять потом").
Workflow: I write a precise step spec (ready code, expected outputs, "mismatch -> stop and report"), run
`python C:/UnrealEngine/Games/AZ/.agents/agent_script.py --model <best> --task-file <spec.md>` (one-shot; shell OFF
unless --auto; full trace -> `.agents/runs/<time>.md`), then verify the result in the project myself. Worth it only
for long mechanical work (batch ops, many repetitive steps); design/debugging stays with me. Token: read at call time
with `[Environment]::GetEnvironmentVariable('LM_STUDIO_API_KEY','User')` (user must `setx` it; NOT set as of
2026-09-25 evening) - never write the token into files or memory. Benchmark: `.agents/bench_models.py`, results in
`.agents/bench_results.md`.

**Field test 2026-09-25 (Bonsai, one slot, ctx 32k):** did the whole-weapon spec end to end in 282 s / 23 tool calls -
script copied byte-identical, 7 builds + verify ALL PASS (final meshes now in /Game/AZ/Assets/Weapons/<W>/SM_*_Whole),
parts untouched. Weak spots: its final summary was sloppy (listed 2 of 7 builds, "AK-47"), skipped the optional view
step, and when input was cut it looped in repeated reasoning instead of stopping. ALWAYS load with --parallel 1
(4 slots spilled VRAM: 6 tok/s) and keep ctx ~32k while Unreal is open (65k filled 16 GB -> prompt stuck at 0%).
Bench (3 non-Unreal tasks): Bonsai 3/3 17 s, Glimmer 3/3 164 s, Qwen3.5-9B 2/3 6 s; gpt-oss-20b tool calls are not
parsed by this LM Studio (writes the call as text, invents the answer).

**Field test 2 (2026-09-25 night, Bonsai):** `docs/agent-tasks/riflemega-to-master.md` - 23 queued Unreal scripts,
report file read after each, stop rules. 733 s, 59 tool calls, DONE with every result line quoted; my own check
(CHECK_ALL 446 OK) confirmed it. First attempt: the editor was restarting -> it looped calling unreal_status
("fetch failed") instead of stopping - I killed it. Spec pattern that fixed it: "call the status tool ONCE; not
connected -> STOP, do not retry". Unreal scripts run fast here (20 clips ~3-25 s), ~2.5 reads per chunk.

**Intent (user, 2026-09-25):** light tasks go to a LOCAL model to save tokens; it should work "like Claude" (same
tools, skills, memory) but from any terminal via `.agents/agent_script.py`. Claude Code itself stays as is (a
Claude-Code-on-LM-Studio launcher was built, then dropped at his request).

**Wiring:** LM Studio server 127.0.0.1:1234, Require Authentication ON, token read from env `LM_STUDIO_API_KEY`
(`setx`), never stored in files. `/v1/chat/completions` does NOT execute MCP tools - the script is the MCP host
(mcp 1.26 + openai SDK, both installed; Python 3.14). Servers: filesystem (node server-filesystem, root = project),
unrealclaude (bridge at **C:/UE57/...** - the C:/UnrealEngine copy has no node_modules), unreal-mcp :8001/mcp,
rider :64343/stream, gimp-mcp (uv). Built-ins: run_command (PowerShell, user confirms each; refused by the SAME hook
`.claude/hooks/guard_git_destructive.py`), list_skills/read_skill (.claude/skills then .agents/skills),
read_memory (this memory dir, basename only). System prompt = AGENTS.md + skill list + MEMORY.md.
LM Studio also speaks the Anthropic API (/v1/messages works); its own mcp.json lives in `~/.lmstudio/mcp.json`
(unrealclaude path fixed to C:/UE57, backup `mcp.json.bak-2026-09-25`).

**Measured:** default `--servers filesystem,unrealclaude` = 34 tools, task with one tool call 3 min 17 s, answer
wrong by one (said 8 skills, there are 9). `--servers all` = 194 tools: 10 min, EMPTY answer. Loaded model
`prism-ml/bonsai-27b` is **Q1_0 (1-bit)** and its template rejects split system messages (fails under Claude Code's
Anthropic requests). Better candidates already downloaded: openai/gpt-oss-20b, qwen/qwen3-coder-next,
qwen/qwen3.6-27b (load with a large context; LM Studio's JIT default is 8192).
