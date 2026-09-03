---
name: project_psia_heavy_strike_plan
description: "★★★ PSIA strikes — END OF SESSION 2026-09-03: heavy punch/kick on R (weighted random, fallback = drawn variant), paired jabs L/R, all COMMITTED + PUSHED. Next: CLI build before reopen (LC patch), verify double-punch fix + kick pair origin, obstacle-aware unpaired strikes, heavy length decision. PSIA principle + geometry model inside. Task #17."
metadata:
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-03T02:00:00.796Z
---

# PSIA strikes (heavy on R, paired jabs, punch-or-kick variants) — running state 2026-09-03

Goal unchanged: a dedicated heavy (key R) that, when a Chalkie is in the search window, aligns BOTH bodies
through a PoseSearch Interaction pair, plays both halves in sync so the fist meets the chest on the authored
frame, else falls back to today's warped heavy. Context: [[reference_punch_reaction_content_inventory]]
§ REASSESSMENT; machinery = the catch driver ([[project_grab_grapple_design]] tail).

## STATE 2026-09-03 03:00 — heavy COMMITTED (991fb8b), jab pairs BUILT (uncommitted), kick variant CODED (needs closed build)
- **PIE 02:21 (4 heavy pairs):** search/align/sync/gate/knockback all held (cost ~96, entry 0.00, close-in
  32-40cm, hero moved 45 = his authored travel, Chalkie flew 242cm, `HitReact ignored` fired) but the hook
  WHIFFED 4/4: `[MeleeWin] OPEN swingT=0.45 gap=162` and no `[MeleeHit]`. Cause (timing, not alignment): the
  250ms zero-velocity ROOTING move I queued on the victim before the search was still live when the close-in
  (also OverrideVelocity) arrived and the EARLIER move won — victim@0.25s moved 0, then half the close-in.
  Two velocity overrides on ONE body = the first wins. Fix (Live-Coded, in 991fb8b, NOT re-verified): no
  rooting on the strike path, search + close-in in the press tick. Also: a StruckPair victim is strikeable
  again (HitReact retriggers on Event.Strike.Victim) so chained jabs re-pair.
- **Jab pairs (user: "implement all punches like left right"):** `BP_GA_Punch_L/R` REPARENTED to
  `UAZ_GA_StrikeInteraction` (Tools/strike_jab_content_build.py; BlueprintEditorLibrary.reparent_blueprint
  keeps CDO values + references), StrikeDatabase PSD_AZ_Punch_L/R (schema PSS_AZ_Strike, sampling [0,0.10]),
  StrikeMontage = the jab montage itself (its warp windows are inert on the pair path), MaxCloseInDistance
  25, entry ≤0.15. Victim montages `Strike/AM_Zombie_Strike_Jab_L/R_F_5` = [WalkIn 0.17/0.19 | React = KB
  [0,0.6] ≈37cm recoil], BeatEnd at contact+0.55. Measured: Punch_L knuckle 83.8 @0.17 (x0.3), Punch_R 78.2
  @0.19 (x7.7) → victim origin Y 110.8 / 105.9 → pair window ≈95-145cm actor gap; closer = the old warped
  jab (fallback). **The warped heavy is GONE from LMB** (BP_GA_Punch_L.PunchLunge_L = None); R only.
  Design cost to compare: a paired jab decides the hit at the press (no whiff at range) and the pair's
  HitReact cancels the Chalkie's own swing ~0.2s BEFORE the fist lands.
- **Kick variant (user: heavy punch OR kick at random on R):** `FAZ_StrikeVariant {Database, Montage,
  StrikeSockets, Weight}` + `StrikeVariants` array on the strike GA; `ShuffledVariants()` = weighted draw
  without replacement per press, `BeginStrike` tries them in order, first valid alignment wins (two PSIAs in
  ONE DB would be picked by cost = never random); `UAZ_GA_MeleeAttack::GetStrikeSockets()` is now virtual
  so the kick sweeps ball_r/foot_r. Lint clean, NOT BUILT. Kick measured: `RTG_RM_Fists_Kick_Front_Move_R`
  1.5s, 165cm walk-off, ball_r peaks 105.6cm @0.45 (z115, x-4), root adv 89.5 → montage [0,0.85], hit
  0.38-0.52, cancel 0.60-0.85, victim origin Y 232 → kick window ≈180-300cm, punch 125-245 (overlap =
  random). `Tools/strike_kick_content_build.py` builds it + sets BP_GA_HeavyStrike.StrikeVariants.
