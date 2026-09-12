---
name: feedback_parallel_build_header_edit_corruption
description: "★★ Editor crashes at STARTUP (heap corruption in the first CDO constructor, e.g. AAZ_Item -> FBodyInstance/TArray::Empty realloc) after a build that reported Succeeded = a header was edited by another agent WHILE the module was compiling; objects from both layouts got linked. Fix: wipe Intermediate/.../Development/AZ/*.obj and rebuild with a source-mtime snapshot before/after to prove nothing moved."
metadata:
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
---

# A build can succeed and still produce a binary that crashes at startup (2026-09-11)

**Symptom:** editor dies during `ProcessNewlyLoadedUObjects` -> `UClass::CreateDefaultObject` of the first
game class (`AAZ_Item` ctor -> `CreateDefaultSubobject` -> `SetCollisionProfileName` -> `TArray::Empty` ->
`FMallocMimalloc::Realloc`). UBT log: `Result: Succeeded`. The AZ.log ends right at
`InternalLoadLibrary: 'AZ'`.

**Cause (verified by timestamps):** four headers changed at 18:05-18:07 (another agent: EquipmentComponent,
AbilitySystemComponent, GA_FirearmFire, PlayerController) while the build was compiling; the DLL linked at
18:09:50. TUs compiled before and after the edits carry different class layouts; the first CDO whose size
disagrees corrupts the heap. Nothing in the constructor is wrong.

**How to apply:**
1. Startup crash in a CDO constructor after a "successful" build = suspect a mid-build edit FIRST. Check
   `find Source/AZ -name '*.h' -newermt '<build start>'` against the DLL mtime.
2. Fix = consistent rebuild: `rm Intermediate/Build/Win64/x64/UnrealEditor/Development/AZ/*.obj` (module only,
   ~190 files, ~50 s), snapshot `stat -c '%Y %n'` of Source/AZ before, run
   `Build.bat AZEditor Win64 Development -Project=... -WaitMutex -FromMsBuild`, diff the snapshot after. If it
   differs, rebuild again. Only launch the editor when the diff is empty.
3. With parallel agents, never launch the editor on a binary whose build window overlaps someone else's
   header edit.

Related: [[feedback_parallel_editor_edits]], [[feedback_build_paging_file_parallelism]].
