---
name: BlendStack helper functions take FAnimNode_BlendStackInput, NOT outer BlendStack
description: Two BlendStack inner-vs-outer pitfalls. (1) UBlendStackAnimNodeLibrary helpers cast internally to FAnimNode_BlendStackInput — pass the original FAnimNodeReference, never ConvertToBlendStackNode, or all 5 getters return null. (2) THE INVERSE GRAPH BUG (2026-06-06): a FULL Blend Stack node nested inside the sample graph (instead of Blend Stack Input) silently hijacks all visible playback to frame 0 — StartTime and MM SelectedTime are ignored because they feed the bypassed outer node.
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# BlendStack helpers want the inner Input proxy, not the outer BlendStack ref

## Rule
When a BP wires `State Machine Blend Stack Input` (or any `K2Node_AnimNodeReference` for a `FAnimNode_BlendStackInput`) into a function call like `Get Current Blend Stack Anim Asset` / `Get Current Blend Stack Anim Asset Time` / `Get Current Blend Stack Anim Is Active`, the C++ wrapper must pass that `FAnimNodeReference` **directly** to the engine library:

```cpp
// CORRECT
UAnimationAsset* Asset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(Node);
float Time             = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(Node);
bool bActive           = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(Node);
```

NOT through any conversion helper:

```cpp
// WRONG — silently returns null/false at runtime
const FBlendStackAnimNodeReference BSNode = UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(Node, Result);
UAnimationAsset* Asset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(BSNode);
```

**Why:** Engine source `UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset` (and the related four helpers) does:

```cpp
if (FAnimNode_BlendStackInput* BlendStackInput = Node.GetAnimNodePtr<FAnimNode_BlendStackInput>())
{
    if (BlendStackInput->Player && *BlendStackInput->Player)
        return (*BlendStackInput->Player)->GetAnimationAsset();
}
return nullptr;
```

It casts to `FAnimNode_BlendStackInput`. `ConvertToBlendStackNode` produces a `FBlendStackAnimNodeReference` typed for `FAnimNode_BlendStack` (the **outer** node). When the source is the inner Input proxy the conversion type mismatches, the resulting reference has no valid pointer of the requested type, and the cast back fails. Returns null silently.

**How to apply:** In any AZ AnimInstance helper that wraps a BlendStack library call from BP, drop the `ToBlendStackNode`/`ConvertToBlendStackNode` conversion and pass the original `FAnimNodeReference& Node` straight through. If you genuinely need an outer-BlendStack reference (e.g., for `BlendTo`/`UpdateRequest`), then keep the conversion — but library *getters* never want it.

## Symptom catalog (so the next failure is recognized fast)
- `Get_DesiredFacing` returning `FQuat::Identity` despite a healthy `Trajectory` (because `GetBlendStackAnimAndTime` returned null Anim → early return Identity).
- `Get_DynamicPlayRate` returning `1.0` constant.
- `Get_StrideWarpAlpha` / `Get_StrafeWarpAlpha` returning `0.0`.
- `EnableSteering` returning false unless the `BlendStackInputs.bLoop || ...` fallback masks it.
- Steering node visually inert (no spring damper correction) → OFR-Accumulate offset never recovers → mesh visually decoupled from capsule rotation.

## Origin incident
2026-05-04, branch `feature/rootmotion`. The OFR-Accumulate-mode 90° lag was misdiagnosed for hours as missing GASP curves / missing OW+Steering nodes / missing motion matching, until on-screen runtime diagnostics in `UAZ_AnimInstance` showed `Steer[*] Anim=<null>` and `TargetYaw=0`. Fix landed at `AZ_AnimInstance.cpp::GetBlendStackAnimAndTime` and `EnableSteering`.

## Second incident (2026-06-06) — a FULL Blend Stack node INSIDE the sample graph
The inverse mistake, in `AZ_ABP_MoverAnimInstance`: the outer Blend Stack's per-sample inner graph hosted **another FULL `Blend Stack` node** instead of `Blend Stack Input`. Effect: each sample instanced a fresh inner stack that played the bound Anim from **its own literal AnimationTime=0** — the visible pose ignored every chooser StartTime and MM SelectedTime (those fed the bypassed OUTER node, whose own player stays frozen at its push time and renders to nobody). Cost two days of phantom debugging ("mid-air pushes freeze", "MM ignored", "replay from 0") — full story in [[project_jump_system_status]].

**Rule:** inside a Blend Stack sample graph, the ONLY pose source is `Blend Stack Input`. A full Blend Stack there still "works" visually (clips play from 0), which is exactly what makes it deadly — everything looks alive while every timing input is dead.

**Detection recipe (definitive):** temporary UE_LOG in engine `FAnimNode_BlendStack::ConditionalBlendTo` printing requested asset/time, playing asset/time, exec flag, player count. Read it as streams:
- **Multiple interleaved streams per frame = multiple node-state instances** (one per full Blend Stack node actually updated).
- A stream **born `playing=none players=0` at every transition edge** and dying one blend-duration later = a per-sample inner-graph instance.
- A stream pinned at the exact written StartTime that never advances = the bypassed outer node.
- `players=N` is stack depth during a cross-fade (2 while fading, 1 after), NOT instance count.

**10-second editor check:** double-click the Blend Stack node → sample graph must be `Blend Stack Input` → (chain) → output; Ctrl+F the ABP for "Blend Stack" — more than one full node = the bug.