- **"UPoseSearchInteractionAsset::CalculateWarpTransforms unsupported non identity root bone" is BENIGN**
  (verified in PoseSearchInteractionAsset.cpp:375-399): a `#if ENABLE_ANIM_DEBUG && WITH_EDITOR` check that
  samples the root BONE pose and `Equals(Identity)` it at 1e-4 — it only logs. The alignment uses
  `Sampler.ExtractRootTransform` (root motion), which is what the runtime drives; the root bone is locked
  (RefPose) at runtime. Fires for our walk-in sections (root bone at Y≈83 mid-loop) AND for the heavy at
  Pos(0.00002,0.00004,0.0004). Ignore it; the earlier plan line "must not appear" was wrong.
- **Double punch (user 02:45):** every right click = 2 jabs. `[InputBuffer] latched` 0.14s after the click,
  replay at the cancel window. Cause: Held (Triggered) runs every frame the button is down; a click lasts
  several frames; refused frames 2..N latched as a new press. FIX (built 03:15): `DownInputTags` on the
  ASC — only the first Held frame of a press may latch (Pressed clears, Released removes).
- **Jab pair, hero drift:** `hero moved=20cm` by contact on an in-place jab (no RM drive → the player's own
  walk continued) → root-root 90 vs authored 116. FIX (same build): in-place hero clips get a zero-velocity
  OverrideVelocity for CloseSeconds (the victim's close-in is on the OTHER body — no overlap); only
  travelling clips get DriveRootMotion (driving an in-place clip is the ~zero-delta pin).
- **03:20 state (uncommitted since 991fb8b):** kick variant BUILT + LIVE (`AM_Fists_Kick_Strike` = clip [0.25,0.85],
  contact 0.20, blend-in 0.10, hit 0.13-0.27 on ball_r/foot_r, victim origin Y 164.7 after a −12 hero-travel-loss
  correction; PSD_AZ_Strike_Kick indexed). Punch pairs land (3/3 at swingT 0.50-0.51); kick pairs landed 3/6 before
  the −12 correction (root-root 143-153 vs modelled 137) — re-verify. **Fallback = the press's drawn variant**
  (`SelectMontage` override, sockets follow; both trimmed montages carry warp windows again for target-present-
  no-fit; Live-Coded, in the source). Weights are the USER's (BP editor: punch 0.6 / kick 0.4, verified by the
  11:9 draw); the kick script now preserves existing weights. Jab blend-in 0.10 (was 0.25: contact 0.17 under a
  0.25 blend = ~67cm reach, not 84). Still unverified: double-punch fix (no jab clicks in 4 sessions), corrected
  kick origin. User verdict so far: "heavy hits are shorter than original" = the 0.9s trim (no non-walking
  recovery exists in Heavy2Idle; options: trim to 1.2 + cap the drive at contact, or a FightingAnimsetPro heavy).
- Python traps hit tonight → [[feedback_python_save_only_if_dirty]] (map_key not dirty; import_text for
  tags/keys; Rotator(roll,pitch,yaw); EditDefaultsOnly struct fields → export_text/import_text;
  open_editor_for_assets plural; AnimMontage has no post_edit_change).

## NEXT STEPS (in order) — session end 2026-09-03 03:30
1. **CLI build BEFORE the editor reopens**: the fallback-by-variant change (`SelectMontage` override +
   sockets in the two fallback paths) is a Live Coding patch on top of the 03:15 DLL — it dies on restart.
2. PIE to close the two unverified items: single jab clicks (double-punch fix: no `[InputBuffer] latched`
   right after an activation) and kick pairs at 1.3-2.4m (`[MeleeHit] BP_GA_HeavyStrike` at swingT
   0.14-0.25, `victim@0.20s … root-root` ≈131-141 with origin 164.7).
3. Open design items the user raised: (a) obstacle-aware unpaired strikes — obstacle sweep in the
   no-hostile branch registering `MeleeTarget` at the hit point minus stand-off (the trimmed montages
   carry warp windows again), plus a clearance trace on the aligned victim transform on the pair path;
   (b) heavy length — 0.9s trim vs 1.2s + drive capped at contact vs a FightingAnimsetPro heavy;
   (c) variety = more PSIAs per database (walk/run/idle victim), the search selects.
