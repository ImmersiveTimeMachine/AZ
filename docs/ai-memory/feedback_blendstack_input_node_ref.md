---
name: BlendStack helper functions take FAnimNode_BlendStackInput, NOT outer BlendStack
description: Critical port pitfall — UBlendStackAnimNodeLibrary helpers cast internally to FAnimNode_BlendStackInput (the per-anim inner template's input proxy). Going through ToBlendStackNode/ConvertToBlendStackNode/FBlendStackAnimNodeReference targets the OUTER FAnimNode_BlendStack and silently fails, causing all 5 helpers to return null at runtime. Pass the original FAnimNodeReference directly.
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
