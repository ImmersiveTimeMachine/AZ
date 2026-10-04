# Winchester on the pistol pattern - plan (2026-09-28, for the user's decision)

User order: "adapt to the pistol logic: walking, crouching, shooting - all with the arms extended (the aim pose),
except the fast run. Only the M16 has a relaxed mode. Turns, crouch, crouch turns, aim offset - assess everything the
pack has and propose which clips we use, the same types as the pistol. Melee and grenades with the rifle later."

## 1. What the pistol does (CHT_v2 rows 308-401, measured today)

| State | Pistol | aim column |
|---|---|---|
| Idle standing | `Idle_Relaxed` / `Idle` | False / True - the ONLY place relaxed differs |
| Idle crouch | `CrouchLoop` | both |
| Walk / run loops | 8 directions, motion matching | **Any** (one leg set, torso from lock + AO when aiming) |
| Crouch walk | 8 directions | Any |
| Sprint | `SprintLoop` + start / 2 stops | Any |
| Starts | forward, turn-starts 90/180 L/R, strafe starts B/L/R, crouch starts | Any |
| Stops | forward RU/LU, strafe B/L/R, crouch stops | Any |
| Stance | `Idle2Crouch` / `Crouch2Idle` | Any |
| Jump | 3 phases: start (idle/walk/run) -> fall loop -> land / land-to-walk / land-to-run | Any |
| Turn in place | `TurnL/R_90Loop` (loops, rate-matched to the capsule) | both; crouch uses the M16 crouch-aim loops |
| Fire / reload | `ShootOnce`, `Reload_2` | per stance |

## 2. What the RifleMega pack has (446 clips, `/Game/AZ/Assets/Master/RifleMega`)

- Three standing styles with full locomotion: **Rifle01 = butt at the shoulder, arms extended** (the pistol-like
  pose), Rifle02 = at the hip (the "gun in front" the user rejected), Rifle03 = lowered shoulder, lower stance
  (pelvis 77 vs 84 cm). Crouch **Rifle_Cr** = shoulder.
- Per style: walk / run 8 directions (+ `_IPC` in-place twins), circle walk/run L/R, sprint (01/03 only), idles
  (Rifle01: Idle00 3.3 s + Idle01-04 = 2.0-5.7 s fidgets), turns `Turn90L/R`, `Turn180` (1.0 s, root yaw -90/-180),
  `Turn_Linear_90L/R` (loopable), `St_to_Cr` / `Cr_to_St`, jumps `Jump_Idle/Walk/Run` (CLOSED: take-off + landing in
  one clip, 2.0 / 2.1 / 1.4 s, 0 / 345 / 430 cm), dodges, grenade, pick-ups, stun, hit / death sets.
- Aim offsets: 17 poses per style (Rifle01 and Cr are already `AO_Winchester_Stand / Crouch`).
- Shooting: Winchester (lever cycle), Light / Hard, shotgun, double barrel, automatic. Reload: Winchester, shotgun,
  double barrel, automatic. Take / hide: TakeUp, HideDown, TakeDown, HideUp (01/02/03/Cr). Melee: Hard / Light 01-06 +
  6 hit reactions. Patrol set (NPC). Style transitions 01 <-> 02 <-> 03.
- **Not in the pack: starts, stops, turn-starts, a fall loop, separate landings.**
- The M16 AIM set (MetaHuman-native, shouldered arms) has exactly those: 28 standing + 10 crouch starts, 17 + 8 stops,
  5 jump starts, the aim fall loop, lands, stance transitions. The grip nodes adapt the hands to the Winchester.

## 3. Proposed mapping (Winchester = pistol pattern)

