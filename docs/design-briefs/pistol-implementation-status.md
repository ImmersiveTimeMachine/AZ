# Pistol addition to the existing firearm system

2026-09-11. The user approved the plan and requested implementation. Gameplay
testing remains user-run; no automated tests, PIE, or editor tests are authorized.

## Current state

**Visual correction completed later in the same session:** the user correctly
reported four added pickups rendering rifles on the ground. Initial readback had
checked UE5.8's transient SkeletalMeshAsset editor alias; NEVER-notified assignment
left the real SkinnedAsset=M16. The equipped pistol CDO had the inverse mismatch
(editor aliasPistols_B, real assetNone). Native mesh setters now repair the pistol
weapon CDO, all four pickup CDOs, placed instances and existing preview actors.
Five Blueprints compiled and saved; L_001 saved. Native getters and live previews
confirm one pistol plus three magazines, with the original rifle preserved.
No native rebuild is required. The earlier alias-based visual checks below are
superseded by Saved/PistolVisualFix/verify.json and the corrected inventory receipt.

PistolB_Ammo was also visually identified as a cartridge casing. Magazine pickups
now use /Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/SM_Pistol_Magazine,
extracted from the actual pistol Clip_Bone:559vertices/618triangles, original UVs
and material, centered pivot, one box collision. Source pack bytes are unchanged.
The helper is Tools/pistol_magazine_mesh_setup.py; targeted reference repair is
Tools/pistol_visual_fix.py. The regular inventory author helper now uses native
skeletal setters/getters so reruns cannot recreate the bug. Gameplay manifests,
round counts, recoil, abilities and socket tuning were unchanged by this repair.
Ground visual confirmation: Saved/Screenshots/WindowsEditor/RiderMCP/
20260911-202459_viewport.png. No PIE or gameplay tests were started for this fix.

Implemented, built, authored and saved. The user's full build succeeded in37.17s;
main DLL19:50:50UTC September11, editor PID28024 started19:50:59UTC. New native
properties were read back successfully before asset authoring. No further build
or restart is required for this pistol slice. Gameplay acceptance is user-run.

All eight pistol Blueprints and the shared MHC AnimBP compiled successfully.
Inventory manifest/initial-magazine/ability readback passed after saving. The
main chooser has402rows (308preserved +94pistol rows),86owned animation copies,
two aim offsets and four databases. All four PoseSearch BuildIndex Succeeded
messages are recorded at19:57:33–34UTC, with indexes stored to DDC.

Four persistent pickups are saved in L_001 beside the first rifle pickup:
AZ_Pistol_Pickup at(450,40,240), full magazine(450,100,240), partial(450,160,240),
empty(450,220,240). Each has ownership tag AZ.Pistol.Setup.v1 and a stable actor
name/label. Map backup: Saved/Backups/PistolLevel/20260911T155827/L_001.umap.
Existing rifle placements, quick-slot assignments and source animations remain.

Final readback confirmed all eight BPs UpToDate, no dirty pistol packages or map,
and no notify events on the hero fire/reload or clean weapon firing clip. An
unrelated dirty hero Blueprint was left untouched. The user started Play while
placement was underway; Codex announced and stopped those sessions to finish
the editor-side save, but did not start PIE or invoke gameplay tests. The user
was already in Play again during final read-only checks.

Readback helpers were corrected for two authoring-only issues: graph binding now
audits the graph independently of the chooser templates that were already gated;
manifest comparison normalizes only FText localization identities assigned on
save, retaining source text and all gameplay fields. Raw saved manifests remain
in the receipts. No native rebuild was needed for those helper corrections.

When Play is running, EditorAssetLibrary existence/metadata helpers may report
false or no editor world. The final check used direct asset/editor-object paths
and the earlier verified saved manifests, rather than treating that as data loss.
The final receipt is Saved/Pistol/final-readback.json. Weapon mesh firing samples
at0/.1/.2333s preserve the actual mesh grip (zero translation deviation, about
.06degree compressed rotation difference), avoiding the skeleton-reference axis trap.

Presentation source assets:

- /Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/SM_Pistol_Pickup: solo pistol
  extracted from Pistols_B through GeometryScript, copied source materials and
  simple box collision. Source metadata AZ.Pistol.PickupMeshSource is set.
- /Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Pistol:512x384 transparent white mesh
  silhouette, UI compression/group, no mipmaps, NeverStream. The supplied pack's
  Pistol_Pickup is crossed pistols, so the solo mesh is used for both icon and pickup.

Native editable art is at
C:/UnrealEngine/Games/AZ/UI Design/CHALK_HUD_v03/unreal-art/CHALK_Pistol_NATIVE.xcf.
Tools/pistol_mesh_icon_geometry.py derives contours from exported solo mesh OBJ;
Tools/pistol_icon_export_art.py renders those contours as a native GIMP vector
layer and PNG. The final solo-pistol PNG was visually inspected. An initial
crossed-pistol silhouette was superseded before importing the texture.

## Shared native changes

- Weapon animation profile StandingAimPose/CrouchingAimPose; Mover anim instance
  WeaponStandingAimPose/WeaponCrouchingAimPose game-thread snapshots for property
  bindings. New profiles replace old torso/AO state; missing sources fail closed.
  The existing rifle's two literal aim poses MUST be assigned to its profile
  before the shared graph bindings are changed or gameplay is attempted.
