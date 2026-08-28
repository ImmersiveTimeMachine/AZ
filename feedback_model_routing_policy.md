---
name: feedback_model_routing_policy
description: "★★ USER RULE (2026-08-27): which model runs which task — Opus 5 main loop, Sonnet 5 for mechanical/bulk/search subagents, Fable 5 only when Opus is provably struggling, Haiku effectively banned here (Feb 2025 cutoff predates UE 5.8). Includes the effort lever and the verify-anyway rule."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-27T21:56:46.476Z
---

# Model routing policy for the AZ project

**User, 2026-08-27:** *"create some rules what type of model should we use... this will optimise our work
and increase the performance and quality"*.

**Why:** token cost and latency are dominated by which model runs the bulk work, and subagent tokens never
enter the main context — so routing is the single biggest lever on both speed and cost. But capability
mismatches cost far more than they save: one wrong root cause on this project has repeatedly burned a whole
evening. The policy is therefore *cheap where the work is mechanical, expensive where it is judgement*.

## The lineup (Anthropic docs, verified 2026-08-27)

| Model | Latency | $/MTok in → out | Context | Reliable knowledge cutoff |
|---|---|---|---|---|
| Claude Fable 5 `claude-fable-5` | Slower | 10 → **50** | 1M | Jan 2026 |
| Claude Opus 5 `claude-opus-5` | Moderate | 5 → **25** | 1M | May 2026 |
| Claude Sonnet 5 `claude-sonnet-5` | Fast | 2 → **10** | 1M | Jan 2026 |
| Claude Haiku 4.5 `claude-haiku-4-5` | Fastest | 1 → **5** | 200K | **Feb 2025** |

Output tokens dominate agent work: **Opus = 2.5× Sonnet, Fable = 5× Sonnet.**

## ★ How to apply it

**Route by JUDGEMENT DENSITY, not by how big the task looks.** A 900-line mechanical edit is Sonnet work;
a three-line change that decides an architecture is Opus work.

| Task type | Model | Where |
|---|---|---|
| Root-cause diagnosis, cross-system reasoning, "why is this wrong" | **Opus 5** | main loop |
| Architecture / design decisions, choosing between approaches | **Opus 5** | main loop |
| Reading engine source to settle a fact; falsifying a hypothesis | **Opus 5** | main loop |
| Codebase search, reference sweeps, "find every usage of X" | **Sonnet 5** | subagent |
| Dead-code / asset audits, inventory, producing a candidate list | **Sonnet 5** | subagent |
| Bulk mechanical edits from a CONFIRMED list (deletes, renames, applying a spec) | **Sonnet 5** | subagent |
| Boilerplate, docs, changelogs, test scaffolding | **Sonnet 5** | subagent |
| Commit, push, build invocation, log tailing, file moves | **no subagent** | main loop, direct |
| Adversarial review of a risky change | **Opus 5** | subagent, fresh context |
| Anything where Opus has demonstrably failed twice | **Fable 5** | main loop |

## Hard rules

1. **Haiku 4.5 is effectively banned on this project.** Its reliable knowledge cutoff is **Feb 2025** —
   before UE 5.8 exists. It will confidently produce UE 5.4-era APIs. Its 200K context also cannot hold our
   larger files. Use it only for text with zero engine content, and even then prefer Sonnet.
2. **Do not spawn a subagent for a two-line edit or a `git commit`.** Cold-start costs more than it saves.
   Subagents earn their keep when the work is (a) verbose output you do not want in main context,
   (b) parallelisable, or (c) genuinely self-contained.
3. **Tune `effort` before switching models.** The docs are explicit that effort is often the better lever.
   Opus 5 defaults to `high`; step to `xhigh` for the hardest agentic work, drop to `low` for mechanical
   stages. Haiku does not support the parameter at all.
4. **Fable 5 is not the default "better" button.** The stated rule of thumb: reach for Fable only when
   evaluation shows **Opus struggling**. If Opus clears the bar, its speed and price win. At 5× Sonnet
   output it is the most expensive thing we can do.
5. ★ **Model choice NEVER substitutes for verification.** In the session that produced this file a subagent
   confidently reported the wrong root cause (the normalization set) and it was falsified by one direct
   read. A more expensive model would not have made its report trustworthy. [[feedback_verify_never_presume]]
   applies to every model equally: relaying a subagent claim without re-reading the authoritative source is
   still presuming.
6. **Pick the tool before the model.** Rider's `get_file_problems` / `safe_delete --preview` answer
   "is this dead?" authoritatively and Blueprint-aware; no model of any size beats the IDE index at that.
   See [[reference_rider_mcp_new_tools]].
7. **`subagent_type: "fork"` ignores the `model` override** — a fork always runs the parent's model and
   inherits full context. Use a fresh agent when you actually want a cheaper model.

## The two multi-model patterns we use

- **Orchestrator → workers** (the default here): Opus main loop holds the plan and the judgement; Sonnet
  subagents do the sweeps and the bulk edits and return a table, not file dumps. Most tokens bill at the
  Sonnet rate and the main context stays clean.
- **Executor → advisor** (escalation): Sonnet does the work and escalates a specific hard decision back to
  Opus. Useful for long mechanical runs with occasional judgement calls.

## Worked example — this project, 2026-08-27

- CMC locomotion root cause (frame-skew diagnosis, engine tick order, ownership-vs-grace design) → **Opus,
  main loop.** Required falsifying three plausible hypotheses and reading engine source.
- "Find every unused UPROPERTY across 284 declarations, cross-check C++ + asset bytes" → **Sonnet
  subagents, in parallel.** Mechanical, verbose, self-contained.
- Deleting 2 verified-orphan PSDs, the commit, the push → **main loop, no subagent.**

Related: [[feedback_verify_never_presume]], [[reference_rider_mcp_new_tools]],
[[project_cmc_input_gap_doctrine]], skill `agent-and-research-discipline`.
