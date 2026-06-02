---
name: feedback_file_paths
description: ALWAYS use full absolute paths (C:\UnrealEngine\...) when referencing files — user clicks them in Rider IDE
type: feedback
---

ALWAYS show full absolute paths when referencing files. Never use relative paths or shortened paths.

**Why:** User works in Rider IDE which makes full paths clickable for instant navigation. Relative paths like `Source\AZ\Public\...` don't work — must be `C:\UnrealEngine\Games\AZ\Source\AZ\Public\...`.

**How to apply:** Every single time you mention a file in text output — edits, build errors, "I updated file X", references — use the full `C:\UnrealEngine\...` path. No exceptions. This applies even when saying things like "Updated the header" — say "Updated `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h`" instead.
