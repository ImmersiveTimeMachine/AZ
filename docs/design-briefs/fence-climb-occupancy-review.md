# Fence climb refusal with infected nearby

Read-only investigation, September 15, 2026. User asked whether an infected on the opposite side prevents climbing. No source, assets, collision settings or gameplay state changed; no PIE session was started/stopped and no automated tests were run.

## Findings

Traversal has no general enemy-proximity restriction. It currently uses the configured Visibility channel for surface detection, floor/support traces, terminal/landing capsule overlap and the descent sweep. Only the owner is ignored by the shared query params. A living or dead infected component that blocks Visibility can therefore affect these checks if it occupies their actual path/volume. A committed grab, stagger or death separately prevents the ability through GAS.

The latest recorded refusals on `SM_fence_16` are `unsafe-climb-drop ... reason=landing-overlap`, including 03:59:04.551, 03:59:33.303 and 03:59:53.703 UTC. These logs do not identify the overlapping actor. Read-only resampling of those landing capsule positions in the live game world found **the fence itself**, with blocking responses to both Visibility and Pawn, at all three positions. No infected was needed to reproduce that geometry overlap. The recorded standing-success position was clear.

Live world: `/Game/Post_ap_city/Maps/UEDPIE_0_Showcase.Showcase`. Hero capsule radius30 cm, half-height90 cm. The sampled ground capsule centers used the logged floor166 cm plus half-height and2 cm clearance.

| Recorded case | Sample capsule center | Current blocking component |
|---|---|---|
| Walk refusal | (-8297,-5162,258) | SM_fence_16.StaticMeshComponent0 |
| Walk refusal | (-8182,-5162,258) | SM_fence_16.StaticMeshComponent0 |
| Run refusal | (-8275,-5163,258) | SM_fence_16.StaticMeshComponent0 |
| Standing success | (-8259,-5147,258) | None |

`SM_fence_16` is actually `/Game/Post_ap_city/Meshes/Post-apocalypse_vol2-square/Near_object/SM_fence_4.SM_fence_4`, unit scale. Its component override references `DA_TraversalSurface_Fence3_ClimbAndDrop`, which describes the approximately5.85 cm upper envelope measured on SM_fence_3. Fence4's overall local bounds are X[-165.226807,165.226868], Y[-15.407715,23.281006], Z[0,236.312607]. It protrudes farther on one side. Those render bounds alone are not collision measurements; the overlap queries independently confirm the obstruction.

The shorter walk/run climb exits, approximately49–50 cm past the front ledge, can hit this geometry; the longer standing exit, approximately64 cm, clears it. Selection currently picks one clip and only afterward checks its detailed climb/drop exit. If that check fails, it returns NoCandidate without trying a different clip whose exit might clear the fence.

Separate nearby refusals are configuration issues: `SM_fence_20` (also SM_fence_4) and `SM_fence_21` (SM_fence_5) have no component or mesh traversal data. Their logs explicitly report `no-provider`.

## Recommended next implementation

1. Measure the actual SM_fence_4/5 traversal edges and collision clearance, then author profiles appropriate to those meshes. Do not copy Fence3's envelope blindly or replace the user's actors.
2. Evaluate detailed exit clearance as part of clip selection, so a rejected short exit can fall back to another compatible clip with a clear destination. Preserve1x playback and physical landing checks.
3. Separate surface/floor measurement from actor occupancy. Infected must not become ledges or floor measurements. A nearby enemy outside the exit volume should not veto traversal; an enemy truly occupying the capsule exit/landing needs an explicit occupancy policy, such as another clear candidate, rather than disabling collision.
4. Add blocker actor/component names to refusal diagnostics so scenery, infected and unsupported-provider failures are distinguishable in one log line.

Current source: `C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp`, especially shared query params near680, far landing near571–588, and detailed climb/drop exit checks near964–1002. Evidence: `C:/UnrealEngine/Games/AZ/Saved/TraversalFenceOccupancy/live-overlap-snapshot.json`, `surface-snapshot.json`, and `C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log`.
