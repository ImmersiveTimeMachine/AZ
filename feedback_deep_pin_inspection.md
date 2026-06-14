---
name: feedback_deep_pin_inspection
description: When BP functions don't work as expected, always compare pin metadata flags (bIsReference, bIsConst) not just names and types
type: feedback
---

When debugging BP function issues (like "function doesn't appear in dropdown" or "signature mismatch"), always inspect the FULL pin metadata, not just pin names and types.

**Why:** We missed that SM state binding requires pass-by-reference parameters. The pin names and types matched, but `bIsReference` was false on our pins and true on the working ones. This cost significant debugging time.

**How to apply:**
- When a function "should work but doesn't", compare pin flags: `bIsReference`, `bIsConst`, `bIsWeakPointer`, `ContainerType`
- Look at the PROTOTYPE function signature carefully — `const&` means pass-by-reference
- When validation agents compare functions, they should check pin metadata flags, not just names/types/connections
- Add pin flag inspection to `ListFunctionNodes` output for easier debugging
- When creating pins programmatically, always check if the target usage requires reference params (SM bindings, delegate signatures, etc.)
