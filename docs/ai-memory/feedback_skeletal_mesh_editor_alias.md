# UE5.8 skeletal mesh editor alias can hide the actual rendered mesh

2026-09-11 pistol incident: duplicated rifle pickups appeared as rifles in the
world even though Python readback said SkeletalMeshAsset=None. The equipped pistol
CDO reported Pistols_B in that same editor field but actually had no skinned mesh.

In UE5.8, `SkeletalMeshAsset` is a transient editor alias. Assigning it with
`set_editor_property(..., notify_mode=NEVER)` bypasses the PostEditChange path
that updates the actual `SkinnedAsset`. Reading that alias afterward only verifies
the attempted editor value, not what Unreal renders or the weapon code reads.

Use `component.set_skeletal_mesh_asset(mesh_or_none)` for assignment and
`component.get_skeletal_mesh_asset()` / `get_skinned_asset()` for verification.
For static meshes, use `set_static_mesh()`; `get_editor_property('static_mesh')`
reads the actual property (there is no exposed get_static_mesh method here).

Repair CDO components AND already-instantiated level/thumbnail actors, then
compile/save affected Blueprints and save the map. Confirm both native getters
and rendered previews. No native rebuild is needed for this asset correction.

The early cache hypothesis was disproved: the transient pistol preview actor also
held the stale actual skinned asset. Updating the native mesh setter fixed the
world objects and previews. Engine implementation:
C:/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SkeletalMeshComponent.cpp
(PostEditChange at1487; actual setter path at3508 in this engine checkout).

Also: `/Game/MilitaryWeapDark/Weapons/PistolB_Ammo` is a cartridge casing, not a
detachable magazine. The actual magazine is rigid Clip_Bone geometry in Pistols_B.
Owned SM_Pistol_Magazine now contains559vertices/618triangles extracted from that
bone, with original UVs/material and one box collision. Always inspect the mesh
visually before assigning an asset based on its name alone.
