---
name: feedback_animgraph_node_reference_wiring
description: Any AnimGraph helper / SM transition rule that takes an FAnimNodeReference parameter (BlendStackNode, MotionMatchingNode, etc.) MUST have a K2Node_AnimNodeReference node wired to it. Unwired = invalid ref = library calls fail-closed and the rule silently never fires.
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Rule: AnimGraph helpers taking FAnimNodeReference need a Node Reference wire

**Rule:** Any time a transition rule or anim-graph helper function (built on AZ_AnimInstance, BlendStack, MotionMatching, etc.) accepts an `FAnimNodeReference` parameter, the BP graph MUST feed that pin from a `K2Node_AnimNodeReference` node whose `Tag` matches the actual AnimGraph node's title (e.g. "State Machine Blend Stack").

**Why:** When the FAnimNodeReference is invalid (pin unwired or wrong tag), the engine helper libraries fail-closed:
- `UBlendStackAnimNodeLibrary::IsCurrentAssetLooping(invalid)` returns its default (often `true`).
- `UBlendStackAnimNodeLibrary::GetCurrentAssetTimeRemaining(invalid)` returns `0`.
- `UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(invalid)` reports failure.

In `IsAnimationAlmostComplete(BlendStackNode)`:
```cpp
const bool  bLooping      = UBlendStackAnimNodeLibrary::IsCurrentAssetLooping(BSNode);
const float TimeRemaining = UBlendStackAnimNodeLibrary::GetCurrentAssetTimeRemaining(BSNode);
return !bLooping && TimeRemaining <= AnimationAlmostCompleteThreshold;
```
If the input ref is invalid → `bLooping = true` (default) → returns `false` forever → SM stuck in the source state. Real-world incident (2026-05-03): SM stuck in `Transition to Locomotion`, never advanced to `Locomotion Loop`. Diagnosis was the BlendStackNode pin in the T2L→LocoLoop transition rule was not wired to the "State Machine Blend Stack" Node Reference.

**How to apply:**
- When porting a BP graph (transition rule, OnStateEntry, OnUpdate) that calls a function with `FAnimNodeReference` input, IMMEDIATELY verify the rule graph contains a `K2Node_AnimNodeReference` whose `Tag` text matches the target AnimGraph node's title.
- The tag is editor-set on the AnimGraph node itself (Details panel → Tag field). Common tags in this project: `State Machine Blend Stack`, `Motion Matching`.
- The MCP `search_nodes` does NOT surface transition-rule sub-graphs in this project's UnrealClaude MCP — when in doubt, ASK the user to open the rule and confirm the wire, or have them paste a screenshot. Don't assume "the BP looks fine" from `search_nodes` returning 0 hits.
- This rule applies to: `IsAnimationAlmostComplete`, `SetBlendStackAnimFromChooser`, `ConvertToBlendStackNode`, `GetMotionMatchingSearchResult`, any custom helper in `AZ_AnimInstance` that takes `FAnimNodeReference`, and any BlendStack/MotionMatching library call.
- Quick fix recipe: in the rule graph, RMB → "Add Anim Node Reference" → set Tag to the target node's title. Wire its `Value` output into the helper's `BlendStackNode` (or equivalent) input. The target AnimGraph node must already have its Tag field populated to the same string.

**Related:** there is a related `BlendStackNodeTag` mismatch issue documented in earlier sessions — node displays "Blend Stack" in title but Tag is set to "State Machine Blend Stack" (or vice-versa). Always verify the actual `Tag` field, not just the visible title.
