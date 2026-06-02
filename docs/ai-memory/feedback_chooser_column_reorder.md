---
name: feedback_chooser_column_reorder
description: Chooser editor REORDERS columns on asset reload (OutputStruct trails input columns) — re-inspect indices every session before SetCell-by-index; never reuse prior-session indices.
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

See [[project_v2_locomotion_progress]] (jump arc milestone) and the chooser tooling in skill `az-cpp-utility-tools`.
