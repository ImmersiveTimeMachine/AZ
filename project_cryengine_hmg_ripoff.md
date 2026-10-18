---
name: project_cryengine_hmg_ripoff
description: "CryEngine AZ HMG rip-off fixes (2026-10-17) — fire, broken GameSDK 1P rip clip, first-person camera modes; Artur's accepted defaults"
metadata:
  node_type: memory
  type: project
  originSessionId: 4d147044-5ec5-4d0e-bd62-511fbd640c9f
  modified: 2026-10-18T00:54:51.769Z
---

CryEngine AZ (C:/CryEngine/AZ), ripped-off HMG, fixed with Artur on 2026-10-17. The code lives in Codex-owned files (WeaponRuntime, CharacterAnimation, Player).

- **Fire after rip-off.** AZ armed `spin_up_time` 0.5 s on every press for Rapid modes. GameSDK `CRapid::StartFire` never does; only `CSingle::StartFire` and `EndReload` use it. With the arming removed for Rapid, the ripped HMG fires. Codex had only tested detached fire through scripted `use_hmg` holds, never real clicks after a rip.
- **Broken GameSDK clip.** `stand_tac_ripoff_hmg_1p_01` (4.4 s): at normalized 0.58–0.60 the Char1P upper body jumps 0.3–0.5 m back, and the pose stays broken (head behind the spine) to the end. The data proved it is not root motion (accumulated root motion = 0) and not the Char3P shadow (smooth). Fix: console `az_hmg_rip_cut` 0.52 ends the rip early via `StopExternalAction` (0.2 s blend-out finishes before 0.58).
- **Camera.** `Bip01 Camera` is a child of `Bip01 Head`. Torso IK pins it to the eye, except under Torso1P clips (layers 10–12). Console `az_fp_camera_align`:
  - 1 (default, Artur's pick) — body aligned so the bone sits on the fixed eye;
  - 0 — old behaviour;
  - 2 — GameSDK pos+rot follow; Artur rejected it as strange;
  - 3 — position-only follow.
- **Diagnostics.**
  - `az_fp_trace 1` — per-0.1 s bone/head/shadow/clip trace during a rip.
  - Log lines `AZ_WEAPON_FIRE_BLOCKED`, `AZ_WEAPON_TRIGGER_REJECTED`, `AZ_FP_ALIGN begin/end`.

**Why:** Artur judged each camera variant in-game; mode 2 looked like the view swinging, and mode 0 put the camera into the shoulder.

**How to apply:** for any other GameSDK 1P clip that "goes into the body", first trace the bones (`az_fp_trace`), then check the clip itself before changing the camera. Related: [[project_cryengine_noesis_ui]], [[feedback_stop_the_patch_loop]].
