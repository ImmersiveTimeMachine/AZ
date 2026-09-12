# Ctrl toggles crouch

2026-09-12. User requested one Ctrl press to crouch and the next to stand.

GA_Crouch now waits for the next replicated InputPressed event with bTestAlreadyPressed=false, instead of ending on release. PlayerController activates crouch only on Started, snapshots the active input spec before dispatch, and does not reactivate it on the second press. Held/Triggered events are excluded. OnUnPossess clears crouch input and cancels its active ability so PlayerState-owned stance intent cannot carry into another pawn.

Existing Mover stance/ceiling clearance, camera, animation, sprint interaction, and death/grab cancellation are retained. No direct capsule or animation changes. The old reflected OnCrouchInputReleased(float) callback name is retained for loaded Blueprint compatibility, but the native next-press task now calls it. No header/reflection or asset edits were needed.

Live asset checks: BP_AZ_GA_Crouch inherits the native class, owns Movement.Crouching, is LocalPredicted/InstancedPerActor, and disables retrigger. Its old BP release chain is disconnected. Current input config maps Input.Action.Crouch to AZ_IA_RT_Crouch; AZ_IMC_RT_PawnInputs maps LeftControl. The RT action has Pressed and Released triggers; release Triggered events are intentionally ignored by the new crouch route.

UBT succeeded in 8.96s. AZ Live Coding patch applied at 18:31:21.789 UTC, and the editor reported success with no object changes. Source whitespace and independent input/lifecycle reviews passed. No PIE or tests were started. User should check Ctrl down/up remains crouched and a second down stands, including holding the key. A normal build before a future editor restart retains the patch in the main DLL.