- Item.Type.Magazine.Pistol native tag. Existing Weapon.Pistol and pistol item
  tags are reused.
- Optional WeaponState.CascadeMuzzleFlash and replicated cosmetic mirror;
  existing Niagara takes priority if both are present. The pack's pistol flash
  is Cascade.
- Optional AAZ_Weapon.WeaponMeshFireAnimation, played once by the accepted-shot
  cosmetic path after skeleton compatibility validation. No new ammo owner.
- Inventory action now says Load into weapon; generic missing-name fallback.

## Authoring sequence after successful full build/restart

Run through Rider ue_execute_python using runpy.run_path(..., run_name='...'),
then call the returned main function. Do not run unguarded rifle foundation code.

1. Tools/pistol_socket_setup.py main('audit'), then main('author'), main('verify').
   Root already ran the read-only socket audit successfully. It appends only
   RightHandPistolSocketAim, RightHandPistolSocketRelaxed, PistolHolsterSocket to
   metahuman_base_skel, preserving the actual hero mesh's nine existing sockets.
2. Tools/pistol_animation_setup.py main(stage='content'), then 'chooser', then
   'graph'. Use fresh dynamic audit: another workstream changed current chooser
   from304 to308rows during preparation. Latest audit plans94 appended rows,
   86owned clips,25measured loops,4BranchIn DBs and2AimOffsets. Never overwrite
   incoming concurrent edits or use fixed historical row counts.
3. Compile and save AZ_ABP_MoverHero_MHC using native tools after Python returns.
   Do NOT compile/reconstruct/save this AnimBP inside ordinary Python.
4. Build the four new PoseSearch database indexes and verify them. Run animation
   helper stage='verify'. No PIE is needed for asset compilation/index generation.
5. Tools/pistol_inventory_setup.py main('audit'), then main('author'). Compile the
   returned eight full Blueprint object paths with native tools, then main('save')
   and main('verify'). Author creates pistol actor, three thin native-ability BPs,
   pistol pickup and full/partial/empty magazine pickups. It also makes the clean
   weapon fire clip. It does not spawn pickups or change quick-slot assignments.
6. Place labelled pistol/full/partial/empty pickups near the existing rifle pickup
   area in L_001, with idempotent ownership labels and a map backup; save the map.
   Existing authored examples and quick slots must be preserved.
7. Complete saved readback, verify rifle preservation and report user gameplay
   checks. No test assets or automation harnesses should be added.

## Configuration and content facts

- Weapon actor /Game/AZ/Blueprints/Weapon/AZ_BP_Pistol, single-only mode; existing
  Aim/Fire/Reload native classes with pistol-specific BP required tags. All three
  existing rifle BP ability CDOs still require Weapon.Rifle and remain unchanged.
- PistolB.Standard magazine family, initial editable gameplay capacity15,
  partial pickup7, empty0; nonstackable1x1magazines and2x2pistol. Capacity is a
  gameplay default, not a real-world model claim. Inventory owns actual remaining
  rounds and preserves circular reload, exact selected load, auto skipping empties,
  and drop/re-pickup persistence. Rifle magazines cannot load into the pistol.
- Existing reticle visual definition is reused; pistol source controls spread/
  recoil. Current HUD intentionally includes compatible nonempty spare count and
  magazine icon, per the user's later correction. Do not revive hidden-MAGS policy.
- Existing manual quick-select assignments stay manual; pickup does not assign
  or activate a weapon. Pistol sprint stays in the authored relaxed hand posture.
- Actual Pistols_B mesh points+Y, although shared skeleton-only reference suggests
  +X. Socket helper uses actual mesh reference + MuzzleFlash frame. Unit scale and
  identity WeaponMesh3P component transform are required. Its trigger is aligned
  to the aimed index finger; same hand-relative grip lowers with relaxed wrist.
- Source Fire_Pistol_W=.233333s, RateScale1, contains built-in sound/flash notifies.
  Author an owned AS_Pistol_WeaponFire copy and remove those notifies, otherwise
  they double the shared accepted-shot effects. Original pack is preserved.
- All214 imported hero sequences reference the MetaHuman skeleton. Canonical
  unsuffixed variants are in place, suffix variants commonly enable root motion.
  Some additive bases still reference UE4 original-pack assets; owned copies repair
  those bases. Imports remain untouched.
- Standing ShootOnce=.8s and Reload_2=2.1667s provide masked actions for both stances
  initially; crouch starts are close by measured local upper-body pose. Crouched
  fire/reload grip still needs visual user review. No procedural hero IK is enabled.
- Current aimed turn-in-place spring owns capsule yaw: use in-place pistol turn
  loops for standing aim. No crouch pistol stepping loop exists; crouch idle is the
  explicit initial fallback while the capsule turns. Other unsupported starts/
  pivots preserve appropriate existing shared transitions.

Receipts are under Saved/Pistol, Saved/PistolSockets, Saved/PistolAnimation, and
Saved/PistolInventory. User checks next: pick up/equip pistol, aim/single fire,
cycle/select magazines, empty auto reload, switch rifle/pistol, and verify retained
rounds on drop/re-pickup. Assess standing/crouched grip and shared-transition feel.
