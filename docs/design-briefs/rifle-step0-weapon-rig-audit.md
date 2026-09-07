# Rifle step 0 — weapon-rig, mechanical animation, sockets and detachable-magazine audit

Scope: weapon rig only. Character-animation selection/retarget verification, gameplay/item contracts, input policy and the
main step-0 report are Codex's and are not duplicated here.

Status: read-only inspection of live editor data and local source, 2026-09-07, checkpoint `229f9b9` on `spike/cmc-backport`.
No asset, Blueprint, socket or skeleton was created, edited, saved, retargeted or reimported. No PIE, no build, no Live
Coding, no preview playback. Every claim below is either evidence (with its source) or is explicitly labelled a proposal or
an unknown.

---

## 1. Blueprint, components, meshes, skeleton and current animation configuration

### 1.1 Confirmed identities

| Fact | Value | Evidence |
|---|---|---|
| Gameplay weapon BP | `/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle` | loaded live; generated class `AZ_BP_Rifle_C` |
| Native parent | `AAZ_Weapon` (itself `AAZ_Item`, `IAbilitySystemInterface`) | `C:/UnrealEngine/Games/AZ/Source/AZ/Public/Weapon/AZ_Weapon.h:26-27` |
| Active visible mesh component | `WeaponMesh3P` | live component dump; `AZ_Weapon.cpp:48-53` |
| Mesh on that component | `/Game/AZ/Assets/M16/SKL/M16_Skeleton` | live `skeletal_mesh_asset` read |
| Weapon skeleton | `/Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton` | live `skeleton` property of the mesh |
| Filesystem root of that content | `C:/UnrealEngine/Games/AZ/Content/AZ/Assets/M16/` | asset-registry path mapping |

Both "known facts to verify" in the assignment are **confirmed**: `WeaponMesh3P` is the active visible component, and it
uses `M16_Skeleton` / `M16_Skeleton_Skeleton`.

### 1.2 Full component inventory (live, unchanged)

