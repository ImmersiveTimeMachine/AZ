---
name: Implementation handoffs to Claude Opus 5
description: User identifies their implementing agent as Claude Opus 5; tailor copy-ready implementation instructions accordingly.
type: feedback
---

On 2026-09-14 the user explicitly asked to remember that the agent receiving our implementation instructions is **Claude Opus 5** (the user's model designation). They welcome adjustments that make these prompts more efficient.

In this collaboration, the user often asks us to inspect code/assets/logs, reason about design, and write instructions or answers for that separate implementing agent. A request to produce a handoff does not itself mean we should implement the gameplay change. Follow the current request if the user later asks us to implement directly.

Write self-contained, copy-ready handoffs that:

- Lead with the exact objective and current priority.
- Distinguish user-approved decisions, measured evidence, hypotheses and open checks.
- Reference current absolute source/asset paths and existing mechanisms to reuse; avoid stale architecture or restarting completed work.
- State observable behavior, ownership/handoff contracts, important failure paths, scope limits and completion criteria.
- Allow routine implementation choices instead of overprescribing every line or repeating generic Unreal tutorials.
- Keep instructions concise, remove repeated warnings, and put critical constraints where they affect the work.
- Preserve project rules: player animation at 1x where applicable; no new automated tests without an explicit request; ask before starting PIE/editor tests, normally let the user test and inspect the logs afterward.

These are clarity/context improvements, not verified model-specific tricks. Do not invent capabilities or special prompting requirements for the named model. Consult current primary documentation only if a future task requires model-specific factual advice.
