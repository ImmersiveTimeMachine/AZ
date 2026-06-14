---
name: feedback_transition_rule_tools
description: How to properly create AnimGetter, ArrayContains, and rebuild transition rules programmatically
type: feedback
---

When building transition rule graphs programmatically, these patterns work:

**AnimGetter (Current State Time):**
- Use `AddTransitionRuleAnimGetter` — sets SourceNode (SM), GetterClass, SourceAnimBlueprint
- Must manually set `CachedTitle` using `FText::Format` with function display name + SM node name
- Must set `Contexts.Add("Transition")` before AllocateDefaultPins

**Array Contains (CONTAINS node):**
- Use `AddTransitionRuleArrayContains` — creates `UK2Node_CallArrayFunction` (NOT UK2Node_CallFunction)
- After connecting TargetArray pin, `NotifyPinConnectionListChanged` propagates the array type (FName, etc.)
- `ConnectTransitionRulePins` now auto-calls NotifyPinConnectionListChanged on both nodes

**Comparison operators (>, <):**
- Use `Greater_DoubleDouble` / `Less_DoubleDouble` as function names — these are concrete functions, not PromotableOperators
- Do NOT use `K2Node_PromotableOperator` or `K2Node_CommutativeAssociativeBinaryOperator` — they crash or create wildcard pins

**Boolean logic (AND, OR):**
- Use `BooleanAND` / `BooleanOR` function calls — these work reliably
- For 3+ inputs, chain: AND(A, B) -> AND(prev, C) or use separate OR nodes

**Shared transition rules:**
- When `bSharedRules=true`, BoundGraph may be null on non-owner transitions
- Use `RecreateTransitionRuleGraph` (calls UnshareRules) to get a private editable copy
- After unsharing, the duplicated graph may contain old broken nodes — clear and rebuild

**Why:** K2Node_PromotableOperator and K2Node_CommutativeAssociativeBinaryOperator need special editor initialization that our tools don't provide, causing crashes or wildcard pin issues. K2Node_CallFunction can't handle CustomThunk functions (Array_Contains) — need K2Node_CallArrayFunction with NotifyPinConnectionListChanged for type propagation.

**How to apply:** Always use the specific helper functions (AddTransitionRuleArrayContains, AddTransitionRuleAnimGetter) and concrete math functions (Greater_DoubleDouble) instead of generic node creation for these cases.
