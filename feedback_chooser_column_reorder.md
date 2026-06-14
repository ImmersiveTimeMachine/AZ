---
name: feedback_chooser_column_reorder
description: Chooser scripted-edit gotchas — columns REORDER on reload (re-inspect indices every session); setting a BOOL cell RE-SORTS ROWS IMMEDIATELY (re-locate rows BY ASSET before every set); OutputStruct field edits can read back True in-memory yet NOT persist (verify via reload_packages); check every set_cell return value.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

When the Chooser table editor **reloads** a `UChooserTable` asset (editor restart, or fresh load after save), it **reorders columns so every `OutputStructColumn` comes AFTER all input columns**. Any input column you appended *after* an output column with `UAZ_ChooserUtils.add_*_column_to_sub` gets **renumbered** on the next load.

**Why:** `add_enum_column_to_sub` / `add_bool_column_to_sub` append to the end of the column array, so a new input column lands after the output column in-session. On reload the editor normalizes order (inputs first, outputs last), shifting indices.

**Concrete hit (CHT_v2_CharacterAnimations, 2026-06-01):** appended `StartDirection`(was 7) and `bMovingTransition`(was 8) *after* `OutputStruct`(6). After an editor restart the layout became `6 StartDirection | 7 bMovingTransition | 8 OutputStruct | 9 bJustLanded`. A later script that hardcoded the old indices (6=output, 7=StartDir, 8=movingTransition) set cells on the WRONG columns.

**Why it didn't corrupt (but silently failed):** `set_cell_*_on_sub` validates the column TYPE — `set_cell_output_struct_field_on_sub` on an EnumColumn, `set_cell_bool_on_sub` on an OutputStructColumn, `set_cell_enum_on_sub` on a BoolColumn all **return False and no-op** (no corruption), leaving those cells at their `add_empty_row` DEFAULTS. The `ERRORS:[]` looked clean only because the new-row `set_cell` return values weren't being checked.

**Rules:**
- **ALWAYS re-`inspect_chooser_columns` (or `dump_chooser_full_tree`) in the CURRENT session and read indices from THAT** before any `set_cell_*_on_sub(... colIndex ...)`. Never reuse an index remembered from a prior session/dump.
- **Check every `set_cell_*` return value** — a `False` means a type/index mismatch (wrong column), not just "no change."
- Cell DATA survives the reorder (cells move with their column), so existing rows stay correct; only by-index writes in NEW code break.
- Want stable indices? Add input columns BEFORE the output column, or just always read indices live.

## ROW-level gotchas (learned 2026-06-06/07, the jump-foot saga)

- **Setting a BOOL input cell RE-SORTS rows IMMEDIATELY.** A batch script that sets row 31 then row 30 by pre-computed indices can hit a row that MOVED after the first set (concrete hit: the walk takeoff pair ended `(False→LU, True→RU)` after a two-set batch). **Rule: re-locate the target row BY ASSET NAME (fresh `dump_chooser_full_tree`) before EVERY `set_cell_*` call** — never set two cells from one dump.
- **OutputStruct field edits can silently fail to persist.** `set_cell_output_struct_field_on_sub('bUseMM','True')` returned True, the post-`compile_and_save` in-memory dump read True — and after an editor restart the DISK value was False (while a BlendTime set in the same batch DID persist). **Rule: after any chooser edit batch, `EditorLoadingAndSavingUtils.reload_packages([pkg])` and re-dump — verify the SERIALIZED state, not memory.** Repeat the edit if it didn't stick.
- Editing output-struct fields on one row has been observed to perturb OTHER rows' fields (BT values drifting on untouched rows). Always re-verify the whole affected section after a batch.

## ★ ENUM-CELL silent no-op when the column's cached enum is null (learned 2026-06-09, crouch rows)
- **`set_cell_enum_on_sub` resolves the value name via `Col->InputValue->GetEnum()` (the editor-cached `FEnumContextProperty::Binding.Enum`). If that enum is NULL it SKIPS the value assignment but STILL `return true`** (`AZ_ChooserUtils.cpp:1599-1606`) — the cell stays at the add-empty-row default (0). The `dump_chooser_full_tree` enum-name + value also read through the same `GetEnum()`, so a null-enum column prints `EnumColumn ()` (empty type) and rows show raw ints.
- **`Binding.Enum` is null when the chooser has NOT been compiled THIS editor session yet** (cached editor-only data, not reliably reloaded). Concrete hit: CHT_v2 column 0 (SMState) dumped `EnumColumn ()` on a fresh session → ALL 12 crouch-row `set_cell_enum_on_sub(c0,...)` silently no-op'd to 0, even though c1/c2/c3/c6 (already-cached enums) set fine. The closing `compile_and_save` THEN repopulated c0's enum — too late for the sets that already ran.
- **Rule: before name-setting enum cells, force the column's enum to resolve.** Either (a) `compile_and_save` (or `set_column_binding_chain(col, chain, ctx)` which Compiles) ONCE up front, then verify `inspect_chooser_columns()[col]` shows the enum TYPE (not `()`), THEN do the name-sets; or (b) always dump/inspect after the batch and re-set any column that came back `= 0`/raw-int. `RebindChooserEnums` CANNOT fix this — it only swaps a NON-null `Binding.Enum` (`:964`).
- Standing rows escaped this only because their c0 was set by `AddAnimRow` (raw `uint8` value, no enum lookup — `:145`); but `AddAnimRow`/`AddNestedChooserRow` assume a stale 3-column `SM|Gait|Stance` layout and only append cells to cols 0-2 → would leave cols 3-N ragged on the current 11-column CHT_v2. Do NOT use them to author rows now.

See [[project_v2_locomotion_progress]] (jump arc milestone), [[project_jump_system_status]] (the foot saga where these hit), [[project_crouch_system]] (the crouch-row build that surfaced the null-enum trap), and the chooser tooling in skill `az-cpp-utility-tools`.
