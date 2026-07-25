---
name: feedback_no_hardcoded_asset_paths
description: "USER RULE 2026-07-24: NO hardcoded /Game/ asset paths in C++ — ever. All content refs (montages, curves, BP ability classes) are EDITOR-ASSIGNED UPROPERTYs; C++ keeps only native-class fallbacks (code refs OK) and graceful degradation when unset."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-24T18:24:27.977Z
---

# No hardcoded asset paths in C++

**Why:** the user banned them explicitly ("no more hard coded assignment change all of them let's
assign in UE Editor") after seeing `FSoftObjectPath(TEXT("/Game/..."))` ctor defaults and
`LoadClass(TEXT("/Game/..._C"))` grant resolution in the grab feature.

**How to apply:**
- Content assets (montages, curves, meshes, DAs): `UPROPERTY(EditDefaultsOnly)` soft/hard refs, value
  set in the owning BP asset's defaults. NEVER a path string in a constructor.
- Ability/effect classes: `UPROPERTY(EditDefaultsOnly) TSubclassOf<...>` on the granting pawn/actor,
  set in the pawn BP. Ctor may default to the NATIVE class (a code reference, not a content path) as
  fallback; grant sites use the assigned class and patch ITS CDO (see the BP-child tag-container
  lesson in [[project_grab_grapple_design]] §7b).
- Every such property must DEGRADE GRACEFULLY when unset (fallback class / skip feature / scale 1).
- Verify with `grep -r "/Game/" Source/` — only comments may match.
- ★ SCRIPTED BP-DEFAULT EDITS ON ABILITY (GA) CLASSES: `set_editor_property` on the CDO alone is NOT
  enough — runtime ability instances (`NewObject(Owner, Class)` in CreateNewInstanceOfAbility) copy the
  GENERATED CLASS defaults, which only re-bake on COMPILE. Always follow the CDO write with
  `unreal.BlueprintEditorLibrary.compile_blueprint(bp)` + save (regular BPs only — NEVER AnimBPs, GC
  crash rule). Proven 2026-07-24: pool assigned+saved+verified on CDO, instance saw an EMPTY array
  until the compile. Pawn/actor BP CDO edits (spawned-actor path) propagated without compile, but
  compile-after-write is the safe default everywhere.
- Current assignments live in: Chalkie pawn BP (Death/Melee/GrabAbilityClass), hero pawn BP
  (GrabbedAbilityClass), BP_GA_PlayerGrabbed (StruggleMontage, ShakeIntensityCurve).
