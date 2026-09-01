---
name: feedback_inform_before_proposing
description: "★★★ USER RULE (2026-09-01): INFORM BEFORE PROPOSING. Before proposing or building a solution: (1) grep MEMORY.md for the domain and READ every ★ file it names — 'read before X' is an instruction; (2) inventory existing AZ_*Utils UFUNCTIONs before writing a new one; (3) never claim 'needs a manual edit / cannot be scripted' without grepping the utils headers; (4) before replacing authored content, read the authoring API's doc comment and find the real discriminator (tags, not display names); (5) before changing a tuned value, find why it was tuned."
metadata:
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-01T04:16:13.877Z
---

# Inform yourself BEFORE proposing a solution

**User, 2026-09-01:** *"how we solve it already, we have motionwarping and so on so lets check our
knowledge from what we learned to be sure we dont make a regression"* — then, after that check exposed
two errors: *"make some rules to inform yourself before proposing a solution."*

The user's single question caught more than an hour of my work heading the wrong way. Every fact needed
was already written down before I started.

## What went wrong (one session, four instances)

| # | I did | What was already known |
|---|---|---|
| 1 | Proposed PSIA pairing, then warp-window surgery, for "punch goes through body" | `MEMORY.md` indexes `project_motion_warping.md` as "★ **Read before warp/montage-reaction work**". I read it only when the USER asked. |
| 2 | Added `AddGameplayEventNotify` to `UAZ_PoseSearchUtils` + burned a closed-editor build | `AZ_MontageUtils::AddGameplayEventNotify(Montage, **FName** EventTagName, ...)` already existed — taking an FName, exactly as that file's gotcha 2 prescribes |
| 3 | Told the user the warp window "needs a manual editor drag — I can't script it" | `AZ_MontageUtils::AddMotionWarpingNotify` / `RemoveMotionWarpingNotifies` are listed in that same file |
| 4 | Hypothesised "2 melee windows = leftover duplicates", was about to delete one | They are hit (`Event.Montage.Melee.Window*`) + combo-cancel (`Event.Combat.Cancel*`). The authoring function's own doc says they coexist. |
| 5 | Was about to re-tune `WarpApproachDistance`/`RootMotionSeconds` on a clip | The 2026-08-02 verdict already read: `Heavy2Idle` is "an excellent warping test but a **poor primary attack** … give it its own input/ability at range" |

Common root: **I treated my own measurements as the starting point instead of as a check on recorded
knowledge.** Measuring is not a substitute for reading — measurement told me *what* was happening, the
memory told me *what had already been decided about it*.

## The rules

1. **Domain-grep memory BEFORE the first proposal, not after the first failure.** `grep -il <domain>`
   over `memory/`, and READ every file `MEMORY.md` marks ★ for it. When the index says "read before X
   work", that is an instruction with a trigger condition — treat encountering X as the trigger.
2. **Never write a utility before inventorying the existing ones.**
   `grep -n "UFUNCTION" -A2 Source/AZ/Public/Animation/AZ_*Utils.h | grep static` costs one call.
   Six libraries exist (see the `az-cpp-utility-tools` skill); notify/montage authoring lives in
   **`AZ_MontageUtils`**, NOT `AZ_PoseSearchUtils`. A duplicate function is worse than none: two
   implementations drift, and here it also cost a closed-editor build (editor close + build + reopen +
   user's time).
3. **"It can't be scripted" is a claim requiring evidence.** Before telling the user something needs a
   manual editor step, grep the utils headers for the capability. Being wrong here spends the user's
   hands on work already automated.
4. **Before replacing or deleting authored content, find the DISCRIMINATOR.** Read the authoring
   function's doc comment first — it documents what may legitimately coexist. Dumps that print display
   names hide tag-level differences (`DumpMontageNotifies` prints "AZ Melee Window" for both a hit and a
   cancel window). Verify with a source that shows the discriminator (grep the .uasset for tag names),
   and prefer tag-scoped replace (`bReplaceExisting`) over delete-all + re-add.
5. **A tuned value has a reason; find it before changing it.** Numbers like `WarpApproachDistance=95`,
   `RootMotionSeconds=1.7`, `GrabHoldDistance=92` were each set by a past incident. Grep memory and the
   property's own comment for the incident before re-tuning, or you re-introduce the bug it fixed.
6. **When the user asks "won't this be a regression?" — stop and read.** That question has now paid for
   itself; treat it as a hard stop, never as something to answer from reasoning.

Related: [[feedback_verify_never_presume]], [[feedback_stop_the_patch_loop]], [[feedback_aaa_design_first]],
[[project_motion_warping]], [[reference_punch_reaction_content_inventory]].