| Component | Class | Mesh assigned | Visible | Declared in | Verdict |
|---|---|---|---|---|---|
| `WeaponMesh3P` | SkeletalMeshComponent | **`/Game/AZ/Assets/M16/SKL/M16_Skeleton`** | `true` | `AZ_Weapon.h:280`, created `AZ_Weapon.cpp:48` | **ACTIVE — the only thing that renders** |
| `WeaponMesh1P` | SkeletalMeshComponent | none | `false` (`AZ_Weapon.cpp:42`) | `AZ_Weapon.h:277` | unused first-person slot |
| `SkeletalMesh` | SkeletalMeshComponent | none | `true` (draws nothing) | `AZ_Item.h:36` (`SkeletalMeshComponent`) | unused item-base component |
| `Mesh` | StaticMeshComponent | none | `true` (draws nothing) | `AZ_Item.h:28` (`MeshComponent`) | unused item-base component |
| `CollisionComponent` | BoxComponent | — | hidden in game | `AZ_Weapon.h:274`, `AZ_Weapon.cpp:32` | pickup collision; rel. loc `(0, 17, 11)` |
| `PickupSphere` | SphereComponent | — | hidden in game | `AZ_Item.h:32` | pickup overlap |
| `Arrow` | ArrowComponent | — | editor only | inherited | editor gizmo |
| `AbilitySystemComponent` | `UAZ_AbilitySystemComponent` | — | — | `AZ_Weapon.h:241`, `AZ_Weapon.cpp:57` | weapon-side ASC (Codex's contract area) |

**Unused 1P/base components vs the active one:** `WeaponMesh1P`, `SkeletalMesh` and `Mesh` all carry no mesh asset. Only
`WeaponMesh3P` renders. Nothing was changed; this is recorded so a later phase does not attach a magazine prop or an AnimBP
to the wrong component. Note `SkeletalMesh` and `Mesh` are *visible* but empty — they are harmless today but are exactly the
kind of component that silently starts rendering if something later assigns a mesh to the wrong slot.

### 1.3 Current animation configuration — the rig is inert

| Property | Value | Consequence |
|---|---|---|
| `WeaponMesh3P.anim_class` | **`None`** | no weapon AnimBP |
| `WeaponMesh3P.animation_mode` | `AnimationMode.ANIMATION_BLUEPRINT` | in Blueprint mode with no Blueprint → evaluates the reference pose only |
| `M16_Skeleton.post_process_anim_blueprint` | **`None`** | no post-process graph either; nothing animates the mesh from the asset side |
| `M16_Skeleton.physics_asset` | `None` | no simulated/secondary motion |
| LOD count | `1` | no LOD-related bone stripping to worry about |
| Material slots | 10 (`M16_base_mat` ×9, `M16_stick2_mat` ×1) | see §5 for why slot-based magazine hiding is not the recommended route |

**Verified:** the weapon mesh today is a static prop in practice — the rig exists, but nothing drives it. This matches the
plan's statement and is the central gap this audit is about.

### 1.4 Sibling actors that also show this mesh (context, unchanged)

- `/Game/AZ/Blueprints/Weapon/AZ_M16` — `SkeletalMesh` (the item-base component) = `M16_Skeleton`, `anim_class=None`.
  Confirms the plan's "AZ_M16 is only an item actor, not the gameplay weapon".
- `/Game/AZ/Blueprints/Items/Equippables/Weapons/BP_Inv_EquipActor_Rifle_M16` — **two** SkeletalMeshComponents, *both*
  `M16_Skeleton_GEN_VARIABLE`, *both* assigned `M16_Skeleton`, both `anim_class=None`.
  **Flagged:** two components with the same mesh on one actor is a double-render risk (and would later mean two magazines
  even before any prop work). Needs a user visual check in that Blueprint before it is used for inventory preview.

---

## 2. Mechanical bone hierarchy and what can actually be animated

### 2.1 The hierarchy (27 bones, live read via `UAZ_SkeletonUtils::GetBoneNames` + `SkeletalMesh.get_bone_parent`)

```
root
└── hand_r
    ├── UE4_M16_BasePart            (static geometry)
    ├── UE4_M16_CompensatorBase     (static geometry — muzzle end)
    ├── UE4_M16_CompensatorMod      (static geometry — muzzle end)
    ├── UE4_M16_ElitBase            (static geometry)
    ├── UE4_M16_SightBase           (static geometry)
    ├── UE4_M16_StockMod            (static geometry)
    ├── catch          ── UE4_M16_BasePart_003     ── catch_end
    ├── selector       ── UE4_M16_BasePart_002     ── selector_end
    ├── charginghandle ── UE4_M16_ElitBase_Shutter ── UE4_M16_ElitMod_Shutter ── charginghandle_end
    ├── ejector        ── UE4_M16_ElitBase_001     ── ejector_end
    ├── trigger        ── UE4_M16_BasePart_001     ── trigger_end
    └── magazine       ── UE4_M16_MagazMod         ── magazine_end
```

This is a **rigid-parts rig**, not a deforming character rig: every mechanism bone is a pivot that parents exactly the
geometry bone(s) for its own part, and the geometry bones are named after the original per-part static meshes
(`UE4_M16_BasePart`, `UE4_M16_MagazMod`, …), which exist independently as static meshes (§5). The six `*_end` bones are
Blender export leaf markers, not animation targets.

Note `hand_r` is a bone **inside the weapon rig** (the whole gun hangs off it); it is not the character's `hand_r`.

### 2.2 Reference-pose pivots (local translation relative to the parent, live read via `Skeleton.get_reference_pose()`)

| Mechanism bone | Local translation rel. `hand_r` | Moving part it drives |
|---|---|---|
| `trigger` | `(-0.000261, -6.955359, 6.105661)` | `UE4_M16_BasePart_001` |
| `selector` | `(1.505781, -8.102423, 0.830770)` | `UE4_M16_BasePart_002` (fire selector) |
| `catch` | `(1.518828, -8.562296, 9.301699)` | `UE4_M16_BasePart_003` (magazine/bolt catch) |
| `charginghandle` | `(0.000000, -14.151815, -3.372726)` | `UE4_M16_ElitBase_Shutter`, `UE4_M16_ElitMod_Shutter` |
| `ejector` | `(-0.838637, -12.290596, 13.066059)` | `UE4_M16_ElitBase_001` |
| `magazine` | `(0.000057, 6.804444, 15.916255)` | `UE4_M16_MagazMod` |

Each geometry child carries a compensating offset (e.g. `UE4_M16_MagazMod` = `(1.081988, -6.804444, -15.916255)` relative to
`magazine`), i.e. the parts all resolve to a common origin while the mechanism bones sit at their real pivots. That is the
expected shape for this kind of export and is what makes per-part animation possible.

Whole-mesh imported bounds: origin `(-1.082810, 17.930748, 5.508225)`, box extent `(4.165903, 42.339661, 13.956741)`,
sphere radius `44.774906`. The long axis is **Y** (extent 42.3) and X is thin (4.17) — used for the muzzle reasoning in §4.

### 2.3 Deformation readiness — partially verified, one item genuinely unknown

| Question | Answer | Basis |
|---|---|---|
| Do the mechanism bones exist? | **Yes**, all six | live bone list |
| Do they each own a distinct geometry bone? | **Yes** | hierarchy above |
| Does matching per-part geometry exist? | **Yes** | the same part names exist as standalone static meshes (§5) |
| Are the parts actually *skinned* to those bones (weights)? | **UNKNOWN — not inspectable** | see below |

Per-vertex skin weights are not exposed to Python/Blueprint in this engine build. Everything reachable was checked:
`unreal.SkeletalMesh` exposes only `find_socket`, `get_bone_parent`, `get_bone_children`, `num_sockets`,
`get_socket_by_index`, `get_imported_bounds`, `has_vertex_colors`, LOD helpers; `unreal.Skeleton` exposes
`get_reference_pose`, `copy_bones_from_skeleton`; `UAZ_SkeletonUtils` exposes `GetBoneNames`, `ListSockets`, `AddSocket`,
`RemoveSocket` and blend-profile helpers only (`C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_SkeletonUtils.h:22-71`).
None of these return influences. Bone names and hierarchy are strong circumstantial evidence but, per the assignment, are
**not** proof of deformation readiness.

**User visual verification required (precise steps).** Open
`C:/UnrealEngine/Games/AZ/Content/AZ/Assets/M16/SKL/M16_Skeleton.uasset`, then for each of `trigger`, `selector`, `catch`,
`charginghandle`, `ejector`, `magazine`:

1. In the Skeleton Tree, select the bone.
2. Either enable **Character ▸ Mesh ▸ Selected Bone Weight** to see the influenced vertices tinted, **or** drag the bone's
   translate/rotate gizmo in the viewport and watch which geometry follows.
3. Record: does exactly the expected part move, does the whole gun move, or does nothing move?

The result decides §5's magazine-hiding method and whether authored mechanism clips will read correctly. Please do not
save any accidental gizmo edit.

---

## 3. Existing rifle-mechanism animation content — exhaustive search result

### 3.1 Result: there are **zero** weapon-mechanism clips for this rig

Scanned **11,148** animation-class assets (`AnimSequence`, `AnimMontage`, `AnimComposite`, `BlendSpace`, `BlendSpace1D`,
`AimOffsetBlendSpace`, `PoseAsset`, `AnimBlueprint`) and all **141** skeletons in the project.

| M16 weapon skeleton (all 27-bone, identical bone sets) | Clips bound to it |
|---|---|
| `/Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton` ← **the active one** | **0** |
| `/Game/Assets/M16/SKL/M16_Skeleton_Skeleton` | **0** |
| `/Game/RM_Movement/M16/SKL/M16_Skeleton_Skeleton` | **0** |
| `/Game/Assets/Weapons/M16/Rig/SKM_Armature_Skeleton` (21 bones = same minus the six `*_end`) | **0** |

This is a bounded, exhaustive registry result, not an impression. It confirms the assignment's prior read.

### 3.2 Character clips that merely look like weapon clips — do not reuse as mechanism animation

`IDLE_02` exists twice (`/Game/Assets/M16/Animations/BlenderImport/IDLE_02` and
`/Game/RM_Movement/M16/Animations/BlenderImport/IDLE_02`; 3.267 s, 98 frames, non-additive). Despite living in an "M16"
folder it is bound to `CHR_M16_SKL_Skeleton`, an **84-bone character skeleton** (`pelvis`, `spine_01..05`, `hand_l`, full
finger chains, SurvivalMan materials). It is a character clip holding a rifle, and contains **none** of the M16 mechanism
bones. Folder name is not provenance — same trap the main plan already flagged for `Mocup_Online/Rifle`.

That character skeleton does carry a socket `weapon_r_muzzle` on `hand_r` at
`loc=(-11.12, -14.82, -13.08) rot=(2.8, 156.2, 15.9)` — a *character-side* muzzle reference, not a weapon-rig socket, and
outside this audit's ownership.

### 3.3 Donor candidates that genuinely animate weapon mechanisms — none are drop-in

| Donor rig | Mechanism-ish bones | Clips available |
|---|---|---|
| `/Game/Assets/Weapons/AK12_Rifle` (same vendor family as the M16) | `clip`, `lock`, `trigger`, `safety_switch`, `recharge`, `scope`, `auto_single`, `hand`, `root`, `root_hand_holder` | `Anim_rifle_fire_trigger`, `Anim_rifle_recharge`, `Anim_rifle_recharge_switch`, `Anim_rifle_auto_switch`, `Anim_rifle_safety_swich_inverse` — all **0.208 s / 5 frames** |
| `/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B_Skeleton` (6 bones) | `slide`, `trigger` | `Fire_Rifle_W`, `Reload_Rifle_Hip_W`, `Reload_Rifle_Ironsights_W`, `Prone_Reload_Rifle_W` |
| `/Game/InventorySystemPro/.../SK_Rifle_Skeleton` (8 bones) | `bolt`, `magazine`, `slide`, `trigger` | `Weap_Rifle_Fire`, `Weap_Rifle_Reload` (+ matching montages) |
| `/Game/FPS_Controller/.../Weapons/AK/SK_AK_Body` (6 bones) | `catch`, `selector`, `slide`, `trigger` | `A_AK_Body_Reload`, `A_AK_Body_Tactical_Reload`, `A_AK_body_Shot_01..03`, `A_AK_Body_First_Equip` |
| `/Game/FPS_Controller/.../Weapons/Pistol/SK_X24` (11 bones) | `slide`, `trigger` | `A_X24_Body_EmptyReload`, `A_X24_Body_Final_Shot`, `A_X24_Body_Shot_01..03` |

**The AK12 is the closest relative and still is not compatible.** Its clips animate
`root_hand_holder, root, clip, lock, trigger, safety_switch, recharge, scope, auto_single, hand` — only `trigger` and `root`
share a name with the M16 rig. The semantic mapping is obvious (`clip`→`magazine`, `recharge`→`charginghandle`,
`safety_switch`→`selector`, `lock`→`catch`) but name-based reuse is impossible; it would need an IK Retargeter with a
hand-authored chain mapping, and at 5 frames these read as two-pose extremes rather than finished animations.

**Conclusion for content phase 0:** M16 mechanism motion is **asset authoring or a data-driven bone layer**, not asset
reuse. The donors above are useful as *timing and amplitude reference* only. No absent content is assumed to exist.

---

## 4. Socket inventory and proposed attachment contract

### 4.1 What exists today (live, exact)

Sockets live on the **Skeleton** asset (`find_socket` returns `M16_Skeleton_Skeleton:SkeletalMeshSocket_2/_3`), not on the
mesh — so any future socket work belongs on `/Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton`, and only that copy has them.

| Socket | Parent bone | Relative location | Relative rotation | Scale |
|---|---|---|---|---|
| `LeftHandGrip` | `hand_r` | `(7.953090, -7.950085, 25.291336)` | `(0, 0, 0)` | `(1,1,1)` |
| `LeftHandGripAim` | `hand_r` | `(5.684339, -7.950085, 22.308682)` | `(0, 0, 0)` | `(1,1,1)` |

`num_sockets = 2`. Explicit negative checks: `find_socket("Muzzle")`, `find_socket("muzzle")`,
`find_socket("CasingEject")`, `find_socket("Magwell")` all return **`None`**. The other three M16 mesh copies have **no**
sockets at all; `/Game/Assets/Weapons/M16/Rig/SKM_Armature_Skeleton` has an unrelated `rifle_hand_rSocket`.

### 4.2 The missing Muzzle socket already has live consumers — it fails silently

`UAZ_GA_Shoot` expects a socket literally named `Muzzle`:

| Site | Behaviour when the socket is absent |
|---|---|
| `Source/AZ/Public/AbilitySystem/Abilities/AZ_GA_Shoot.h:115` — `FName MuzzleSocketName{TEXT("Muzzle")}` | default name, editable per ability |
| `Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Shoot.cpp:177-181` — guarded by `WeaponMesh->DoesSocketExist(...)` | muzzle flash is **silently skipped** |
| `Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Shoot.cpp:185` — `SpawnSoundAttached(FireSound, WeaponMesh, MuzzleSocketName)` | **not guarded**: falls back to the component origin, so fire audio plays from the gun's pivot, not the barrel |
| `Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Shoot.cpp:351` — `StartLocation.SourceSocketName = MuzzleSocketName` | trace start location degrades to the component transform |

There is **no** muzzle property on `AAZ_Weapon` itself (`AZ_Weapon.h` declares `CarrySocketName`, `RelaxedSocketName`,
`AimSocketName`, `LeftHandGripSocket` only, lines 90-101) — the muzzle name lives on the ability. Whoever owns the fire
contract should decide whether it moves onto the weapon definition; flagged, not decided here.

### 4.3 Proposed attachment contract

Existing sockets are reused where correct. **Every transform below is a proposal and requires visual validation in the
skeletal mesh editor — none of these numbers is measured geometry.**

| Contract name | Status | Proposed parent bone | Rationale (evidence) | Transform |
|---|---|---|---|---|
| `GripLeft` | **Reuse, exists** | `hand_r` | `LeftHandGrip` / `LeftHandGripAim` already exist and are already read by `AAZ_Weapon::GetLeftHandSocket` (`AZ_Weapon.h:120-121`) | measured, §4.1 |
| `Muzzle` | **Missing — new** | `UE4_M16_CompensatorMod` | the compensator is the muzzle device, and it is the part at the far end of the barrel; the mesh's long axis is Y (extent 42.34 vs 4.17 on X) | **PROPOSAL — must be placed visually at the bore exit, with +X of the socket aligned down the bore** |
| `CasingEject` | **Missing — new** | `ejector` | the rig already has a dedicated `ejector` pivot at `(-0.838637, -12.290596, 13.066059)` rel. `hand_r`, i.e. the ejection-port side | **PROPOSAL — place at the ejection port lip, rotation facing outward** |
| `Magwell` | **Missing — new** | `magazine` | the `magazine` pivot at `(0.000057, 6.804444, 15.916255)` rel. `hand_r` is exactly the magazine's mount point, so a socket there follows any magazine animation for free | **PROPOSAL — identity offset from the `magazine` bone is the natural first guess; validate visually** |
| Magazine **in-hand** presentation | **Missing — new**, character-side | character `hand_l` (MetaHuman body rig) | a detached magazine must ride the hand during transfer; the weapon rig cannot own this | **PROPOSAL — character-side socket; belongs to the character-rig owner, listed here only for completeness of the contract** |
| Magazine **world** presentation | **Missing — new**, actor-side | dropped-magazine actor root | a discarded magazine is a world prop, not a weapon socket | no socket needed |

Placing `Muzzle` at the compensator rather than at `hand_r` matters: attaching it to `hand_r` would appear to work while
putting the flash, the fire audio and the shot's start location at the gun's mount point instead of the barrel.

---

## 5. Detachable magazine — a usable mesh exists

### 5.1 Verified: separate magazine geometry already exists as static meshes

| Asset | Bounds (origin / box extent) | LODs | Sections | Material slot |
|---|---|---|---|---|
| `/Game/Assets/M16/mesh/UE4_M16_MagazMod` | `(0.001438, 2.147208, 9.976605)` / `(1.259078, 4.499778, 9.749054)` | 1 | 1 | `M16_StockBase_LOD0SG` |
| `/Game/Assets/M16/mesh/UE4_M16_MagazBase` | `(0.000000, 1.796998, 10.288954)` / `(1.065024, 4.149034, 8.580768)` | 1 | 1 | `M16_StockBase_LOD0SG` |
| `/Game/RM_Movement/M16/mesh/UE4_M16_MagazMod` | identical to the above | 1 | 1 | same |
| `/Game/RM_Movement/M16/mesh/UE4_M16_MagazBase` | identical to the above | 1 | 1 | same |

**Use `UE4_M16_MagazMod`**, not `MagazBase`: the assembled rifle's magazine bone is literally named `UE4_M16_MagazMod`, so
`MagazMod` is the variant that matches what is currently on the gun. `MagazBase` is a different (slightly smaller) variant
and would not visually match.

**Caveat to resolve before use:** these live under `/Game/Assets/M16/` and `/Game/RM_Movement/M16/`, **not** under the
active `/Game/AZ/Assets/M16/` tree (which contains no `mesh/` folder at all). Referencing content outside the AZ tree, or
migrating a copy into it, is a content-ownership decision for the user — not made here, and no asset was copied.

No mesh extraction or authoring is needed to obtain a magazine prop. Single-section, single-LOD static meshes are ideal for
an attached prop.

### 5.2 Avoiding two visible magazines

The rifle's own magazine is part of `WeaponMesh3P`, so a prop must be paired with hiding the built-in one. Options, best
first:

1. **Hide the bone (recommended).** `USkeletalMeshComponent::HideBoneByName("UE4_M16_MagazMod", PBO_None)` on
   `WeaponMesh3P` removes the vertices bound to that bone; `UnHideBoneByName` restores them. This is clean, reversible,
   costs nothing per frame, and is exactly the case a rigid-parts rig is good for. **Conditional on §2.3**: it only works
   if the magazine geometry is actually weighted to `UE4_M16_MagazMod` (or `magazine`). If the user's visual check shows the
   magazine is weighted elsewhere, this option dies and option 3 becomes primary.
2. **Scale the bone to zero** through the weapon's mechanical animation. Works without weighting assumptions about
   *which* bone hides, but it is animation state rather than a clean visibility switch and can interpolate visibly.
3. **Material-section hiding** (`ShowMaterialSection` / a masked material). **Not recommended here**: the skeletal mesh has
   10 material slots that are 9× the same `M16_base_mat` plus 1× `M16_stick2_mat`, so the section↔part mapping is not
   established and hiding a section could remove unrelated geometry. Would need a per-section audit first.

**Contract:** exactly one magazine is visible at any instant — either the rifle's built-in geometry, or the detached prop,
never both. The prop is attached to `Magwell` (§4.3) when seated, to the character's hand during transfer, and spawned as a
world actor when discarded. The prop is **presentation only**: it carries no rounds and is not an inventory item; every
representation resolves the same magazine instance ID, which the inventory owns (Codex's contract).

---

## 6. Weapon-presentation contract

Semantic contract labels for coordination between the character action and the weapon rig. **These are not GameplayTags and
none is registered.** Timings are deliberately absent: the magazine-transfer timeline is mapped from the character clips and
is Codex's, not invented here from weapon-side guesses.

| Contract label | Required mechanical pose / visibility on the weapon rig | Bones / assets involved |
|---|---|---|
| `ShotAccepted` | Trigger pulled and released; charging-handle/bolt group cycles rearward and returns; casing effect emitted at the ejection port; muzzle flash + fire audio at the bore. Fires **once per accepted shot**, never from a cosmetic loop. | `trigger`, `charginghandle` (+ its two shutter parts), `ejector`; `Muzzle`, `CasingEject` sockets |
| `MagOut` | Magazine catch actuates; the built-in magazine geometry becomes hidden **at the same instant** the detached prop becomes visible in the hand. Single-frame handover, no overlap. | `catch`, `magazine` / `UE4_M16_MagazMod` hidden; prop shown |
| `MagStowed` | No magazine visible in the magwell; the outgoing prop is either hidden (stowed) or handed to a world/character presentation. Weapon reads "empty well". | built-in magazine still hidden; prop parented away from `Magwell` |
| `MagPresented` | Incoming magazine prop visible in the character's hand, approaching the magwell. Weapon well still empty. | prop on the character hand socket |
| `MagInCommitted` | Prop reaches the magwell and is released; the built-in magazine geometry is restored **as** the prop is hidden. This is the visual twin of the inventory's atomic swap — it must be idempotent, because a repeated notify must not produce two magazines. | `magazine` / `UE4_M16_MagazMod` unhidden; prop hidden |
| `MechanismReady` | Bolt/charging handle released to battery, catch returned to rest, weapon in its ready pose. Required visually even if the first gameplay model has no separate chamber round. | `charginghandle`, `catch` returned to reference pose |
| `ActionEnded` / `Cancelled` | The rig returns to a **defined** resting pose for the current state: magazine visible if one is inserted, hidden if not; bolt forward; trigger, selector, catch at rest. Cancellation before `MagInCommitted` restores the pre-action visibility; cancellation after it keeps the completed state. No half-detached prop may survive. | all mechanism bones; prop visibility reset |

Two invariants the weapon side owns regardless of timeline: **(a)** exactly one magazine visible at any instant, and
**(b)** a cosmetic weapon notify never debits a round or commits a magazine swap — it only changes pose and visibility.

---

## 7. Missing work, classified

### 7.1 Asset authoring
- **Mechanism clips for the 27-bone M16 rig**: fire cycle, reload beats (mag-out, mag-in, bolt release), charging-handle
  pull, and an empty/bolt-locked hold. Nothing reusable exists (§3.1); donors need retargeting or re-authoring (§3.3).
- Alternative to clips: a small data-driven bone-animation layer driving the six mechanism bones from action phase. Cheaper
  to build, less expressive. Decision not made here.
- Optional: a magazine prop asset in the AZ tree if the user does not want a cross-tree reference to
  `/Game/Assets/M16/mesh/UE4_M16_MagazMod` (§5.1).

### 7.2 Rig / socket adjustment
- Add `Muzzle`, `CasingEject`, `Magwell` to `/Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton` with visually validated
  transforms (§4.3). Sockets live on the skeleton, so this is one asset.
- Decide whether `MuzzleSocketName` stays on `UAZ_GA_Shoot` or moves to the weapon definition (§4.2).

### 7.3 Runtime integration
- Assign a weapon AnimBP (or post-process AnimBP) to `WeaponMesh3P` — today `anim_class` and
  `post_process_anim_blueprint` are both `None`, so the rig cannot move at all (§1.3).
- Implement the §6 contract as a coordination surface between the character action and the weapon rig, with idempotent
  `MagInCommitted`.
- Implement magazine hide/show via `HideBoneByName` on `WeaponMesh3P` plus prop attach/detach (§5.2).
- Guard the unguarded fire-audio socket call (`AZ_GA_Shoot.cpp:185`) or ensure the socket exists.
- Resolve `BP_Inv_EquipActor_Rifle_M16`'s duplicate skeletal mesh components (§1.4).

### 7.4 User visual verification
- **Skin weighting of the six mechanism bones** (§2.3) — the one item that blocks a confident answer on magazine hiding.
- Placement of `Muzzle`, `CasingEject`, `Magwell` (§4.3).
- Whether `UE4_M16_MagazMod` visually matches the magazine currently on the assembled rifle (§5.1).
- `BP_Inv_EquipActor_Rifle_M16` double-mesh check (§1.4).

### 7.5 Ready / needs-preparation / unknown matrix

| Item | State | Evidence |
|---|---|---|
| Weapon BP, active mesh component, skeleton identity | **Ready** | §1.1, §1.2 |
| Mechanism bones exist with per-part hierarchy | **Ready** | §2.1, §2.2 |
| Separate magazine mesh exists | **Ready** (outside the AZ tree) | §5.1 |
| `LeftHandGrip` / `LeftHandGripAim` sockets | **Ready** | §4.1 |
| Weapon AnimBP / any driving animation | **Needs preparation** — none assigned, none exists | §1.3, §3.1 |
| M16 mechanism clips | **Needs preparation** — 0 found across 11,148 assets / 141 skeletons | §3.1 |
| `Muzzle`, `CasingEject`, `Magwell` sockets | **Needs preparation** — verified absent | §4.1 |
| Magazine hide/show mechanism | **Needs preparation**, method conditional on weighting | §5.2 |
| Skin weighting of mechanism bones | **UNKNOWN** — not exposed by any available API | §2.3 |
| Section↔part mapping of the 10 material slots | **UNKNOWN** — only needed if bone hiding fails | §5.2 |
| Whether donor clips can be retargeted economically | **UNKNOWN** — needs a retarget trial, not authorized here | §3.3 |

---

## 8. Handoff

**Verified reusable assets**
- `/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle` with `WeaponMesh3P` → `/Game/AZ/Assets/M16/SKL/M16_Skeleton` is the correct and
  only rendering path; the 1P and item-base mesh components are empty and can be ignored (not removed) for now.
- The 27-bone rig is a well-formed rigid-parts mechanism rig: `trigger`, `selector`, `catch`, `charginghandle`, `ejector`,
  `magazine` each own their geometry, with usable pivots recorded in §2.2.
- `LeftHandGrip` / `LeftHandGripAim` on `M16_Skeleton_Skeleton`, already consumed by `AAZ_Weapon::GetLeftHandSocket`.
- `/Game/Assets/M16/mesh/UE4_M16_MagazMod` is a usable detachable-magazine prop, matching the magazine on the assembled gun.

**Concrete missing items**
- No weapon AnimBP and no post-process AnimBP → the rig is inert today.
- No mechanism animation for this rig anywhere in the project (exhaustive result, §3.1). Donors exist but none is
  name-compatible; the closest, the AK12 from the same vendor family, uses `clip`/`recharge`/`safety_switch`/`lock`.
- No `Muzzle`, `CasingEject` or `Magwell` socket — and `UAZ_GA_Shoot` already depends on `Muzzle`, degrading silently
  (flash skipped, fire audio at the pivot, trace start fallback).
- No magazine hide/show implementation, so a prop today would render alongside the built-in magazine.

**Proposed socket/mechanism contract**
- Reuse `LeftHandGrip`/`LeftHandGripAim`; add `Muzzle` on `UE4_M16_CompensatorMod`, `CasingEject` on `ejector`, `Magwell`
  on `magazine` — all three transforms are proposals pending visual placement.
- Presentation contract for `ShotAccepted`, `MagOut`, `MagStowed`, `MagPresented`, `MagInCommitted`, `MechanismReady`,
  `ActionEnded/Cancelled` in §6, with the two invariants: one visible magazine at a time, and cosmetic notifies never
  change ammunition state.

**Checks still requiring the user's preview**
1. Skin weighting of the six mechanism bones in `M16_Skeleton` (§2.3) — decides the magazine-hiding method.
2. Visual placement of the three proposed sockets (§4.3).
3. `UE4_M16_MagazMod` visual match against the rifle's built-in magazine (§5.1).
4. `BP_Inv_EquipActor_Rifle_M16`'s two identical skeletal mesh components (§1.4).

No runtime wiring is claimed or authorized by this audit.
