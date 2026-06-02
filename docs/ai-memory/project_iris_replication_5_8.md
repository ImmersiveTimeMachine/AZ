---
name: az-iris-replication-adoption-ue-5-8-mover-iris-workable-not-blocked
description: "Project-level record (2026-05-15) of the Iris-replication investigation for CHALK on UE 5.8. Correction to a widespread misbelief — based on Epic's July 2025 forum post — that Mover+Iris is \"BLOCKED until further notice.\" Direct 5.8 source inspection shows NetworkPrediction (Mover's replication backend) has full Iris support; Mover inherits it; AZ's input structs use the engine reference pattern and are Iris-safe by construction. Records the enable steps applied and the empirical PIE-MP smoke-test gate that follows."
metadata: 
  node_type: memory
  type: project
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Iris in CHALK (UE 5.8) — workable, not blocked

**Decision (2026-05-15):** Adopt Iris project-wide on `feature/rootmotion`. SP-first; co-op extensible. The earlier "Iris is blocked by Mover" line is a stale conclusion from Epic's July 2025 forum post — it does not match the 5.8 engine source.

## What the 5.8 engine actually shows (verified directly)

NetworkPrediction (Mover's replication backend) is fully Iris-supported in UE 5.8:

- `C:\UnrealEngine\Engine\Plugins\Runtime\NetworkPrediction\Source\NetworkPrediction\NetworkPrediction.Build.cs:32` — `SetupIrisSupport(Target)` is called unconditionally.
- `NetworkPredictionNetSerializers.cpp` — full Iris NetSerializer implementations (Quantize / Dequantize / Serialize / Deserialize / IsEqual / Validate / CloneDynamicState / CollectNetReferences) using `FIrisPackageMapExports`.
- `NetworkPredictionReplicationProxy.h` — dedicated `// Iris support` members (`FIrisPackageMapExports PackageMapExports`, `FNetTokenExports NetTokensPendingExport`, `GetIrisPackageMap*References` accessors).
- `NetworkPredictionComponent.cpp:120-126` — explicit `// Iris flow` and `// Non-Iris / fallback flow` branches in the ServerRPC param reader.

Mover itself depends on NetworkPrediction (`C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Mover.Build.cs:18`) and inherits the Iris path. The only known Mover-Iris gap is for **Blueprint user-defined structs** inside `FMoverDataCollection`, with an in-engine fallback already implemented at `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Private\UserDefinedStructSupport.cpp:134-148` (`// Fall back case when Iris is enabled`).

## Why AZ's Mover input structs are Iris-safe

AZ's `FAZ_MoverCustomInputs` (`Source/AZ/Public/Animation/AZ_LocomotionTypes.h:149`) and `FAZ_MoverInputCmd` (`Source/AZ/Public/Character/AZ_MoverInputCmd.h:13`):

- Both are **native C++ USTRUCTs with virtual `NetSerialize` overrides** — they set `STRUCT_NetSerializeNative` and **bypass** the UDS-Iris workaround gate at `UserDefinedStructSupport.cpp:131` (`(StructFlags & STRUCT_NetSerializeNative) == 0`).
- Both follow the **engine reference pattern** of `FCharacterDefaultInputs::NetSerialize` (`MoverDataModelTypes.cpp:61`): `Super::NetSerialize(...)` then `Ar << ...` / `Ar.SerializeBits(...)`. `Ar.SerializeBits` and `Ar <<` work under both legacy and Iris bit streams.
- `FMoverDataCollection` (the container that holds them) is registered with `WithNetSerializer = true` (`MoverTypes.h:428-436`). Iris's auto-discovery picks it up; `FMoverDataCollection::NetSerialize` (`MoverTypes.cpp:97`) delegates to `NetSerializeDataArray` (`MoverTypes.cpp:518`), which virtual-dispatches to AZ's struct overrides at line 581.

## GAS under Iris in 5.8 — partial / transitional

GAS opts in unconditionally at the build level: **`GameplayAbilities.Build.cs:42`** — `SetupIrisSupport(Target);`. But at runtime, GAS has explicit `if (UE::Net::ShouldUseIrisReplication())` branches that route tag replication through a **different struct** than the canonical one. Honest state: GAS runs under Iris and is correct, but tag-count replication uses a compatibility surface while Epic finishes the native Iris path for `FGameplayTagCountContainer`.

**Important — not a legacy fallback.** The compatibility surface is itself a fully Iris-aware GAS struct (`FMinimalReplicationTagCountMap`) with its own native Iris NetSerializer. "Falling back" here means falling back from one Iris path to another, not from Iris to legacy.

### The two-surface design

ASC carries the same tags in two forms:

| | `FGameplayTagCountContainer` | `FMinimalReplicationTagCountMap` |
|---|---|---|
| **Role** | Authoritative server-side state — queries, ability blocking, GE prerequisites | Compact wire format — tag presence to peers |
| **Storage** | Tag tree with parent-tag relations, owners, change delegates | Flat `TMap<FGameplayTag, int32>` (tag → count) |
| **Iris fragment in 5.8** | Push-model path CVar-gated, off by default | Native Iris NetSerializer (`MinimalReplicationTagCountNetSerializer.h/.cpp`) |
| **Member on ASC** | `GameplayTagCountContainer` (`AbilitySystemComponent.h:1876`) | `MinimalReplicationTags` (line 1923), `ReplicatedLooseTags` (line 1943) |
| **Game-code visibility** | `HasMatchingGameplayTag`, `GetGameplayTagCount`, etc. — primary API surface | Internal; rarely touched directly |

### The runtime decision (per tag update)

`AbilitySystemComponent.cpp` (`UpdateTagMapSingle_Internal` at line 715, `UpdateTagMap_Internal` at line 786):

```
if (Iris on) AND (CVarReplicateTagCountContainerWithIris is OFF)  -- default
    -> Write to MinimalReplicationTags / ReplicatedLooseTags
       Local container still updated immediately; the *wire* uses the minimal-tags
       struct (which has a working Iris NetSerializer)

if (Iris on) AND (CVarReplicateTagCountContainerWithIris is ON)
    -> MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, GameplayTagCountContainer, this);
       Push-model the canonical container directly (newer, less battle-tested)

if (Iris off)  -- legacy
    -> Replicate GameplayTagCountContainer the legacy way
```

Epic's own comment at `AbilitySystemComponent.cpp:719`:
> *"Iris replication doesn't work the same so we're falling back to using the Minimal and replicated loose tags until it's fixed."*

### Evidence map (5.8 source — verified 2026-05-21)

- `C:\UnrealEngine\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\GameplayAbilities.Build.cs:42` — `SetupIrisSupport(Target);` (unconditional)
- `AbilitySystemComponent.cpp:717, 752, 780, 788, 841, 882` — six runtime branches in tag-update paths
- `GameplayEffect.cpp:4671, 4979` — GE replication branches (`bIrisWithoutTagCountSerializer`)
- `GameplayEffectTypes.cpp:810, 861` — GE types replication branches (`UsingCountContainerRep`)
- `Public/Serialization/MinimalReplicationTagCountNetSerializer.h` + `Private/Serialization/MinimalReplicationTagCountNetSerializer.cpp` — native Iris NetSerializer GAS routes through
- `FMinimalReplicationTagCountMap::NetSerialize` (`GameplayEffectTypes.cpp:1629`) — underlying wire-format serializer (bit-packed; configurable tag-count bit width via `AbilitySystemGlobals.h:236`)
- `FMinimalReplicationTagCountMap` struct decl: `GameplayEffectTypes.h:1676`

### Implications for CHALK

- All game-side queries (`HasMatchingGameplayTag`, `GetGameplayTagCount`) still target the local authoritative container — code reads identically to legacy.
- Tag replication is correct on both ends (verified by PIE-MP smoke test 2026-05-21).
- ⚠ The minimal-tags wire format has a configurable max tag count; overflow logs an error at `GameplayEffectTypes.cpp:1642` and tags do not replicate. With CHALK's tag volume (per-ability state + weapon slot tags) we are nowhere near the cap.
- ⚠ Slightly more indirect propagation: receiver walks the minimal-tags `TagMap` and reapplies to its local container (`SetOwner` + `UpdateOwnerTagMap` at `GameplayEffectTypes.cpp:1716, 1739`).
- 🔄 **Future-direction switch:** `CVarReplicateTagCountContainerWithIris=1` enables push-model on the canonical container. Flip when Epic stabilizes that path (likely 5.9+).

### Lesson — accurate version of the headline claim

Earlier framing "GAS ✅ Iris-aware in 5.8" was correct but understated the nuance. Accurate version: **GAS compiles Iris in, runs under Iris, and has explicit branches to keep correctness — but tag-count replication routes through a compatibility shim (a separate Iris-native struct, NOT a legacy fallback) until Epic finishes the canonical path.**

## Why the stale "BLOCKED" conclusion existed

- Epic's Alex Koumandarakis ([forum post, Jul 2025](https://forums.unrealengine.com/t/iris-network-prediction-plugin-incompatibilities/2651670)) said *"Work on Iris and Mover is ongoing, but at this time, we don't have any sort of roadmap to share."*
- That was true for UE 5.7. **It is no longer true for 5.8** — the Iris support shipped in NetworkPrediction.
- A research agent reading only `Mover.Build.cs` (which doesn't itself call `SetupIrisSupport`) and the July post will conclude BLOCKED. They miss that Mover depends on NetworkPrediction, where the Iris hookup lives.

**Lesson for future sessions:** when assessing Iris support for any plugin that uses NetworkPrediction, check the **NetworkPrediction module's** Build.cs and net-serializer code, not just the leaf plugin's.

## Enable steps applied (2026-05-15)

1. **`C:\UnrealEngine\Games\AZ\Source\AZ\AZ.Build.cs`** — added `SetupIrisSupport(Target);` (canonical 5.8 idiom; conditionally pulls in `IrisCore` when Iris is enabled at the target level).
2. **`C:\UnrealEngine\Games\AZ\Config\DefaultEngine.ini`** — added under `[/Script/Engine.Engine]`:
   ```ini
   +ConsoleVariables=net.Iris.UseIrisReplication=1
   ```
3. **`AZ.uproject`** — Iris plugin was already enabled (lines 139-141) since the 5.8 migration. No change needed.

## Empirical gate (the PIE-MP smoke test)

The bar from [project_sp_first_coop_extensible.md](project_sp_first_coop_extensible.md) — *"2-player listen-server PIE should just work"* — is the validation:

1. CLI-rebuild `AZEditor` (clean rebuild because dependency module list changed).
2. Open editor, switch PIE to **Number of Players: 2**, **Net Mode: Play As Listen Server**.
3. Press Play.
4. Pass conditions:
   - Both pawns spawn without crash.
   - Both pawns move (input replicates from client to server, mover state replicates from server back).
   - GAS abilities activate on both ends (Jump / Crouch / Aim / Shoot).
   - No `LogIris` / `LogNet` / `LogNetSerialization` errors in the log.
5. Fail mode → bisect: try `net.Iris.UseIrisReplication=0` to confirm the regression is Iris-specific; capture the specific log error; check `LogIris.Verbose 1` for fragment-registration issues.

## Surface not yet validated under Iris (acceptable risk for first pass)

| Surface | State | Plan |
|---|---|---|
| `UAZ_HeroAttributeSet` | Legacy `OnRep` style — works under Iris but not push-model | Convert to `MARK_PROPERTY_DIRTY_FROM_NAME` later (free improvement, ~1 hr) |
| `FAZ_Inv_CommonUI_InventoryFastArray`, `FAZ_Inv_InventoryFastArray` | FastArrays — work under Iris, unoptimized (no Iris fast-array fragment yet) | Revisit when inventory work resumes |
| `FAZ_GameplayEffectContext` | GAS has Iris-aware NetSerializer for FGameplayEffectContext | No change |
| GAS tag replication (`GameplayTagCountContainer`) | Routes through `FMinimalReplicationTagCountMap` Iris NetSerializer by default (Epic's transitional surface — Iris-native, not legacy) | See §"GAS under Iris in 5.8 — partial / transitional". Flip `CVarReplicateTagCountContainerWithIris=1` when Epic stabilizes native push-model path (likely 5.9+) |
| 26 RPCs across equipment/inventory/montage | Iris doesn't eliminate RPCs — same code path | No change |

None of these are blockers; all degrade gracefully.

## Status flags (Iris in UE 5.8)

- **Iris itself:** Beta + opt-in (per Epic productboard); no legacy-replication deprecation timeline.
- **Lyra:** Iris-enabled only in the engine-internal `/Samples/Games/Lyra/` branch; public sample defaults to legacy.
- **Fortnite:** ships on Iris since 2022.
- **AZ:** Iris enabled 2026-05-15; PIE-MP smoke test PASSED 2026-05-21.

## Revisit triggers

- If PIE-MP fails — diagnose and either fix or roll back the CVar.
- If Iris promotes to Production in 5.9/6.0 — review the Beta-era workarounds we may have inherited.
- If Mover or GAS publish a `*_Iris` companion module — investigate whether to depend on it.

## Related memory

- [[project_v2_architecture]] — committed to push-model state, deterministic trajectory, tags-as-events. Iris rewards this discipline.
- [[project_sp_first_coop_extensible]] — defines the "2-player PIE just works" bar this enable targets.
- [[project_multipawn_class_design]] — vehicles also use Mover; same Iris path will apply when they land.
- [[project_ue58_migration_2026-05-10]] — base engine state for this work.
