---
name: feedback_check_memory_first
description: Always read MEMORY.md and relevant memory files BEFORE spawning research agents or asking the user clarifying questions
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
When the user asks anything that might already have been researched and documented, **read the relevant memory files first**. Do not spawn Explore/research agents and do not ask the user for context that's already in memory.

**Why:** During this conversation I spawned 4 parallel Explore agents to analyze GASP's input/rotation/TIP pipeline — but `gasp_character_movement.md`, `gasp_animbp_architecture.md`, and `gasp_update_logic_flow.md` already documented exactly what I needed (e.g. `Get_OrientationIntent` per-mode behavior at lines 59-63, including the "idle keeps last orientation in OrientToMovement/Strafe; Aim updates at delta > 60°" rule). The user called this out as redundant work and as me "walking around with the same solution."

**How to apply:**
1. Before any research action (Agent spawn, web search, codebase exploration), scan `MEMORY.md` for entries that match the topic.
2. Read the matching memory files in parallel.
3. Synthesize from memory FIRST. Only after that, identify the specific gap that requires fresh research, and scope new investigation tightly to that gap.
4. If memory contradicts the current symptom, verify against current code (memories can be stale) — but use the memory as the starting hypothesis, not a blank slate.
5. When citing memory findings, mention the file (e.g. "per `gasp_character_movement.md:59-63`") so the user can audit.

This applies especially to GASP/AZ comparisons since `MEMORY.md` lists 8+ GASP reference files covering character, animbp, c++, posesearch, choosers, data model, project settings, anim notifies, and update flow.
