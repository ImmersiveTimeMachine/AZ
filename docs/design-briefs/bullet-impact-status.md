# Bullet impact smoke — implemented, built and saved; user gameplay check pending

User requested an existing bullet impact smoke particle effect after the rifle reticle work. This addition uses the accepted authoritative firearm hit. It does not change HUD/inventory appearance, weapon damage, ammunition, animation, or aiming.

## Selected asset

`/Game/MilitaryWeapDark/FX/P_Impact_Stone_Small_01` is a Cascade ParticleSystem already installed with the weapon content. Its neutral grey smoke lasts approximately 0.44–0.67 seconds, with small rock flecks lasting 0.2–0.25 seconds. Both emitters have one loop, zero continuous spawn rate, and a single burst. The system auto-deactivates. No external parameters are required.

Use scale 1 initially. The effect emits along local +X; orient that axis to the authoritative impact normal. Live asset exports, exact lifecycle/space readback and comparison findings are in C:/UnrealEngine/Games/AZ/Saved/BulletImpact/asset-recommendation.md. Inspection included the genuine cached thumbnail, not a gameplay render. The existing Niagara alternatives were either sparks/flash effects or required batched multi-hit arrays.

## Source implemented

- C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h adds optional `UParticleSystem* WorldImpactEffect` and uniform `WorldImpactScale` (default 1). Null effect or zero scale skips this cosmetic. No hardcoded content paths in C++.
- C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_FirearmFire.cpp selects the effect only for a valid blocking scenery hit, excluding initial penetration, Pawn actors/ECC_Pawn components and invalid point/normal. It snapshots the reference and scale before TryConsumeWeaponRound, because inventory publication may synchronously change equipment or its manifest. Damage callbacks may also destroy the target, so classification happens before damage.
- C:/UnrealEngine/Games/AZ/Source/AZ/Public/Weapon/AZ_Weapon.h and Private/Weapon/AZ_Weapon.cpp carry the accepted effect/scale in the existing unreliable multicast. Impact playback precedes hit-marker callbacks and muzzle checks, skips dedicated servers, uses a 1 cm outward offset and `Normal.Rotation()`, and spawns with automatic pooling/release.
- Impact is independent of damage confirmation. Misses, rejected shots and character hits do not emit stone dust. The existing hit-confirmation X, audio, muzzle flash, damage, hearing and ammo transaction are preserved.
- This is a configurable default scenery response per weapon. Flesh responses and material-specific glass/metal/etc. mapping are separate future work. The existing physical-material trace data remains available for that extension.

## Validation and remaining work

Independent source review found no blocking issues. RPC declaration/definition/call agree, and the exact Cascade spawn signature, pooling behavior and transform application were checked against the local engine source. Whitespace checks passed. No automated tests added or run; no PIE started.

The user completed the full build/restart. UnrealBuildTool reports `Result: Succeeded`, 22.24 seconds. The DLL is newer than all four impact source files; the editor process started afterward, and live reflection accepts the ParticleSystem WorldImpactEffect and float WorldImpactScale fields. No C++ edits followed this build.

C:/UnrealEngine/Games/AZ/Tools/bullet_impact_assign.py assigned the selected effect at scale 1 to the rifle Blueprint template and both L_001 placed overrides. Fresh component readbacks before/after Blueprint compilation and after saving confirmed all other manifest fields/fragments and contained magazine exports were preserved. The reticle, P01 animation profile, muzzle flash, fire sound and contained magazine remain configured. Only the pickup BP and L_001 were explicitly saved; their updated disk timestamps and the BP's serialized smoke/reticle dependencies were verified. No vendor effect edits were needed. Never rerun the unguarded rifle_inventory_content_setup.py.

After assignment, the user runs PIE: equip and aim the rifle, fire at a nearby wall and ground to judge orientation/size/fade, then fire into open space. A world hit should emit one compact puff; a miss should emit none. Check ammo debit and existing muzzle/audio/hit marker. Character hits should keep their current damage feedback without stone dust. Runtime behavior and visual tuning remain unverified until that check.

## Persistence correction and receipts

The pre-build backup remains at C:/UnrealEngine/Games/AZ/Saved/Backups/BulletImpact/20260909T024431560961/. On restart, the rifle and both placed pickups unexpectedly had ReticleDefinition=None while their disk hashes still matched that backup. The definition asset itself loaded correctly. The original BP file and registry dependencies lacked the reticle reference, despite the previous in-memory readbacks.

Two authoring hazards were identified: importing text into a borrowed live manifest can bypass setter change detection, and normal component property notifications rerun construction scripts, leaving cached Python wrappers pointing at discarded TRASH_ components. The native component and pickup construction scripts do not overwrite the manifest.

Restored the reticle using detached manifest copies and checked every other field against the original pre-build capture. Corrected both authoring helpers to use detached imports, explicit Modify, notification mode NEVER for this data-only property, and fresh component resolution for readbacks. NEVER skips reconstruction while preserving template-value propagation; subsequent dedicated Blueprint compilation is followed by another fresh audit. The restored BP now has a serialized dependency on DA_HUDReticle_Rifle. Runtime restart/PIE behavior remains a user check, not something inferred from the old cached objects.

After saving the restored reticle, refreshed the impact-only baseline at C:/UnrealEngine/Games/AZ/Saved/Backups/BulletImpact/20260909T034118307908/. Active receipt: C:/UnrealEngine/Games/AZ/Saved/BulletImpact/backup-receipt.json. Then assigned smoke, compiled the pickup BP outside raw Python, re-read current components, and explicitly saved the BP/map. Final receipts: reticle-restoration.json, assign-readback.json, verify-readback.json and final-editor-readback.json under C:/UnrealEngine/Games/AZ/Saved/BulletImpact/. The saved BP contains both reticle and impact dependencies. Unrelated animation/MetaHuman work was excluded from our saves.

For future read-only verification in ordinary Unreal Python:

```python
import runpy
impact = runpy.run_path('C:/UnrealEngine/Games/AZ/Tools/bullet_impact_assign.py', run_name='bullet_impact_helper')
impact['main']('verify', effect_path='/Game/MilitaryWeapDark/FX/P_Impact_Stone_Small_01', scale=1.0)
```

Do not reapply the old foundation or reticle backups over later weapon changes. Audit and refresh the baseline when deliberately changing configuration in a future session.