4. Weights are the user's (punch 0.6 / kick 0.4 in the BP); Tools/strike_kick_content_build.py preserves them.

## What is WRITTEN (uncommitted, unbuilt) — `git status` shows it
- `AZ_GameplayTags`: `State.Combat.StruckPair`, `Event.Strike.Victim`.
- `AZ_MontageUtils::BuildSectionedMontageRanged(..., SegmentStartTimes, SegmentEndTimes, ...)` (old builder
  forwards). EVIDENCE the util is needed: Python CAN write FAnimSegment.anim_end_time but the montage length
  never recomputes (2.667 stays; no post_edit_change on AnimMontage; sequence_length read-only).
- `UAZ_GA_StrikeInteraction : UAZ_GA_MeleeAttack` (new .h/.cpp): precheck → parent's warped heavy;
  pair path: commit, root the victim (zero-vel OverrideVelocity 250ms), `TryBeginStrikeWhenStill` (≤4
  ticks, <15cm/s), `TryStrikeSearch` (MotionMatchMulti self=Attacker/target=Victim, validates PSIA attacker
  == StrikeMontage, victim montage has a "React" section = CONTACT, entry ≤ StrikeEntryMaxTime 0.20,
  CalculateFullAlignedTransform(TimeOffset = contact − entry) per actor with EACH actor's own mesh offset,
  close-in ≤ MaxCloseInDistance 60), `PlayPairedStrike` (warp targets removed, close-in layered move ending
  AT contact, our montage at entry + RM drive, `Event.Strike.Victim` payload {montage, EventMagnitude=entry,
  Instigator=hero}, probes), `EndAbility` cancels the victim's HitReact if the pair ends BEFORE contact.
  Fallback anywhere = `Super::ActivateAbility` (double commit harmless).
- `UAZ_GA_HitReact`: 4th trigger `Event.Strike.Victim` (ConfigureOnCDO); pair path = payload montage,
  entry position, RM drive DEFERRED to the React section start (timer), StruckPair loose tag 1/0, faces the
  Instigator via `AAZ_InfectedAIController::SetFacingOverrideWorld` (cleared in EndAbility),
  `StrikePairRecoverSeconds` 0.5; **`ShouldAbilityRespondToEvent` refuses every non-strike trigger while
  StruckPair is up** (runs BEFORE the retrigger's EndAbility — the contact hit's Event.Combat.HitReact would
  otherwise replace the authored knockback with a pose-picked flinch).
- `AZ_InputConfig.h`: rows EditAnywhere (EditDefaultsOnly made Python refuse "cannot be edited on instances").
- Content DONE (saved): `Strike/AS_Zombie_Walk_F_5_Loop_RM`, `Strike/AS_Zombie_Walk_F_5_KnockBack_RM`
  (dups with EnableRootMotion ON — the /Game/Zombie_01 originals untouched & unreferenced),
  `RT/AZ_IA_RT_HeavyStrike` (dup of IA_RT_Melee) mapped to **R** in AZ_IMC_RT_PawnInputs (R ALSO maps
  AZ_IA_RT_Reload — fists ignore it; decide later), `PSI/PSS_AZ_Strike` (dup of PSS_AZ_Catch with skeletons
  swapped: Attacker=SKEL_SurvivalMan, Victim=UE4_Mannequin_Skeleton; 2 cross-role root Position channels).

## MEASURED FACTS that changed the plan
- **All six `Zombie_Walk_F_1..6_KnockBack_Walk` START ON THE IMPACT**: head-lead drops 15-22cm over frames
  0-6, root flies BACKWARD from 0.1-0.37s (F_5: −267cm by 2.78s, then walks forward again from 3.5s; net
  −134). Memory's f33/f74/f13 "impacts" were stumble artifacts. The old detector had the axis sign inverted.
  → no pre-impact walk exists → victim montage = [walk loop cut | knockback]. Walk cut = best pose match of
  `Zombie_Walk_F_5_Loop` to KB frame 0 (frame 92, rms 16cm) → WalkIn = [2.567, 3.067] (17.6cm, 35cm/s).
