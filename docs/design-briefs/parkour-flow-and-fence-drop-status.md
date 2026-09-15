**Parkour flow corrections and fence climb/drop — built and authored**

September 15, 2026 UTC. User supplied `C:/Users/Artur/Videos/NVIDIA/Unreal Engine 5/Unreal Engine 5 2026.09.14 - 22.16.23.03.mp4`, then explicitly requested a profile allowing SM_fence_3 to climb and fall as a provisional behavior.

**What the recording showed**

The 11.71-second recording was inspected through 47 sampled frames and matching gameplay logs. It visibly contains a run hurdle, a stop at the following obstacle, a standing mantle, a fall/bump, a normal jump, another hurdle and another stop. Frame sheets are under `C:/UnrealEngine/Games/AZ/Saved/VideoReview/Parkour_20260914_221623/`. Filename time is not exact video/log synchronization: the first-frame HUD's preceding idle age aligns approximately with 02:16:23.43 UTC. Video-relative times should therefore be treated as approximate; log event times below are exact.

Three separate causes were identified:

- A press during Traversing could return NoCandidate when the geometry probe missed. The ordinary jump ability then installed its landing wait despite never performing a jump. In the recorded session, a press at 02:16:33.156 during hurdle was followed by traversal completion at 34.731 and a jump watchdog at 35.670. The same pattern repeated afterward. The runtime watchdog was 2.5 s. This was an unintended input lock, not animation blend time.
- Moving hurdle copies contained the source's early TraversalBlendOut timing, but no native AZ handoff there. Their actual release was approximately 0.23 s before the full montage end. The authored tail could carry the character into the next obstacle and stop it while traversal still owned movement.
- Traversing held the underlying locomotion state at IdleLoop. A moving exit then selected an unnecessary ordinary from-rest start, adding around 0.62 s before the loop. Settled standing exits do legitimately need different treatment.

**Code changes**

1. `TryStartMantle` now returns BodyBusy before geometry if the ASC owns State.Traversing or the raw Mover mode is Traversing. The tag covers queued entry; the raw mode covers the handoff interval. An extra press cannot fall through into ordinary Jump's landing watchdog. Existing fresh-press-only activation and release behavior remain intact; this does not add an input buffer.
2. The animation update records a Traversing-to-Walking edge before replacing its mode history. The SM resumes LocomotionLoop directly when movement remains permitted/held and real planar speed exceeds the existing 10 cm/s idle threshold. Normal MM/PoseHistory selects the incoming pose; no hardcoded phase or velocity change was added. Settled, blocked and no-input exits retain their existing behavior. Traversing-to-Falling stays in the air path.
3. `FAZ_TraversalRequest.SuccessMovementMode` defaults to Walking and accepts Walking/Falling. The executor captures it before mutation and uses it on successful completion/handoff. Abort/watchdog recovery keeps the existing physical support recovery. Normal mantle/hurdle/climb requests remain Walking by default.
4. Surface profiles have a default-off `bAllowClimbAndDrop`. A permitted Climb/drop requires an authored paired far edge, the existing climb height range, a valid far-side floor, adequate measured exit distance, a clear terminal capsule, clear ground capsule and a swept descent. Only this explicit mode waives the climb clip's terminal platform-support requirement. The candidate stores airborne terminal pose separately from the real floor, and GA_Climb requests Falling on success.

Climb exit distance is derived from the existing bake contract: RequiredTopSupport = final forward root displacement + 10 cm. The code names/document this margin and checks the exit against depth + capsule radius + surface margin. All six current climb root tracks were sampled at montage end minus 0.30, 0.23, 0.10 seconds and at end; the terminal forward position was stable within 0.05 cm at the natural blend-out region. This is asset evidence, not a gameplay trajectory verification.

**Moving hurdle handoffs**

Added exactly one Event.Traversal.Handoff notify to each of the 16 gameplay-owned Walk/Run hurdle montages. Standing hurdle and all mantle/climb montages were not changed in this pass.

The marker uses the existing source blend-out start, constrained to occur after all warp windows and at least one 60 Hz frame after their last end. Every sampled root was at source ground height at the marker; source forward speed remained approximately 181–299 cm/s for Walk and 423–535 cm/s for Run.

