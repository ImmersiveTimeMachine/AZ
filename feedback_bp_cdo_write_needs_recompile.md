---
name: feedback_bp_cdo_write_needs_recompile
description: "★★ A scripted/raw property write on a BP-generated class's CDO does NOT reach new INSTANCES until the Blueprint is RECOMPILED — UBlueprintGeneratedClass initializes instances from CustomPropertyListForPostConstruction (a CDO-vs-native diff cached at COMPILE time), not a full CDO copy. Save alone is not enough. Recipe: set property -> BlueprintEditorLibrary.compile_blueprint (non-AnimBP only!) -> save -> verify with unreal.new_object(cls)."
metadata:
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T20:56:50.541Z
---

# BP CDO writes are invisible to instances until the BP recompiles (2026-08-31)

**Symptom:** set `CatchDatabase` on `BP_GA_ChalkieGrab`'s CDO via ObjectTools; read-back on the CDO
shows the value; the BP was even SAVED (asset on disk carries the reference) — yet every granted
ability instance activated with `CatchDatabase = null`, while `GetClass()->GetDefaultObject()` in the
same frame showed the value. Three PIE runs wasted on save/stale-CDO theories (two-strike hit).

**Mechanism (reproduced OUTSIDE GAS in one call):** `unreal.new_object(BP_GA_ChalkieGrab_C)` gave
`PairedGrabMontage = AM_Grab_Chalkie` (set via editor long ago) but `CatchDatabase = None`.
`UBlueprintGeneratedClass` does NOT initialize instances by copying the whole CDO: it runs the native
constructor plus **`CustomPropertyListForPostConstruction`** — a cached list of properties whose CDO
values differ from the native parent, built at **BP COMPILE time**
(`UpdateCustomPropertyListForPostConstruction`). A raw CDO write after the last compile is not in the
list → new instances silently get the native default. Saving serializes the CDO but does NOT rebuild
the list.

**Why:** this is the sharper form of the 2026-07-24 ActivationOwnedTags scar ("runtime CDO patches
don't survive into instances of BP tuning children") — it applies to ANY property, not just GAS tag
containers, and save does not fix it.

**How to apply:**
1. After ANY scripted property write on a BP CDO (ObjectTools set_properties, python
   set_editor_property): `unreal.BlueprintEditorLibrary.compile_blueprint(bp)` then save.
   **NEVER on AnimBPs** — the python GC crash rule stands; for AnimBPs the user compiles.
2. **Verify with the instance, not the CDO:** `unreal.new_object(cls).get_editor_property(prop)` is
   the 5-second truth test — the CDO read-back proves nothing about what instances will get.
3. Native-parent C++ default changes have the same shape (doctrine rule 1): BP children need a
   recompile to pick up re-baselined defaults.

Related: [[feedback_verify_never_presume]], [[feedback_stop_the_patch_loop]],
[[project_grab_grapple_design]], [[feedback_python_gc_crash]].
