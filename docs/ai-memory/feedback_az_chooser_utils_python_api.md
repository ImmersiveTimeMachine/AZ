---
name: AZ_ChooserUtils Python API quirks (dump returns Array, set_cell_bool takes int32)
description: Two non-obvious gotchas when scripting CHT_*  edits via unreal.AZ_ChooserUtils — return type mismatches between docstring and reality. Hit twice during chooser column add for bLeftFootDown.
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# AZ_ChooserUtils Python API gotchas

## Rule
When scripting against `unreal.AZ_ChooserUtils` (NOT `unreal.AZChooserUtils` — note the underscore):

1. **`dump_chooser_full_tree(chooser_path)` returns `Array[str]`, not `str`.** Join with `"\n".join(arr)` before writing to a file.
2. **`set_cell_bool_on_sub(..., value)` takes `int32`, NOT `bool`** — despite the docstring saying `value`. Pass `0` for False, `1` for True. Passing Python `True`/`False` raises:
   `TypeError: NativizeProperty: Cannot nativize 'bool' as 'Value' (IntProperty)`
3. **`get_row_count(chooser_path)` takes a `string` path**, NOT a UObject. Same applies for all `*_on_sub` and `inspect_chooser_columns` helpers — they expect the asset path string, with the engine doing the load internally.
4. **`add_bool_column_to_sub(root_path, sub_name, property_name)` returns the new column index (`int32`).** Sub-chooser is identified by its name (the `name` field in `dump_chooser_full_tree` output), not its UObject reference.

## Why
`AZ_ChooserUtils` is exposed via `UFUNCTION(BlueprintCallable)` and the Python bindings nativize the parameter types based on the C++ signature. The engine signed `Value` as `int32` for tri-state (0=False, 1=True, 2=Any) — Python's bool autoconversion doesn't apply because IntProperty has no implicit bool→int coercion. Likewise `dump_chooser_full_tree` returns `TArray<FString>` so it nativizes to a Python list, not a single string.

## How to apply
Skeleton for any future chooser-column edit task:
```python
import unreal, re, os

util = unreal.AZ_ChooserUtils
cht_path = "/Game/.../CHT_X.CHT_X"

# Dump:
arr = util.dump_chooser_full_tree(cht_path)
text = "\n".join(str(s) for s in arr)

# Add column:
col_idx = util.add_bool_column_to_sub(cht_path, "Sub Name", "bMyProp")

# Populate (use 0/1, NOT False/True):
ok = util.set_cell_bool_on_sub(cht_path, "Sub Name", row_idx, col_idx, 1)  # True

# Always finish with:
util.compile_and_save(cht_path)
```

## Origin incident
2026-05-04, branch `feature/rootmotion`. Adding `bLeftFootDown` column to 6 sub-choosers in `CHT_AZ_CharacterAnimations` to break L/R chooser tie on `_LU/_RU` stop variants. First failure: `dump_chooser_full_tree(...).write()` blew up because return type is Array, not str. Second failure: `set_cell_bool_on_sub(..., True)` threw nativization error because `Value` param is int32. Both fixed by reading the actual signatures via `__doc__` rather than trusting variable names.
