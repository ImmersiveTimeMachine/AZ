---
name: local-model-delegation
description: Delegate mechanical sub-tasks to the user's LOCAL model (LM Studio + .agents/agent_script.py) to save tokens, then verify. Use at the START of every non-trivial AZ task, while splitting it into steps - decide which steps a local model can run from a precise spec. Covers the preflight (LM Studio up, model loaded with Parallel 1 and ~32k context, token, Unreal open when needed), what to delegate and what never to, the spec format, the run command, and the verification.
---

# Local-model delegation (AZ)

Artur's standing rule (2026-09-25): **every time I split a task into steps, consider which steps the local model can
do.** Check that it is running first; if it is not, or anything is unclear, ask him - never start LM Studio or load
models behind his back when he may be using the GPU for Unreal.

## 1. Decide per step
Delegate (worth the spec): long MECHANICAL work with known expected results - running a prepared script N times,
batch renames/moves, per-asset property edits from a table, checking many files against a list, collecting data
into a report. Rough saving: 60-80 % of my tokens on such steps.

Keep for myself: design, debugging, anything needing judgment, measurement or taste (grip fitting, retarget math),
anything destructive or irreversible, git, C++ builds, and tiny tasks (1-3 tool calls: the spec costs more than doing
it). Also never delegate what needs the shell with side effects outside the project.

## 2. Preflight (all must pass, else ask Artur)
```bash
curl -s -m 5 -o /dev/null -w "%{http_code}" http://localhost:1234/v1/models   # 401 = server up (auth on)
~/.lmstudio/bin/lms.exe ps                                                     # a model loaded, PARALLEL 1
powershell -NoProfile -Command "[bool][Environment]::GetEnvironmentVariable('LM_STUDIO_API_KEY','User')"
```
- Model: default `prism-ml/bonsai-27b` (bench 2026-09-25: 3/3, fastest correct). **PARALLEL must be 1** (4 slots
  spilled VRAM -> 6 tok/s) and context ~32768 while Unreal is open (65k filled the 16 GB card -> prompt stuck at 0 %).
  If it is not loaded or wrongly loaded, ask Artur before (re)loading: `lms load prism-ml/bonsai-27b -c 32768 --parallel 1 -y`.
- Token: from the USER env var above. If missing, ask him to `setx LM_STUDIO_API_KEY "<token>"`. Never write the
  token into files, memory, or logs.
- Unreal Editor open (and `http://localhost:3000` answering) if the step touches Unreal.

## 3. Write the spec (file under docs/agent-tasks/ or the scratchpad)
Model the reference `docs/agent-tasks/whole-weapon-meshes.md`: goal; hard rules (never touch X, never save the level,
no improvising); exact commands to run, one per tool call; the ready code to save verbatim; the EXACT expected result
line after each step; "anything else -> STOP and report the line"; the final report format. Put all decisions and
numbers in the code - the model executes, it does not decide. Keep the spec < 60k chars (the agent's tool-output cap).
Reading results: the Unreal tool answers "queued" - tell it to read a report FILE the script writes.

## 4. Run
```bash
cd /c/UnrealEngine/Games/AZ && PYTHONIOENCODING=utf-8 LM_STUDIO_API_KEY="$(powershell -NoProfile -Command "[Environment]::GetEnvironmentVariable('LM_STUDIO_API_KEY','User')")" \
  python -u .agents/agent_script.py --model prism-ml/bonsai-27b --max-steps 80 --task-file <spec.md>   # run_in_background
```
Shell is OFF in one-shot mode (add `--auto` only for commands I have vetted; the git guard still refuses history
rewrites). Default MCP servers: filesystem + unrealclaude (all five = 194 tools, too many for it: `--servers all`
only when needed). Full trace: `.agents/runs/<time>.md`.

## 5. Verify - always, myself
Trust the report FILE lines and the project state, NOT the model's summary (field test: correct work, sloppy summary -
listed 2 of 7 builds). Check outputs exist, match expected numbers, and that nothing outside the task changed.
Failure pattern to expect: when its input is cut or unexpected, it loops in repeated reasoning instead of stopping -
kill the run, fix the spec/harness, retry once; second failure -> do the step myself.
