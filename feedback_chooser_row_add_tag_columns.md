---
name: feedback_chooser_row_add_tag_columns
description: "★★ Adding a chooser row: AddEmptyRowToSub did NOT extend GameplayTag columns, and FGameplayTagColumn::TestRow returns FALSE for a missing cell -> the row is silently filtered out and can NEVER match. Fixed 2026-09-07; prefer DuplicateRowOnSub, and DumpChooserFullTree now prints each column's bound property name."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-08T02:03:13.167Z
---

# Adding rows to a wide chooser table (CHT_v2)

## The silent killer: a row shorter than its columns never matches

`UAZ_ChooserUtils::AddEmptyRowToSub` extended cells only for the column types its `if/else` chain knew
(Enum, MultiEnum, FloatRange, Bool, Randomize, OutputStruct). **`FGameplayTagColumn` was missing** — and
`CHT_v2_CharacterAnimations` has four of them (c8, c9, c17, c18).

A row added that way is SHORTER than those columns, and the engine's guard is:

```cpp
bool FGameplayTagColumn::TestRow(int32 RowIndex, ...) const
{
    if (RowValues.IsValidIndex(RowIndex)) { ... }
    return false;          // <-- missing cell = row FAILS the filter
}
```

So the row is filtered out on **every** evaluation and can never be selected. **No error, no warning, no
log line** — it just never fires. Cost: this would have silently swallowed the rifle InAirLoop rows.

**Fixed 2026-09-07** by padding every column generically after the type switch:

```cpp
for (FInstancedStruct& ColStruct : Table->ColumnsStructs)
    if (FChooserColumnBase* Col = ColStruct.GetMutablePtr<FChooserColumnBase>())
        Col->SetNumRows(Table->ResultsStructs.Num());
```

Both callers add their result BEFORE calling the filler, so `ResultsStructs.Num()` is the intended new
count and nothing is truncated.

## Prefer `DuplicateRowOnSub` (added 2026-09-07)

`UAZ_ChooserUtils::DuplicateRowOnSub(chooser, sub, SourceRowIndex) -> int32` appends an exact copy of an
existing row — result **plus every column cell, tag columns included** — via the engine row virtuals
`SetNumRows` / `CopyRow` (implemented for every column type by `CHOOSER_COLUMN_BOILERPLATE`). No per-type
switch, so **no column type can be missed**. Then set only the cells that differ with `SetCell*`.

Use it whenever a new row must inherit an existing row's gating (weapon tag, stance, bool flags). It turns
"reproduce this row's 21 cells correctly" into "change the two that differ".

## `DumpChooserFullTree` now prints the bound property name

Each column line carries `"bind"` — the last element of its `PropertyBindingChain`, via
`FChooserParameterBase::GetDebugName()`. Before this, a bool column was an anonymous `"c11"` and every row
edit against it was guesswork.

**This immediately paid off:** it revealed `c13` = `Is Moving`, and that the rifle takeoff rows 280/290
have `c13=False` — they are the *standing* variants. A plain duplicate would have restricted the new air
rows to standing falls and left every running jump frozen. Widened `c13` and `c1` to Any instead.

CHT_v2 column map (read from the dump, 2026-09-07):

```
c0 SMState · c1 Stance · c2 Gait · c3 MovementDirection · c4 Left Foot Down · c5 MovementDirection8
c6 bIsAiming · c7 SMState(multi) · c8/c9 OwnedTags · c10 Start Direction · c11 Moving Transition
c12 Just Landed · c13 Is Moving · c14 bStrafe · c15 Reaction · c16 MovementDirection(multi)
c17/c18 OwnedTags · c19 Randomize · c20 AZ Chooser Outputs
```

## Other rules that still apply

- `AddAnimRow` **refuses** any table that is not exactly 3 enum columns (it appends to columns 0-2 only and
  would misalign every later row). On CHT_v2 use `DuplicateRowOnSub`, or `AddEmptyRowToSub` + `SetCell*`.
- Locate source rows **by output asset, never by a hard-coded index** — other agents edit this table.
- An empty tag cell PASSES (`RowValues[i].IsEmpty()` → `!bInvertMatchingLogic`), so a padded cell means
  "match any", not "match nothing".
- Row order is evaluation order, first match wins. New rows append at the end — check no earlier row with
  an `Any` c0 can shadow them (in CHT_v2 only rows 103-106 have `Any` on c0, and c7 pins them to
  IdleLoop|IdleBreak).

See [[feedback_az_chooser_utils_python_api]], [[feedback_chooser_column_reorder]],
[[reference_cht_chooser_structure]], [[project_jump_system_status]],
[[feedback_python_save_only_if_dirty]].
