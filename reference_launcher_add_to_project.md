---
name: reference-launcher-add-to-project
description: "Why Epic Launcher \"Add to Project\" never lists AZ, and how to bring Fab/Vault packs in (temp project + copy incl. __ExternalActors__)"
metadata:
  node_type: memory
  type: reference
  originSessionId: 4ed8bc7e-61ec-42da-a018-4c39edd7dc0d
  modified: 2026-10-14T21:24:35.045Z
---

AZ.uproject EngineAssociation = GUID {305F6CC8-...} = source build C:/UnrealEngine (registered in HKCU\Software\Epic Games\Unreal Engine\Builds). Epic Launcher "Add to Project" lists only projects bound to launcher-installed engines, so AZ never appears (even with "Show all projects").

Recipe: create/add pack to a throwaway project on a launcher engine (≤5.8), then copy its Content/<Pack> AND Content/__ExternalActors__/<Pack> + __ExternalObjects__/<Pack> into AZ (World Partition maps are empty without them — DerelictCorridor 2026-10-14). Don't change AZ's EngineAssociation.
Related: [[project-asset-roundtrip]]
