---
name: feedback_use_agent_teams
description: Always use teams of agents for research and complex tasks, distribute work in parallel
type: feedback
---

Always create teams of agents (not just one) when doing research or complex tasks. Distribute work across multiple agents running in parallel.

**Why:** Single agents miss edge cases. Multiple agents cross-validate findings, cover more ground, and find patterns that one agent alone would miss. The team approach proved extremely effective for Chooser API research (4 agents) and BP node creation tools (8 agents).

**How to apply:**
- For research tasks: launch 3-8 agents covering different angles (engine source, web docs, community forums, reverse-engineering, validation)
- For implementation tasks: have some agents research while others validate or build specs
- Always launch agents in a single message (parallel) when they're independent
- Each agent should have a clear, distinct focus — no overlap
- Consolidate findings before implementing
- Use this approach proactively, not just when asked
