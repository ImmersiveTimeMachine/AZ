---
name: feedback-mechanics-rpg-hooks
description: "Artur (2026-10-18): CHALK must have RPG elements - every mechanic's tunables are a base value read through one RPG/attribute layer, not hardcoded."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 0cf39d47-f3b0-4a50-9b24-845e8ec0506f
  modified: 2026-10-18T21:11:28.692Z
---

**Rule (Artur, 2026-10-18 17:15):** «не забываем, что у нас должны быть и элементы RPG». He said it while the CryEngine AZ mechanics were being built: traversal, Chalkie combat and grab, weapons, ragdoll.

**Why:** CHALK is a survival-horror RPG. The Unreal version has GAS HeroAttributeSet stats, plus inventory panels for skills, infection, mortality and currency. Mechanics tuned with hard constants would have to be retrofitted later.

**How to apply:**
- Keep each tuned constant as the BASE value and read a multiplier or modifier through one RPG layer.
- In CryEngine that layer is the planned AZ Ability System (CryEngine has no GAS). The design note is owned by the cryengine-gamesdk session and posted as Q-REVIEW in agents-chat.md; any module reads it through a header-only `AZ::Rpg::Value(key, base)`.
- Until the layer exists, keep the numbers in one named place per module and list them in the docs.
- Map attributes from the Unreal CHALK HeroAttributeSet first, before inventing new ones.
- Design first, then Artur's go.

Related: [[project_gas_gameplay]], [[project_hud_design]], [[feedback_aaa_design_first]].
