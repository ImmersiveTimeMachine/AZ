# Step-0 M16 skin-weight verification

The skin-weight uncertainty in Claude's original report is resolved through a read-only export. That report remains unchanged as the record of its original inspection limits.

## Method

Exported /Game/AZ/Assets/M16/SKL/M16_Skeleton using Unreal's SkeletalMeshExporterFBX with ASCII=true, export_source_mesh=false, level_of_detail=false, collision=false and export_morph_targets=false. The mesh has one LOD. Parsed FBX skin-cluster Indexes/Weights and their model connections. No preview playback, bone manipulation, asset edits, build or PIE was used.

Export: C:/UnrealEngine/Games/AZ/Saved/Temp/RifleStep0RigExport/M16_weights_readonly.fbx.

Evidence, including source asset SHA-256: C:/UnrealEngine/Games/AZ/docs/design-briefs/rifle-step0-skin-weights.json.

## Result

- 10,485 exported vertices; all 10,485 weighted.
- Zero vertices with more than one positive bone influence.
- Every positive influence is 1.0.
- UE4_M16_MagazMod has 1,546 exclusively weighted vertices.
- The parent magazine pivot has zero direct influences; it moves its weighted geometry child through the hierarchy. The other mechanism pivots follow the same pattern.

| Pivot | Weighted geometry child | Vertex count |
|---|---|---:|
| trigger | UE4_M16_BasePart_001 | 92 |
| selector | UE4_M16_BasePart_002 | 68 |
| catch | UE4_M16_BasePart_003 | 41 |
| charginghandle | UE4_M16_ElitBase_Shutter | 298 |
| charginghandle | UE4_M16_ElitMod_Shutter | 1,296 |
| ejector | UE4_M16_ElitBase_001 | 34 |
| magazine | UE4_M16_MagazMod | 1,546 |

The weighted magazine region's exported bounds have half-extents approximately (1.259078, 4.499779, 9.749053), matching the standalone magazine prop bounds recorded in Claude's audit to rounding precision. This supports the identity of the weighted part in addition to its bone name.

## Contract decision

The preferred built-in magazine visibility operation is HideBoneByName("UE4_M16_MagazMod", PBO_None) on WeaponMesh3P, with the corresponding unhide operation where the chosen presentation policy requires it. The geometry has isolated influences, so material-section hiding or mesh extraction is not needed as the initial approach.

The detachable prop's Magwell attachment remains on a separate stable receiver frame, outside the hidden geometry subtree. Actual prop pivot alignment and visual hide/show handoff still need user-authorized visual verification during preparation/integration. This report proves skin isolation; it does not claim a live reload or hide/show test passed.

Mechanism animation clips/AnimBP and Muzzle/CasingEject/Magwell sockets are still preparation work. The export did not create them or alter the gameplay weapon.
