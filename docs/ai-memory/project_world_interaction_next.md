# Next major block: world-object interaction

LATEST PRIORITY clarification: finish the incomplete throwable system FIRST, then world-object interaction design. Two separate lists: `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-completion-backlog.md` and `C:/UnrealEngine/Games/AZ/docs/design-briefs/world-interaction-catalog.md`. Do not start environment implementation ahead of throwable completion. User is selecting work order; next step is current throwable audit and scoped completion plan.

User decision, September 22, 2026: next major block is interaction with environment objects. Design the player experience and system first; implementation follows a considered plan. Do not automatically begin implementing it from this reminder.

Broad researched candidate catalog prepared at `C:/UnrealEngine/Games/AZ/docs/design-briefs/world-interaction-catalog.md`: 22 interaction families, reference sources, suggested first slice and shared design questions. This is a selection catalog, not approved implementation scope.

User explicitly separates THROWABLE EXPANSION from this environment-interaction catalog. Future throwables should extend the existing grenade system: grenade, stone, ordinary glass bottle, incendiary bottle; consider game-only crafting for the latter. Define supported object types/behaviors and a finite initial list, rather than automatically making every world mesh throwable. Existing stone-related code may already exist; audit before claiming it needs entirely new implementation. No new throwable implementation authorized in this planning exchange.

Requested examples:
- Open doors.
- Open cupboards/drawers or other storage furniture (user described shelves).
- Take an object from a shelf.
- Pick up a picture or another inspectable object, hold it and rotate it for examination.

Design questions for the future block (not yet approved mechanics): targeting and shared action prompts; open/close/use/pickup versus inspect; hand/camera presentation; mouse and controller rotation and navigation; return versus inventory transfer; interruption/cancel; state persistence and quest events. Audit existing interaction, inventory and Menu/InventorySystemPro content before adding parallel systems. User examples are scope direction, not evidence that any of these mechanics is already implemented.

## Deferred melee/destruction refinement

User confirms melee destruction works but is awkward because the existing near-wall/full-body clearance restrictions make it difficult to approach and strike props. Leave that logic unchanged now. Later distinguish legitimate destructible-prop contact from real wall/body obstruction, preserving capsule clearance, real contact timing, attack cancellation and no through-wall damage. Revisit distances/animation fit and fist/kick feel together; do not simply disable wall protection.

User also confirms grenade debris behavior is now satisfactory after reducing added radial speed from 1500 to 300 cm/s. Detailed implementation/build state: `C:/UnrealEngine/Games/AZ/docs/design-briefs/nextgen-destruction-prototype-status.md`. Latest reduced-speed tuning was applied by Live Coding; ordinary closed-editor build is needed for durable base DLL on future restart unless subsequently completed.