| State | Clip(s) | aim | Source |
|---|---|---|---|
| Idle standing | `Rifle01_St_Idle00` + idle breaks `Idle01-04` | both (breaks relaxed only) | pack |
| Idle crouch | `Rifle_Cr_Idle00` + breaks `Cr_Idle01/02` | both | pack |
| Walk / run 8 dir | `Rifle01_St_Walk_*_IPC`, `Rifle01_St_Run_*_IPC` (existing rows 419-426, 435-442) | **Any** | pack |
| Crouch walk 8 dir | `Rifle_Cr_Walk_*_IPC` (rows 451-458) | Any | pack |
| Sprint | `Rifle01_St_Sprint_IPC` (row 459, re-enable) - the only non-shouldered pose | Any | pack |
| Starts / turn-starts / stops (stand + crouch) | the M16 AIM transition rows, DUPLICATED for the Winchester tag with aim=Any (M16 rows untouched) | Any | M16 aim |
| Stance stand <-> crouch | `Rifle01_St_to_Cr` / `Rifle01_Cr_to_St` (rows 462/463) | Any | pack |
| Jump / fall / land | the M16 AIM jump starts + aim fall loop + lands, duplicated like the starts | Any | M16 aim |
| Turn in place | `Riflel01_St_Turn_Linear_90L/R`, `Riflel_Cr_Turn_Linear_90L/R` (rows 464-467) | Any | pack |
| Aim offset | `AO_Winchester_Stand / Crouch` (Rifle01 / Cr poses) | aiming | pack (done) |
| Fire | `Rifle01_St_Shoot_Hard` / `Rifle_Cr_Shoot_Hard` | per stance | pack (done) |
| Reload | `Rifle01_St_Reload_Winch` for both states (relaxed is Rifle02 today) / `Rifle_Cr_Reload_Winch` | per stance | pack |
| Draw / holster | `Rifle01_St_TakeUp` / `Rifle01_St_HideDown` (Rifle02 today) / Cr | per stance | pack |

Profile: speeds = the Rifle01 loops (walk 126, run 253, sprint ~465, crouch 139 cm/s); both "no ground transition"
flags OFF (starts / stops exist now); PSD_WIN_WalkAim / RunAim / Crouch / Sprint stay, the relaxed PSDs retire.

Revert: the Winchester tag comes OFF the 109 M16 relaxed rows (today's relaxed-on-M16 hookup); pack relaxed rows stay
disabled.

Not used: Rifle02 / Rifle03 locomotion and idles, circle walks (possible later MM variety), crouch run loops (no
crouch-run gait), discrete `Turn90/180` (our turn in place is loop-based, like the pistol's; see phase 2), patrol,
style transitions.

## 4. One code change this needs

With the pistol pattern the butt sits at the shoulder even when NOT aiming. AZ Weapon Body Clearance only allows the
shoulder pocket and freezes the wrist turn while `AimAlpha` is up, so today it would push the shouldered weapon ~4 cm
forward and turn it off the line whenever the player is not aiming. Fix: the node detects "shouldered" from the
animation itself (the butt already in the pocket in the incoming pose, eased) and treats that like aiming - weapon
agnostic, and sprint / reload (butt off the shoulder) fall out automatically. Checked offline on pose dumps first.

## 5. Phases

1. **Core pistol pattern** (this doc, sections 3-4): rows + profile + the shouldered detection; offline arm / body
   check on the Rifle01 clips; PIE: idle, walk, run, crouch, sprint, starts / stops, jumps, turns, aim, fire, reload.
2. **Discrete turns** 90 / 180 (pack TurnSet, stand + crouch): needs locomotion state-machine support (today turn in
   place is loops only - the pistol's discrete turns are also unused). Assess after phase 1.
3. **Actions with the rifle**: grenade throw (`Rifle01_St_Grenade`, `Rifle_Cr_Grenade`) into the throwable system;
   rifle melee (Hard / Light 01-02 + hit reactions) into the melee system; dodges; pick-ups; hit / death sets.

## 6. Decisions for the user

1. Idle when not aiming = `Rifle01_St_Idle00` (same as aiming) + 4 fidget breaks. (Alternative: Rifle03 low-ready
   idle - lower stance, pelvis 7 cm lower, dips when starting to walk.)
2. Starts / stops / jumps from the M16 aim set under the grip nodes. (Alternative: none - loops cross-fade, as in
   the pack-only aim today.)
3. Sprint = pack Rifle01 sprint. (Alternative: the M16 sprint.)