Example from the recording: AM_AZ_Hurdle_1_0_Run_Relaxed_Lfoot now has its AZ event at approximately 1.383 s, following BackFloor's end at approximately 1.338 s. Entered at 0.66 s, the intended handoff is approximately 0.723 s after activation rather than the observed 1.811 s late release. Exact gameplay motion and blending still require the user's next pass.

The source BP notify is preserved. Its serialized notify name is TraversalBlendOut, while the runtime class can resolve to BP_NotifyState_MontageBlendOut_C; the authoring script handles both names. Playback remains 1x and source animation sequences are untouched.

**Fence profile now bound to SM_fence_3**

`/Game/AZ/Blueprints/SmartActors/DA_TraversalSurface_Fence3_ClimbAndDrop`

The mesh's runtime Asset User Data now references this new profile. The original DA_TraversalSurface_Fence3 remains available as the conservative no-standing profile. No actor replacement is required.

- Allow Climb / Allow Climb And Drop / Supports Crossing: true.
- Supports Standing and Allow Mantle: false. Existing hurdle permission remains, subject to the unchanged height/geometry gates.
- Tags: Surface.Type.Fence, Surface.Traversal.NoStandingTop, Surface.Hazard.PointedTop, Surface.Traversal.ClimbAndDrop.
- The measured overall render bounds were approximately 31 cm deep because they include the concrete plinth. Vertices above 30 cm show the actual metal fence is approximately 5.85 cm deep. The new custom envelope describes this upper portion, with the real 236.31 cm top; the mesh/collision itself is unchanged.
- Baked climb exits are approximately 49–64 cm beyond the front anchor. Against the 5.85 cm upper depth, 30 cm capsule radius and 5 cm margin, the required clearance is approximately 40.85 cm. Real collision checks still include the wider base and nearby geometry during the drop.

This is the user-approved prototype: climb, clear the far face, then use Falling and existing landing behavior. It is not a newly authored polished fence-straddle/descent animation, and pointed-top contact polish remains a visual review item.

**One system, two providers**

There is one shared action-selection/GAS/Mover traversal system. GASP blocks provide legacy splines; opted-in meshes/components provide surface profiles. Both feed the same executor. The new exit policy is action data, not a second character controller.

**Validation**

- Normal AZEditor build: 23 actions, Result: Succeeded, 13.56 s. AZ DLL timestamp 2026-09-15 02:44:11 UTC; restarted editor PID37864 loaded the reflected profile/candidate/request fields.
- Independent static reviews passed the busy gate, explicit falling handoff, moving exit, profile opt-in and clearance checks. Source whitespace checks passed.
- Authoring backed up all affected mesh/profile/montage packages, then saved and read back the 16 unique handoff events and the new profile binding. Native DescribeSurface resolves the live placed fence with ClimbAndDrop enabled and four world-space edges.
- Before/after scene capture matched the user's current actor identity/transform, geometry bounds, materials and collision. No level changes were made by this authoring pass. The user's current placement is StaticMeshActor_14; do not restore the older Actor_13 snapshot from the preceding task.
- Final dirty map/content package lists were empty. No automated tests were added/run, and no PIE/gameplay tests were started by the agent.

Next user check: repeat the same route with held movement and fresh Space presses, then climb the fence from each side. Check for a native handoff before the next obstacle, absence of phantom jump watchdog waits, and actual clear fall/landing on the fence. No buffering/automatic traversal chaining was added.

**Files and receipts**

- [C:/UnrealEngine/Games/AZ/Tools/parkour_flow_and_fence_drop_setup.py](C:/UnrealEngine/Games/AZ/Tools/parkour_flow_and_fence_drop_setup.py) — main('audit'|'author'|'verify'); latest authoring entry point.
- [C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/verified.json](C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/verified.json)
- [C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/before.json](C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/before.json)
- [C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/build.log](C:/UnrealEngine/Games/AZ/Saved/ParkourFlow/build.log)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_TraversalComponent.cpp)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Traversal.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Traversal.cpp)
- [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp)

The original smart_actor_surfaces_setup.py author/verify operations describe the previous conservative profile. Do not rerun its author operation over the new binding unless intentionally restoring that earlier setup. Its capture() helper remains reused for preservation checks.
