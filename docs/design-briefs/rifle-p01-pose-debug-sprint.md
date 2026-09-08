# P01 pose diagnostics and sprint carry

User requested finer grip measurements, run start/stop and jump/land checks, and back carry during sprint (2026-09-07).

## Presentation

`UAZ_Inv_CommonUI_EquipmentComponent::ReconcilePresentation` remains the attachment owner. A selected rifle now uses `CarrySocketName` while the authority ASC has `Movement.Sprinting`; clearing that tag restores `AimSocketName` or `RelaxedSocketName` according to the current aim tag. No selection, granted ability, rifle profile, body mesh transform, or IK change. Existing selected/presented identity, pawn owner and primary marker guards distinguish sprint carry from ordinary holstering. Both aim and sprint tag callbacks defer reconciliation during selection commits. Attachments replicate through the existing weapon actor.

## Pose diagnostics

New module-owned `FAZ_WeaponPoseDebug` observer registers/removes its end-frame delegate in the AZ module lifecycle. CVars use the existing explicit registration and name-based unregistration pattern. No static console objects, actor tick, gameplay pose writes, or reflected members were added.

Console commands after the required editor restart/build:

- `az.Weapon.Debug 1`: mode, selection/item identity, actual versus expected socket, movement state, aim alpha/angles, selected clip and actual Blend Stack players/times/rates. Blend-in values are each player's own blend-in weight, not normalized final pose contributions.
- `az.Weapon.Debug 2`: also logs body/weapon/socket transforms and right wrist/finger plus left wrist transforms in weapon space. Reports the existing left-grip-marker to wrist distance in centimeters.
- `az.Weapon.Debug 3`: also draws right wrist red, left wrist green, left grip cyan, the wrist-marker gap yellow, and weapon axes.
- `az.Weapon.DebugInterval 0.25`: sample interval in seconds; minimum 0.05. Mode/socket changes are logged immediately.
- `az.Weapon.Debug 0`: off (default).

Look for `[WeaponPose]`, `[WeaponPose.Transform]` and `[WeaponPose.Grip]` in `C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log`. Only game worlds are sampled; editor asset preview instances are ignored. All transform records following a summary belong to that weapon/frame. The weapon's `hand_r` reference bone has not been established as a physical pistol-grip anchor; the diagnostics deliberately do not call its separation from the body wrist a grip error. The previous socket fit preserved that right-hand reference anchor, so further socket changes require measured geometry rather than rerunning the previous left-hand fit.

Both relaxed and aim sockets retain their separate parent bones and existing fit. No IK has been enabled. The newest user screenshot shows a much improved support-hand fit; the residual right-hand/trigger alignment has not yet been measured during gameplay with these diagnostics.

## Validation / remaining work

Source and module changes prepared; no PIE or automated tests run by the agent. All four changed translation units compile successfully with the current UBT-generated MSVC response settings; isolated object/output files are in `Saved/RiflePoseCompileCheck`, and no DLL was linked or patched. This is a compile check, not a loaded build. CLI build remains blocked by the running editor's Live Coding session. Full editor-closed build is needed for module initialization changes; do not Live Code a changed module object layout. User owns close/reopen.

Fresh gameplay logs establish ordinary P01 jogging stops already select `rm_W2_Jog_F_to_Stand_Relaxed_RU/LU`. The remaining unarmed run transitions were shared moving180 pivots (original rows24..27). `Tools/rifle_p01_pivot_setup.py` was applied and verified at21:22:39UTC: master276 to280rows; four existing rifle turn-start fallbacks (aim/relaxed, left/right), Foot=Any, StartTime0, Run-only and !Movement.Sprinting. Other existing non-tag cells/results were preserved. These have only1.7-4.1cm travel in their first150ms versus44-54cm for the original moving pivots, so this fallback introduces slower stop-and-turn anticipation. Receipt: `Saved/RifleAnimationContent/p01-pivot-prepare.json`.

Jump content is prepared in `Tools/rifle_p01_jump_setup.py`: ten existing compatible source families (idle/forward walk/forward jog, foot and aim/relaxed variants) produce twenty owned takeoff/landing sequences. This matches the existing jump selector's coverage; other directional rifle jump sources exist and were not integrated in this pass. Original source files/retargeting and the native state machine are unchanged. The takeoffs trim measured anticipation and hold the last precontact airborne pose through a4.000-4.032second tail; landings use the matching recovery portion. Exact original fractional model frame rates were retained, poses verified against source, and native root tracks preserve unscaled rise13.31-77.22cm (above the existing10cm apex detector). All sampled local pose position errors were0cm. No contact curves are introduced during air/land; the existing last-grounded-foot latch remains authoritative.

The new `PSD_P01_Land` has ten BranchIn-owned entries with a0-0.1333second landing entry window. Native index succeeded at21:27:13.650UTC (hash`f8828c3e08812882caa0b9848fd3bedddb9811d6`) and was stored in DDC. Idle landing keeps the existing direct-play policy; moving landings use single-clip MM. Content receipt: `Saved/RifleAnimationContent/p01-jump-prepare.json`. The dynamic jump chooser pass was applied and saved:280 to302rows. It also refines36rifle-only normal ground start/stop rows from JustLanded=Any toFalse, preventing them from competing at touchdown. Both jump and pivot idempotent audits pass on the final302row master; the pivot manifest's one affected prefix hash was updated only after strict preflight. Final readback: `Saved/RifleAnimationContent/p01-pose-sprint-final-readback.json`. Master backup/application receipt: `Saved/RifleAnimationContent/p01-jump-chooser-prepare.json`.

Live Sprint ability inspection confirms a reachable presentation edge: starting/releasing sprint, or the five-second GE_Sprint expiry, can change the socket during a committed jump/landing. The locomotion driver deliberately retains the selected jump clip until the phase finishes; bypassing that lock would restart jump/root-motion setup. Attachment follows the requested current sprint tag immediately; the chosen jump's arm pose can temporarily remain from the previous mode until landing/phase completion. A later cosmetic upper-body solution may be needed if the user finds that edge distracting. Do not restart the jump to hide it.

## Next session/build steps

After asset activation is complete, user closes the editor. Verify process absence, then build AZEditor normally and require UBT `Result: Succeeded` before asking the user to reopen. After reopen, enable `az.Weapon.Debug 2` for the user's next manual run; do not start PIE. Read the new run's `[Equipment] presentation socket` and `[WeaponPose]` records, including run/pivot/air/land picks and sprint carry transitions. Preserve the current fitted aim/relaxed sockets until runtime measurements identify a defensible correction.