- Rigs: BOTH clips face +Y in anim space (hand_r at −X), mesh yaw −90 on both pawns; Chalkie mesh offset
  (10, 0, −94), hero (0, 0, −92); capsules r=30 both. (memory's "Chalkie mesh yaw+90" note is WRONG.)
- Heavy `RTG_RM_Fists_Punch_Heavy2Idle` 2.667s, +Y travel 202: left knuckle peaks 85.4 @0.34 (root adv
  25.7), **RIGHT knuckle peaks 95.9 @0.50 (root adv 49.1, x=+12.7, z=148)** = THE strike (AT_MeleeSweep
  header agrees). Existing AM_Fists_Punch_Heavy_L: 2 hit windows (0.25-0.55, 1.70-2.64), warp 0→1.65.
- KB F_5 frame 0: spine_03 13cm in front of root (z 127). Contact geometry: knuckle 8cm short of the spine
  axis (≈4cm into the torso) → root-to-root at contact 117cm → victim Origin Y = 117 + 49.1 + 17.6 = 183.6.
  Sweep check: knuckle 24cm from the capsule axis at 0.50 (< r30+12 hit), 47cm at 0.48 (no hit) → the hit
  lands at ~0.49-0.50 deterministically IF alignment is exact; the 0.34 jab whiffs by 60cm.
- Search window: PSIA root-root at t=0 ≈ 184, at 0.15 ≈ 168 → live from ~125 to ~245cm (close-in ≤ 60).
- Python facts: `PoseSearchInteractionAssetFactory/SchemaFactory/DatabaseFactory` and `BlueprintFactory`
  exist; no InputActionFactory (duplicate an IA); `unreal.Key().import_text('R')` builds a key;
  `InputMappingContext.map_key` works; `default_key_mappings.mappings` is the 5.8 IMC storage.

## Decisions taken (answers to the plan's 4 open questions)
1. R = dedicated key (new IA), tag REUSED: `Input.Action.MeleeAttack` (existing, unused) — no new input tag.
2. Victim half = GA_HitReact authored-pair entry (no new GA).
3. Warped heavy stays on LMB lunge (BP_GA_Punch_L untouched) AND is R's fallback.
4. v1 = F_5 only. Contact = the 0.50 right hook (not 0.34). Damage 20 (2× jab) — tune.

## PIE — seam trace + PASS/FAIL
Trace: Chalkie walking in ~150cm → R → `[Strike] … victim rooted` → `search after N tick(s)` → `[Strike]
search actor=… role=Attacker/Victim anim=AZ_Strike_Heavy_F5 t≈0.05-0.15 cost<MAX` → `[Strike] align victim
(…) -> (…) d≤60 dyaw=… | hero d≈45 (his own travel to contact) | contact=0.50 close≈0.4s | root-root at
contact≈117` → `[Strike] pair LIVE` → `[Strike] victim …: pair montage … entry=… contact(React)=0.50 →
root-motion drive in 0.4s for 2.77s` → `[Strike] victim@0.2s moved≈half` → `[Strike] victim@0.4s … root-root
≈117 vel≈0` + `[Strike] contact@… fist->chest ≈ 8-15cm` + `[Strike] victim …: contact - root-motion drive on`
→ `[MeleeHit] … BP_GA_HeavyStrike` at swingT 0.49-0.51 → `[Strike] victim …: Event.Combat.HitReact ignored`
→ Chalkie flies back ~2.6m → BeatEnd 3.20 → `[HitReact] … reaction over — capsule moved ≈260cm`.
PASS = every line above, every press, no through-body. FAIL signatures: `results did not cover both
actors` → schema/role/skeleton or PoseHistory missing on one side; `aligned target too far` at 150cm →
Origin Y wrong (re-measure, don't tune); `pair REFUSED` → HitReact blocked (Grabbing/Dead) or trigger not
on the granted class CDO; victim slides during the walk-in / root-root ≠ 117 at contact → close-in vs RM
drive overlap (check the drive fires AT contact, not before); fist through chest → Origin/contact time;
second reaction after the hit → the ShouldAbilityRespondToEvent gate did not run (check the log line);
hero spins → a leftover warp target (should be removed on the pair path).

## Failure axes still open
- Hit-stop desync: attacker 0.05 vs victim 0.08 at contact → 30ms pair drift; accepted for v1.
- The victim's BT MoveTo during the walk-in: Staggered is up from entry, the close-in OverrideVelocity owns
  the capsule for 0.4s — if the Chalkie still walks, the BT is not yielding on Staggered during MoveTo.
- Abort semantics: pair ended before contact → victim's HitReact cancelled; after contact it keeps playing.
- Gitignored content: NONE now (the RM dups live in /Game/AZ/Blueprints/Animation/Strike).
