---
name: AZ project UE 5.7.4 → 5.8.0 migration record
description: 2026-05-10 record of the AZ project migration from UE 5.7.4 (C:\UE57) to UE 5.8.0 (C:\UnrealEngine). Captures plugin/engine state changes, surprises beyond the predicted Mover diff, and decisions made (AnimationToolsBundle disabled). Pair with project_mover_5_7_to_5_8_diff.md for the Mover-specific predictions and project_local_plugin_patches.md for the SmoothWalkingMode MinimalAPI engine patch that must be re-applied on engine resync.
type: project
originSessionId: 6c1c8fd6-056f-42e0-87d3-c943f4c8cf3d
---
## Migration meta

- **Date:** 2026-05-10
- **From:** UE 5.7.4 at `C:\UE57\` (BuildSettings V6, IncludeOrder Unreal5_7)
- **To:** UE 5.8.0 at `C:\UnrealEngine\` (BuildSettings V7, IncludeOrder Unreal5_8)
- **Trigger:** `.uproject` `EngineAssociation` GUID changed; downstream errors followed.

## Surprises beyond the predicted Mover diff

`project_mover_5_7_to_5_8_diff.md` correctly predicted the 4 Mover signature breaks (FMoverSimContext additions). These additional 5.7→5.8 breaks were NOT predicted and surfaced during the build:

1. **`FJsonObject::Values` is now `TMap<UE::FSharedString, TSharedPtr<FJsonValue>>`** (was `TMap<FString, ...>`). Pair.Key is no longer assignable to `FString`. Fix: `FString(*Pair.Key)` (deref to `const TCHAR*`, construct `FString`). Affects any code iterating JSON object values. **Note:** `FJsonObject::SetField` has an FSharedString overload, so `SetField(Pair.Key, ...)` still compiles.

2. **`FAnimNotifyEvent::LinkSequence(Sequence, Time)` removed.** Replacement: `FAnimLinkableElement::Link(UAnimSequenceBase*, float AbsTime, int32 SlotIndex=0)` on the parent class. Drop-in rename `.LinkSequence(` → `.Link(`.

3. **`FScriptMapHelper::FIterator` no longer dereferenceable.** Was: `MapHelper.GetKeyPtr(*It)`. Now: `MapHelper.GetKeyPtr(It)` — pass the iterator directly. Same for `GetValuePtr`.

4. **`UMotionMatchingAnimNodeLibrary::OverrideMotionMatchingBlendSettings`** dropped its third `bool bOverrideValid` arg. Now 2-arg `(MMNode, BlendSettings)`.

5. **`USmoothWalkingMode` engine bug** — `UCLASS(...)` missing `MinimalAPI` in 5.8. Required local engine patch (see `project_local_plugin_patches.md` §3) for any module that derives from it.

## Plugin state changes (fab/marketplace plugins)

| Plugin | Action | Notes |
|---|---|---|
| `UnrealClaude` | Copied `C:\UE57\Engine\Plugins\Marketplace\UnrealClaude\` → same path under `C:\UnrealEngine\`. Bumped `EngineVersion` to "5.8.0" in .uplugin. | 7 source-level fixes for FSharedString + FScriptMapHelper |
| `AnimationToolsBundle` | Copied; renamed marketplace folder from `Animatio2bf24aaa1317V6` → `AnimationToolsBundle` (UBT 5.8 wouldn't index the hashed folder). Bumped `EngineVersion` to "5.8.0" and flipped `Installed: true` → `false`. | **Disabled in `.uproject`** pending 5.8 port — multiple unrelated 5.7→5.8 breaks (FSmartName deprecated, `FApplicationMode::AddTabFactory`/`RemoveTabFactory` removed, `RegisterTabFactories` now private). Re-enable once Marketplace seller ships 5.8 build. |

`Installed: true` in a .uplugin combined with EngineVersion mismatch causes UBT to filter the plugin out of discovery (silent — manifests as `Unable to find plugin 'X'`). Setting `Installed: false` and bumping EngineVersion together is the workaround.

## Source-side migration

- `AZ.Target.cs` and `AZEditor.Target.cs`: `BuildSettingsVersion.V6` → `V7`, `EngineIncludeOrderVersion.Unreal5_7` → `Unreal5_8`.
- AZ Mover overrides: SimContext added per the predicted diff (4 signatures).
- `AZ_AnimInstance.cpp`: removed obsolete `bOverrideValid` arg.
- `AZ_PoseSearchUtils.cpp`: `LinkSequence` → `Link` (5 sites).
- UnrealClaude plugin: 5 sites converting FSharedString keys; 1 site fixing FScriptMapHelper iterator dereference. (See `project_local_plugin_patches.md` for the bindings-readout patch that's the actual local divergence; the FSharedString fixes will need to be re-applied if upstream UnrealClaude is resynced.)

## How to apply

When opening AZ against a new engine install (e.g. UE 5.9 in the future):

1. Check `.uproject` `EngineAssociation` GUID is correct for the target engine.
2. CLI build: `C:\UnrealEngine\Engine\Build\BatchFiles\Build.bat AZEditor Win64 Development -Project="C:\UnrealEngine\Games\AZ\AZ.uproject" -WaitMutex -FromMsBuild`
3. If UBT can't find a Marketplace plugin: copy from old engine's `Engine/Plugins/Marketplace/` to new engine's same path, drop `Binaries/Intermediate/`, rename folder to plugin name if hashed, bump `.uplugin` `EngineVersion` and clear `Installed:true`.
4. If linker fails on `USmoothWalkingMode` symbols: re-apply the MinimalAPI engine patch.
5. If new `Pair.Key`/`FSharedString` compile errors surface: same `FString(*Pair.Key)` pattern.

Real engine versions verified at: `C:\UnrealEngine\Engine\Build\Build.version`.
