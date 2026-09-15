---
name: feedback_held_input_retries_ability
description: "★★ AbilityInputTagHeld re-activates any INACTIVE matching spec every frame, so an ability that ends immediately (Jump) is silently re-run while the key is held — de-facto input buffering, contaminated logs, and a policy the user never chose. Excluded per-tag in AZ_PlayerController, never globally."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-15T01:18:40.632Z
---

# A held key re-activates any ability that has already ended

`AAZ_PlayerController::AbilityInputTagHeld` forwards every held frame to
`UAZ_AbilitySystemComponent::AbilityInputTagHeld`, whose loop is
`if (!AbilitySpec.IsActive() || WantsRetriggerWhileActive(...)) TryActivateAbility(...)`.

So **any ability that ends quickly is re-activated on every held frame.** That is intended for melee (a held
LMB re-punches when the recovery window opens) and wrong for anything meant to be one-press-one-decision.

`UAZ_GA_PawnJump` ends immediately after `Started` or `BodyBusy`, so its spec was inactive again the next
frame: holding Space re-ran the whole traversal query every tick until something took.

## Symptoms that should trigger this suspicion

- Rejection counts far exceeding plausible presses — **109 `BodyBusy` against 20 traversals** in one
  session, and repeated byte-identical rejection lines at the same position (`dist=30` twice in a row).
- A feature behaving as if it had input buffering that was never implemented. The user had explicitly
  chosen "no delayed traversal"; held-retry gave them the buffering they declined, invisibly.
- Any statistic gathered from those logs is contaminated — one press is counted many times.

## The fix

Exclude the tag in the controller's `AbilityInputTagHeld` early-out, alongside Aim / PrimaryAttack /
Reload / Crouch. **Do not** disable held input globally; other abilities depend on it.

Hold-to-jump-higher is unaffected by the exclusion: it rides `AbilityInputTagReleased` →
`UAbilityTask_WaitInputRelease`, a different path from held re-activation.

## General rule

When an input policy is specified ("one press, no deferral"), the specification is not enforced by the
ability alone — the input stack decides whether the ability is even asked. Check the Pressed/Held/Released
routing before concluding a policy is implemented. Found by an external code review, not by reading the
logs, even though the logs had shown the evidence for two sessions. See [[project_traversal_2026-09-15]].
