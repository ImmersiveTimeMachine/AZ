---
name: agent-and-research-discipline
description: Workflow rules for research and Task-agent usage in the AZ project — check memory before spawning agents, launch teams of 3-8 in parallel for non-trivial research, validate agent findings against editor state before destructive changes, never ask for read approval, always use full absolute paths in output.
---

# Agent & Research Discipline (AZ project)

Five rules. Apply in order.

## 1. Check memory FIRST — before any research action

Before spawning any Task/Explore agent, before web search, before asking the user clarifying questions:

1. Scan `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\MEMORY.md` for matching topic.
2. Read matching memory files in parallel (one message, multiple `Read` calls).
3. Synthesize from memory FIRST. Then identify the specific gap and scope new investigation tightly to it.
4. Memory may be stale — verify against current code, but use memory as starting hypothesis, not blank slate.
5. Cite findings by file (e.g. "per `gasp_character_movement.md:59-63`") so the user can audit.

Especially for GASP/AZ comparisons — `MEMORY.md` indexes 16+ GASP reference files. Re-researching what's documented is the most-called-out anti-pattern in this project.

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_check_memory_first.md` for the full rule and incident.

## 2. Use teams of 3-8 agents in parallel for non-trivial research

Single agents miss edge cases; teams cross-validate.

- Research: 3-8 agents covering distinct angles (engine source, web docs, forums, reverse-engineering, validation). No overlap.
- Implementation: some agents research while others validate or build specs.
- Always launch in a single message (parallel tool calls) when independent.
- Consolidate findings before implementing.
- Use proactively, not only when asked.

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_use_agent_teams.md`.

## 3. Validate agent findings before destructive changes

- Agent says "X doesn't exist" → verify visually (MCP `unreal_blueprint_query`, screenshot from user, file Read) before removing code.
- Agents disagree → trust the one that queried actual node data over the one inferring from memory.
- Confirm visual state with the user before destructive changes (delete, rename, refactor).
- If unsure, query live BP nodes via `mcp__unrealclaude__unreal_blueprint_query` rather than relying on agent recall.

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_validate_agent_findings.md`.

## 4. Never ask for read/glob/grep approval

Just do it. `Read`, `Glob`, `Grep`, `Task` (Explore), `mcp__unrealclaude__unreal_blueprint_query`, `unreal_status` — all read-only — proceed directly.

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_no_read_approval.md`.

## 5. Always use full absolute paths in user-visible output

Every file mention in chat output uses the full `C:\UnrealEngine\...` path. No relatives, no shortenings. The user clicks paths in Rider IDE — only absolute paths are clickable.

Applies to casual mentions too ("Updated the header" → "Updated `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h`").

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_file_paths.md`.

## When to save NEW memory

Save when you have: a workflow rule we wish past Claude had known; a non-obvious fix (root cause + symptom + resolution); an architectural decision with rationale.

Do NOT save: code we just looked at; one-off facts a future Read will surface; session play-by-play.
