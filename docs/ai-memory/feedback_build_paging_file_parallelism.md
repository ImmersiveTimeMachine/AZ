---
name: feedback_build_paging_file_parallelism
description: "When a CLI / Live Coding build fails with \"C3859 Failed to create virtual memory for PCH / paging file too small / C1076 internal heap limit reached\", it's NOT a code error — cap compile parallelism in BuildConfiguration.xml."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Build failure "paging file too small" → cap parallelism, it's not your code

**Symptom (2026-05-31):** a full CLI build (or a Live Coding compile that triggers a big adaptive rebuild) fails with, on MULTIPLE modules including engine ones (`ZoneGraphAnnotations`, `MassSimulation`, `RuntimeTests`, plus `AZ_MoverAnimInstance.cpp`):
```
c1xx: error C3859: Failed to create virtual memory for PCH
c1xx: note: the system returned code 1455: The paging file is too small for this operation to complete.
c1xx: fatal error C1076: compiler limit: internal heap limit reached
Result: Failed (OtherCompilationError)
```
`UnrealBuildTool/Log.txt` shows e.g. `Executing up to 17 parallel actions` / `Distributing 32 actions to XGE`.

**Why:** this is a HOST MEMORY limit, not a compile error in the edited code. Each `cl.exe` PCH creation reserves multi-GB of committed virtual memory; ~17 in parallel exceeds the Windows commit limit (RAM + paging file) when the paging file is small. UBT's "1.5 GB per action" estimate under-counts real PCH commits.

**Fix (no reboot) — cap parallelism in `C:\Users\Artur\AppData\Roaming\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`:**
```xml
<?xml version="1.0" encoding="utf-8" ?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <MaxParallelActions>4</MaxParallelActions>
    <bAllowXGE>false</bAllowXGE>          <!-- force UBT local ParallelExecutor so the cap applies; XGE local execution ignored MaxProcessorCount -->
  </BuildConfiguration>
  <ParallelExecutor>
    <MaxProcessorCount>4</MaxProcessorCount>
  </ParallelExecutor>
</Configuration>
```
Re-run the same `LiveCoding.Compile` / CLI build — slower but completes (`Result: Succeeded`, `Using Unreal Build Accelerator local executor`). Editing the XML invalidates UBT's `XmlConfigCache.bin`, so it takes effect immediately. Raise the numbers (or remove the file) after enlarging the system paging file (System → Advanced → Performance → Virtual Memory).

**Proper long-term fix:** enlarge the Windows paging file (system-managed or a large fixed size). UE builds are memory-hungry; a small/fixed paging file throttles them.

**Related gotcha (same session):** Live Coding patches do NOT survive an editor restart — on reopen the editor loads the last FULL-build binary, so a session's worth of Live-Coding-only changes vanish. A reflected change (new `UPROPERTY`/`UFUNCTION`) needs a full CLI build with the editor CLOSED anyway (Live Coding holds a lock → CLI build fails with "Unable to build while Live Coding is active"); that full build re-bakes ALL pending source changes into the base `.dll`. See [[project_v2_locomotion_progress]], skill `cpp-build-livecoding`.
