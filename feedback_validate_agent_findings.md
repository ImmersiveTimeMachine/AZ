---
name: feedback_validate_agent_findings
description: Always double-check agent findings against actual visual inspection before acting on them
type: feedback
---

Always validate agent research findings against actual editor state before implementing changes.

**Why:** During the Get_DynamicPlayRate build, one agent incorrectly stated GASP didn't have angular velocity nodes. A second agent confirmed they DO exist. Acting on the first agent's incorrect finding caused unnecessary work. The user caught this by searching in the editor.

**How to apply:**
- When an agent says "X doesn't exist", verify visually before removing code
- When agents disagree, trust the one that queried actual node data over the one making assumptions
- Always have the user confirm visual state before destructive changes
- Use multiple validation agents, not just one
- If unsure, query the actual blueprint nodes via MCP rather than relying on agent memory
