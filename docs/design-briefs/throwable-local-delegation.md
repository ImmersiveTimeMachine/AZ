# Throwable completion — local delegation plan

User requests consideration of the existing LM Studio executor to reduce remote token use. Use C:/UnrealEngine/Games/AZ/.agents/agent_script.py; do not replace it. Applied workflow: C:/UnrealEngine/Games/AZ/.claude/skills/local-model-delegation/SKILL.md.

## Current preflight

LM Studio endpoint responds HTTP401 (authentication required). Loaded prism-ml/bonsai-27b, IDLE, context32768, parallel1. Initial user-scope LM_STUDIO_API_KEY was missing. Subsequently configured by user authorization; authenticated models request succeeded, and first filesystem-only task was launched. Do not print/write the secret; do not bypass authentication, change model settings or reload the model. Resolve access before dispatch.

## Division of work

User additionally authorizes choosing lower-capability available models where appropriate. Root decides task allocation without repeatedly asking. After one real work session compare local vs smaller hosted vs root execution using observed task outcomes, elapsed time, reported token use where available, supervision/context overhead and repair effort. Do not assume local or smaller always wins. Do not run a costly artificial benchmark or duplicate already completed work merely to populate a comparison. Account-wide usage is not an exact per-task cost; unavailable monetary data stays unavailable. Current configured model family and actual billing must not be inferred from naming alone.

| Step | Root responsibility | Local executor responsibility |
|---|---|---|
| Existing code | Architecture, invariants, actual failure diagnosis | Exact field/function inventory in bounded files |
| Art/data | Pick appropriate assets, inspect grip/scale, define required results | Enumerate candidate files and metadata, classify from explicit naming rules |
| C++ | Item ownership, atomic crafting, throw state machine, fire/status effects, save safety and AI awareness | Later mechanical check against an explicit checklist; no design or unreviewed edits |
| Asset authoring | Prepare deterministic script/table, own editor write scheduling and rollback | Run approved exact script only after preflight; no concurrent editor mutations |
| Validation | Verify source, saved files, compiler logs and actual behavior | Gather specified report data; never treat its summary as proof |

First low-risk task: C:/UnrealEngine/Games/AZ/docs/agent-tasks/throwable-field-inventory.md. Run with filesystem server only, no --auto, max8 tool rounds; capture report and verify. No precise percentage saving promised: short work may cost more to delegate than to perform directly. Expand responsibility only after correct results.

## Implementation priorities from latest user request

Crafted incendiary game item is required, including separate collectible resources, ignition, throw, shatter and burning enemies. Stone and ordinary bottle share the grenade's existing throw pipeline. Plan schema/header changes together to minimize editor restarts. Basic crafting uses existing inventory transactions and feedback UI; full RPG crafting economy remains out of scope.

Proposed defaults for discussion: an ignition tool is reusable; crafting components are consumed atomically only when output placement succeeds; crafting and ignition are separate actions. Empty glass bottles can be thrown or used as a crafting resource. Use abstract game recipe data, never real manufacturing instructions. Enemy ignition must be designed beyond the old ground-zone-only draft; duration, extinguishing, repeat applications and visuals need one coherent state owner.


First real bounded comparison completed: local launcher authenticated successfully, Bonsai read exactly three requested headers in 3 tool calls, 45 seconds, filesystem-only. Trace .agents/runs/20260925-213947.md. Report correctly listed impact enum and lack of shatter/fire declarations in inspected files, but incorrectly reported numerous explicit numeric initializers as absent and omitted several requested projectile recovery declarations. Root verified against source. Smaller hosted gpt-6-luna report correctly extracted the requested numeric defaults and fuller recovery declarations. Do not infer universal superiority or exact token/cost savings from one task; comparable usage metrics unavailable. Prefer deterministic execution/structured extraction for local, review-heavy code interpretation stays with root/hosted when worthwhile. No gameplay/asset changes in this trial. Credentials are read from environment, never embedded in launcher source or task files.
