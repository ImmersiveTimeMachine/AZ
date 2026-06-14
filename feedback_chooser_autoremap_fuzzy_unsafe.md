---
name: feedback_chooser_autoremap_fuzzy_unsafe
description: "AZ_ChooserUtils.AutoRemapChooserAssets fuzzy tier is UNSAFE for bulk anim-set swaps — it's overlap-coefficient (not Jaccard), picks the FIRST max-scoring candidate so it biases toward longer/prefixed variants (Crouch_X over X) and drops _new suffixes. Always dry-run; prefer an explicit RemapChooserAssets map."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

`UAZ_ChooserUtils.AutoRemapChooserAssets(ChooserPath, SearchPaths, FuzzyThreshold, bDryRun) -> (count, OutReport)` (out-param `OutReport` comes back in the Python tuple — do NOT pass it positionally; passing 5 args throws "takes at most 4 arguments").

**Why the fuzzy tier is unsafe (verified `AZ_ChooserUtils.cpp` ~2013-2040):** Tier-2 scores with `OverlapSimilarity = |intersection| / min(|A|,|B|)` (NOT Jaccard, despite the skill text), and keeps the FIRST candidate that ties the max score (`if (Score > BestScore)`). So when both `AnimPro_X` and `AnimPro_Crouch_X` exist, the source `{walk,fwd,start}` scores **1.000** against BOTH (`{...}` ⊆ `{crouch,...}` → min-size denominator) and whichever comes first in the asset-registry iteration wins — observed to be the **Crouch_** (longer) variant. It also drops `_new` (`Crouch_WalkFwdStop_RU_new → Crouch_WalkFwdStop_RU`).

**Concrete (2026-06-14, CHT_v2 RTG_RM_* → MovementAnimsetPro AnimPro_*):** dry-run mis-mapped `RTG_RM_WalkFwdLoop→AnimPro_Crouch_WalkFwdLoop`, `RTG_RM_Idle→AnimPro_Crouch_Idle`, `RTG_RM_WalkFwdStart→AnimPro_Crouch_WalkFwdStart`, `WalkFwdStart90/180_L/R→Crouch_*`, and forced the GASP idle-breaks onto `AnimPro_Idle`. ~10 of 53 fuzzy hits were wrong.

**Doctrine for "swap to the same animset, newer version":**
1. Run AutoRemap with `bDryRun=True` ONLY as a recon dump (it enumerates every `FAssetChooser` ref via `CollectAllTables`→`ResultsStructs`).
2. Build an EXPLICIT `{currentAssetName: fullTargetPath}` map by exact suffix, then call `RemapChooserAssets(ChooserPath, FromNames, ToFullPaths)` — it matches `AC->Asset->GetName()` exactly, replaces all rows, and `Compile(true)`s (no save — follow with `CompileAndSave`). Both functions traverse the IDENTICAL ref locations (single-asset `FAssetChooser` results), so the explicit map covers everything the dry-run saw.
3. Pre-flight `load_asset` every target; abort without mutating if any is missing.
4. Self-verify: re-run AutoRemap dry-run after — swapped rows should now read `EXACT 1.000 AnimPro_X → AnimPro_X`.

**Scope trap:** the CHT is only the chooser. For `bUseMM=true` rows the played frame comes from the PoseSearch DB, so the matching `PSD_v2_*` databases must be swapped too or the on-screen anim won't change. See [[feedback_chooser_column_reorder]], skill `az-cpp-utility-tools`, [[reference_cht_chooser_structure]].
