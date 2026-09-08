---
name: CHALK HUD approved design and next-session work
description: User approved A/Quiet survival plus top compass, wants native editable GIMP layers, and deferred implementation to next session after asset review and an implementation plan.
type: project
---

2026-09-07: User said the latest HUD looks good; **implementation is for the next session**. Begin then by reviewing existing project assets and developing the implementation plan. Do not re-propose A/B/C or begin implementation during the wrap-up.

Read the full handoff first: **C:/UnrealEngine/Games/AZ/docs/design-briefs/hud-design-next-session.md**.

Approved visual source: **C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/CHALK_HUD_v03_NATIVE.xcf**. A = compact bottom-right health/weapon/ammo, no permanent I hint, temporary cross-shaped quick selector, slim top compass with tracked target/distance. CHALK's civilian survival-horror tone and inventory Oswald/Roboto + warm neutral/peach palette; future RPG consumables/crafting depth primarily in inventory and quick access. Counts/items/effects in the mockup are illustrative; exact input/slots/target visibility rules remain planning decisions.

Explicit authoring requirement: native GIMP text layers and vector shapes/paths on separate named toggleable layers; painted effects separate; generated artwork stays as image. v03 saved-file readback verified 28 text / 101 vector+paths / 3 paint / 1 image. Older comparison boards are flattened previews, not the editable source.

Reuse research already done: ProHUDV2_Horror has WB_Compass_H/WB_CompassMarker_H, HUD manager target API and pickup/interaction widgets; HQUI_ProgressBars has configurable linear/circular bars. Full exact asset and API evidence is in the handoff and v03 README. Revalidate live, but do not rediscover the whole packs from scratch. Bind combat health to AZ_VitalsAttributeSet, not legacy Hero health. Equipment/QuickBar owns weapon state; individual magazines retain their identities. No Unreal code/assets changed for HUD; no tests/PIE run. GIMP MCP was off, so native GIMP batch was used; exact fonts/config and source scripts are retained.
