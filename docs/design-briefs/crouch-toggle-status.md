# Ctrl toggles crouch

2026-09-12. User requested one Ctrl press to crouch and the next to stand.

GA_Crouch now waits for the next replicated InputPressed event with bTestAlreadyPressed=false, instead of ending on release. PlayerController activates crouch only on Started, snapshots the active input spec before dispatch, and does not reactivate it on the second press. Held/Triggered events are excluded. OnUnPossess clears crouch input and cancels its active ability so PlayerState-owned stance intent cannot carry into another pawn.

Existing Mover stance/ceiling clearance, camera, animation, sprint interaction, and death/grab cancellation are retained. No direct capsule or animation changes. The old reflected OnCrouchInputReleased(float) callback name is retained for loaded Blueprint compatibility, but the native next-press task now calls it. No header/reflection or asset edits were needed.

Live asset checks: BP_AZ_GA_Crouch inherits the native class, owns Movement.Crouching, is LocalPredicted/InstancedPerActor, and disables retrigger. Its old BP release chain is disconnected. Current input config maps Input.Action.Crouch to AZ_IA_RT_Crouch; AZ_IMC_RT_PawnInputs maps LeftControl. The RT action has Pressed and Released triggers; release Triggered events are intentionally ignored by the new crouch route.

UBT succeeded in 8.96s. AZ Live Coding patch applied at 18:31:21.789 UTC, and the editor reported success with no object changes. Source whitespace and independent input/lifecycle reviews passed. No PIE or tests were started. User should check Ctrl down/up remains crouched and a second down stands, including holding the key. A normal build before a future editor restart retains the patch in the main DLL.

## Camera glide follow-up

User confirmed the toggle works, but requested a slower crouch-camera transition. Live inspection found a 90-to-55cm capsule half-height change and a camera boom attached directly at local Z70. Mover recentres the capsule immediately, while all current camera modes intentionally disable general positional lag. The mode framing interpolation did not cover that 35cm root step. No earlier dedicated crouch-camera timeline exists on the active v2 Blueprint; legacy camera-component code is not active here.

UpdateCameraForMode now offsets the pivot immediately against the capsule-height difference and eases an equal opposite world-space TargetOffset using the mode's existing InterpSpeed (currently 8, roughly a 0.4-second visible glide). The two corrections cancel at rest, preserving standing/crouched endpoints. It reads actual capsule height, so blocked uncrouch stays crouched, and mid-transition toggles reverse continuously. No general movement/aim lag, physical stance behavior, or animation playback was changed. The existing spring-arm offset holds interpolation state; no new reflected fields, class-layout changes, or asset edits were needed.

The current rig uses its class-default boom offsets as baselines; runtime independent writers of boom-relative location/TargetOffset would need to compose with this owner. First taking control of an already-crouched pawn may glide from standing view height. Source review confirmed no conflicting writers in the current rig. No PIE/tests were run.

Camera patch: UBT succeeded in 8.30s; AZ Live Coding applied at 19:07:15.443 UTC on 2026-09-12, editor success with no object changes at 19:07:16.553. Source whitespace check passed. User visual check is pending.

Final review corrected standing-height scaling to use component Z scale, matching this engine's GetScaledCapsuleHalfHeight (GetShapeScale instead uses minimum axis scale). Final UBT succeeded in 8.21s and AZ patch applied at 19:08:43.888 UTC. Current unit-scale hero behavior is identical, with correct matching math for nonuniform scale.
