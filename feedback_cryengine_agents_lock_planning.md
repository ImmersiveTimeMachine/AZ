---
name: feedback-cryengine-agents-lock-planning
description: "Artur's rule (2026-10-18) for several agents in CryEngine AZ: plan the shared lock time out loud (how long you hold, when you hand off, when you want it back), hand work to other agents, never use another agent's editor/game."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 0cf39d47-f3b0-4a50-9b24-845e8ec0506f
  modified: 2026-10-19T04:57:00.713Z
---

**Rule (Artur, 2026-10-18 01:37, given to every agent working in C:/CryEngine/AZ):**
- Agents may hand work to each other.
- They must plan the shared time among themselves. Each agent says how many minutes it will hold the lock, after how many minutes it wants it back, and how long it will block others. Everyone must know the plan.
- Recorded in `C:/CryEngine/AZ/docs/implementation/agents-coordination.md`, section "Planning shared time and handoffs". Status line markers:
  - `PLAN` — what you will do, when, for how many minutes;
  - `QUEUE` — who goes after whom;
  - `EXTEND` — longer than planned, posted before the old time runs out;
  - `HANDOFF` / `BACK` — handing the lock over, and when you want it back;
  - `TASK` — handing work to another agent.

- Cross-review (Artur 01:53): any agent may hand its reasoning or plan to another agent for validation and an assessment, and must do such reviews when asked. The format is `Q<id> REVIEW <path>` → `A<id>` in agents-chat.md, with a verdict, concrete issues and what was not checked. A cross-review stands in for Artur's "yes" while he is away.
- Shared channel: agents-board.md (one row per agent) and agents-chat.md (Q/A ids), next to agents-coordination.md. Every agent re-reads them with a ~60 s timer.

**Why:** at 01:32 a weapon run inside Codex's own Sandbox switched it to game mode and broke its native import. A process left hanging after a headless export was not killed by anyone.

**How to apply:**
- Before a build, launch or editor use, read the lock and the last Status lines and post `PLAN` or `QUEUE` with minutes.
- Split long holds into blocks and release between them if someone is queued.
- Never drive another agent's Sandbox or game; start your own through Rider Debug so it can be stopped from Rider.
- The build itself must be gated by the lock's exit code: `agent_lock.py acquire ... && { post PLAN; cmake ...; }; release`.
  Never write `acquire ... && post; cmake ...`. On 2026-10-19 00:55 claude-traversal did exactly that: acquire said "busy"
  (claude-npc held a launch lock and a source hold), the PLAN post was skipped, and the build after `;` ran anyway. It replaced
  Game.dll under claude-npc's running test.
- Source holds (`agent_lock.py hold <agent> <min> <paths>`, from 2026-10-18) block builds of other agents. Read `.agent-hold`
  before editing or building shared code.

Related: [[feedback-cryengine-rider-debug-runs]], [[feedback_parallel_editor_edits]].

- 2026-10-21: agent_lock.py has a FIFO queue (.agent-queue). acquire --wait joins it automatically, so a free lock goes to the first queued agent; a missed turn is dropped after 3 min. Fast-polling Claude sessions used to starve codex, which polls every 30 s. Builds still go only through `agent_lock.py run`.
