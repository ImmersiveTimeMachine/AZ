---
name: feedback_sm_transition_access
description: How to access SM transition rules — states have SPACES in names, must use state name INSIDE SM (not SM node name)
type: feedback
originSessionId: f6181671-d4a5-4b82-954f-4f2f5396f92f
---
SM transition rules in AZ_ABP_HeroPawn — correct access pattern.

**Why:** States in the "State Controller" SM have SPACES in names: "Idle Loop" not "IdleLoop". The `list_transitions` and `inspect_transition_rule` tools take an "IdentifyingStateName" parameter — this must be a state name INSIDE the SM (e.g. "Idle Loop"), NOT the SM node's name (e.g. "State Controller").

**How to apply:**
- Use `get_states_in_state_machine(ABP, "Idle Loop")` → returns 7 states: Idle Loop, Transition to Idle, Transition to Locomotion, Locomotion Loop, Transition to In Air, In Air Loop, Idle Break
- Use `list_transitions(ABP, "Idle Loop")` → returns 24 transitions with rule node counts
- Use `inspect_transition_rule(ABP, "Idle Loop", "Re-Enter ", "Transition to Idle")` → dumps rule graph nodes
- Note the trailing space in "Re-Enter " — some state names have quirky formatting
- Transition properties: `inspect_transition_properties(ABP, "Idle Loop", FromState, ToState, TransitionIndex)` shows bSharedRules, SharedRulesName, etc.
- NEVER pass the SM node name (like "State Controller") as the identifying state — it will always return empty results
