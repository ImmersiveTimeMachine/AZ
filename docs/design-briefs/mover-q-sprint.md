# Mover hero Q sprint

Requested 2026-09-06: match the current CMC hero's Q sprint behavior on the active Mover MetaHuman hero.

## Implemented configuration

- Shared RT mapping remains Q -> AZ_IA_RT_Sprint -> Input.Action.Sprint. CMC uses hold/release, not a toggle.
- Active `AZ_BP_PawnMoverHero_MHC` now grants `BP_AZ_GA_Sprint`; its Walking mode SprintSpeed is 641.8 cm/s, matching the live CMC override and sprint clip. Other gait speeds are unchanged.
- Shared sprint ability activates without requiring a BP_CMC_Hero cast. Release ends the ability; OnEndAbility removes GE_Sprint using the ability's own ASC, including cancellation. GE_Sprint retains its existing five-second duration and Movement.Sprinting tag. Shift/Run remains independent.
- CHT_v2_CharacterAnimations rows 84-102 add Sprint: one sprint loop and copies of existing Run start/stop/turn/jump/landing fallbacks. Existing rows 0-83 were preserved.
- AnimPro_SprintFwdLoop1 links through BranchIn to PSD_v2_SprintLoco. The database has one entry, bDisableReselection=True, BranchInId=1384830554. Standing Sprint bypasses the forced Run/StrafeRun database overrides; crouch keeps priority.
- Sprint clip contact_l/contact_r curves use measured foot plants, with 8.333 ms linear ramps centered on boundaries. Left plants at 0-0.008333 and 0.516667-0.633333 s; right at 0.200000-0.333333 s. Existing curves, notifies and root-motion/loop flags were preserved.

## First user run and corrective change

User reported sprint speed with walk/run animation. Saved logs confirm speeds around 631-637 cm/s with old loops retained, and gait=2 reaches the animation function. Static live inspection confirmed Sprint row84 is enabled and uniquely matches standing Sprint locomotion. The ABP evaluates that chooser each update and passes its result directly.

The native MotionMatch call still supplied the outgoing walk/run loop as its continuing asset. For raw sequence candidates, UE's PopulateContinuingPoseSearches discovers that outgoing clip's own BranchIn database, independently of the new candidates. With default DoNotInterrupt it can score and retain the excluded old loop. This differs from the existing whole-database Walk/Run search path.

Corrective change: populate Continuing only when the outgoing asset is in the current search pool, either directly or through a candidate database's Contains check. Reuse that predicate for empty-result fallback. Same-pool continuity remains enabled. New `[v2 MMPool]` lines report gait/stance changes with chosen/pool/outgoing/continuing/selected/cost.

## Verification and next step

User's full build succeeded. The corrective guard passed Rider error analysis, source whitespace checks and a read-only review. UBT reported Result: Succeeded; Live Coding records the AZ patch successful at 2026-09-07 00:56:34 UTC.

The user's 01:01:40-01:02:22 UTC run confirmed the guard: Q selects AnimPro_SprintFwdLoop1, and release selects Run/Walk. Across 43 picks there were no logged Snap/Replay events. Sprint-to-run quality remained poor: seven of eight direct transitions chose Run at 0.40 seconds regardless of outgoing foot; ordinary sprint entry was nearly always 0.30 seconds. Both are right-foot plants. Two additional gait chains (Sprint->Run->Walk->Run in 178/288 ms) interrupted 0.20-second crossfades, a separate input-state effect. No Slomo was logged; sampled playback rate was 1.0.

## Foot matching and exploration-only restriction

The user explicitly restricted sprint to exploration. BP_AZ_GA_Sprint now blocks Movement.Strafe (verified on a fresh instance after compilation). QuickBar's existing fight-mode entry publishes Movement.Strafe and cancels abilities tagged Movement.Sprinting. Mover ProduceInput independently requires !bStrafe for Sprint and otherwise falls through to held Run/Walk. Combat.Ready is a separate upper-body overlay and is not the fight-mode discriminator.

CMC's gait DBs use PSN_AZ_CMC; the Mover gait DBs previously had no NormalizationSet. UE computes channel mean Euclidean deviations separately per database unless a set is assigned. Authored-sample analysis found sprint root-velocity deviation around 0.000936 (engine replaces values <=0.1 with 1), while foot-position deviation was about 80.39. Thus speed residuals can outweigh phase differences during gait changes. This supports a normalization correction; exact runtime per-channel costs were not captured.

Added PSN_v2_ExplorationGaits containing PSD_v2_WalkLoco, PSD_v2_RunLoco, PSD_v2_SprintLoco, and assigned it to those three DBs. Runtime candidate pools remain separate. Schema weights, clip times, and animation-selection behavior are unchanged. Sprint's single entry, BranchInId and disable-reselection flag were verified after saving.

Added outT/inT and outFeet/inFeet (left,right; -1 indicates missing curves) to gait-change [v2 MMPool] diagnostics. These are read from unblended outgoing/incoming sequences, not inferred from the blended foot latch.

Rider error analysis and source whitespace checks passed. UBT reported Result: Succeeded; AZ Live Coding patch loaded at 2026-09-07 01:16:22 UTC. All three DBs reported new BuildIndex Succeeded keys at 01:16:40 UTC. No PIE was run by the agent for these changes.

User validation: after the shared-normalization and exploration-only changes, the user reported "ok works" in response to the requested sprint-to-run and fight-mode checks. The reported behavior is accepted as working. No additional agent-run PIE or automated tests were performed; detailed post-change per-channel costs were not captured.

User drives editor tests: ask before starting PIE or injecting input, otherwise wait for their run and read logs. No automated tests were added. No temporary native setup code remains in source.
