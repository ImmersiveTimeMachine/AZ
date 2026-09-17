# Execution brief for Claude — CHALK throwable system

Implement CHALK's shared throwable action for **stone, grenade and knife** using the plan below. One action/state machine, data-driven item and weapon-context variants. The user approved **Quiet Sage / mockup03**. This brief is an implementation assignment for the receiving agent; the planning session that produced it did not implement gameplay.

**Final control override confirmed by the user, September16: HOLD RMB to aim, RELEASE RMB to throw, CLICK LMB to cancel.** This replaces every earlier two-click or reversed-button proposal. Mouse release requests the animation; inventory is spent only at the validated physical release cue.

**Implementation has progressed.** Start with the [current completion work order](C:/UnrealEngine/Games/AZ/docs/design-briefs/claude-throwable-completion-work-order.md) and [completion audit](C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-completion-audit.md). They supersede historical missing-code findings below; preserve repaired APawn hosting, bilateral collision, full transforms, contact time and the working inventory release path.

**Art is Codex-owned and supplied:** use the [Quiet Sage production kit](<C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_Art_v01/README.md>),11 saved Unreal assets and current BP art assignments. Claude handles runtime integration and requests art revisions from Codex, not replacement placeholder art. Main ribbon/contact assets are already assigned; blocked/body states, projection and HUD still need integration.

**September17 user clarification — mandatory:** grenade goes under **Equippables**; equipping/selecting immediately activates the held idle before RMB. Explore carry and held preparation/aim must retain animated lower-body walk/run/crouch through the existing upper/lower-body blend arrangement. Implement section0 of the current work order, including inventory Equip, direct slot and QuickSelect paths; do not retain a FullBody aiming loop or root the player as a substitute for correct blending.

Use existing grenade HUD texture `/Game/FPS_Controller/UI/Textures/T_FragGrenadeIcon.T_FragGrenadeIcon` through the item's ImageFragment/view. Show grenade icon/name/count immediately while selected in Explore; the equipment row changes presentation, not gameplay mode, and restores the ordinary mode/weapon row afterward. Health stays in place.

Read first:

1. `C:/UnrealEngine/Games/AZ/AGENTS.md` and applicable project skills.
2. `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-system-implementation-plan.md` — complete behavior, ownership, phases and acceptance criteria.
3. `C:/UnrealEngine/Games/AZ/Saved/ThrowPlanning/inventory-gas-audit.md`, `animation-camera-audit.md`, `projectile-prediction-audit.md` — measured findings and exact source/asset references. Revalidate live state before mutation; do not rediscover the project from scratch.
4. Selected editable art: `C:/UnrealEngine/Games/AZ/UI Design/CHALK_Throw_v01/CHALK_Throw_03_NATIVE.xcf`; preview `CHALK_Throw_03.png` in that folder.
5. September16 clarification: `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-phase0-review.md`. Read it with your Phase0 report: retarget unarmed for the delivered stone slice, leave MMB unchanged, select the provisional preview candidate while aiming and lock it at commit, and match bilateral collision responses rather than assuming Ch2 alone proves parity. Reported pistol release times are calibration candidates, not universal cue values.
6. Foundation review: `C:/UnrealEngine/Games/AZ/docs/design-briefs/throwable-foundation-review.md`. Correct the actual APawn/Mover host API, response-aware queries, grip transform/failure handling, activation ownership and remote presentation before relying on the foundation for the transaction.

## Required experience

- Select/ready an owned throwable through the existing inventory/quick-select flow.
- **RMB Started → Start → Loop + trajectory while RMB remains held. RMB release → throw + recovery. LMB press → cancel before physical release.** Holding does not charge power or cook a grenade; no automatic repeat.
- Keep Start: the audited families have matching Start/Loop/release endpoints. During Start, accept one early RMB-release intent for this action, then enter release at the verified ready seam without playing an extra Loop cycle. While already in Loop, respond promptly to release rather than waiting for its full1.23s duration. Apply appropriate pose blending; a section jump alone is not a crossfade.
- LMB cancels Preparing/Aiming, a queued early-release intent, or Windup before the physical release cue. Clear/disarm the action first. Releasing the still-held RMB after cancellation must not throw or restart; require a fresh RMB press. After physical release, cancellation cannot refund or recall the projectile.
- Preserve **1×** at sequence, segment, montage and runtime levels. Recovery exists inside release clips; do not append redundant recovery or hide delays behind timers.
- Quiet Sage: arc/outer marker `#B5C8B7`, warm-white center/text `#EEEAE0`, four open corners, restrained pulse/dash treatment, no heavy bloom. Health stays in place. Runtime counts, names and key hints are real data.
- Preview ends at **first contact**. Surface-aligned marker, no marker in open space, no false continuation through cover. This is not a grenade blast-radius or final-rest prediction.

## Existing integration points and non-negotiable ownership

Inventory owns identity/count/transfer; QuickBar owns readiness/manual bindings; Equipment owns committed weapons and temporary hand presentation; GAS owns action lifecycle; Mover owns the hero; the thrown world actor owns its flight/fuse/impact after release.

Extend readiness by throwable capability, not just the existing exact Consumable category, so a unique knife is supported. Use one shared ability activated from the chosen source; do not give another ability per stack unit. Add a readiness generation so stale input for a previous selection cannot affect the same GUID selected again later.

Current RMB maps to **both Aim and SecondaryAttack**. Intercept throwable context before normal ASC dispatch: one canonical RMB Started prepares, its canonical Released requests the throw, and both ordinary RMB routes are consumed. LMB cancels without firing a rifle or punching. Preserve menu capture, mouse-release suppression after quick-select and explicit Started activation; ASC Pressed alone does not activate an inactive ability. Set the spec's InputPressed state correctly and use `WaitInputRelease` or an equivalent active-spec release listener. Never feed throw through Held retries. Invalidate cancellation before input cleanup can synthesize a release, and verify cancel-while-held then release produces no throw.

Weapon-context grenade animations can retain the gun in the other hand. Use a source/generation-owned Equipment presentation lease; do not blindly commit a holster that clears QuickBar readiness. Hand/grip/family chooses offhand carry or temporary holster. Suppress conflicting fire/aim/reload, firearm overlays and throwing-hand support IK. Restore only the still-matching selection generation. If throwing the equipped unique knife, transfer/retire that exact presentation without leaving a duplicate active weapon.

## Content facts to use in Phase0

- Exact-MH in-place pistol family: `/Game/AZ/Assets/Pistol/AZ_Pistol_Grenade_Throw_{Start,Loop,Close,Far,Cancel,Single}`. Start0.766667s, Loop1.233333s, Close/Far1.5s, Cancel1.066667s, Single1.8s. All RateScale1, nonadditive, no release notifies. Avoid root-motion-enabled suffix1 variants for movable upper-body actions.
- Screenshot rifle source: `/Game/RifleAnimsetPro/Animations/InPlace/Rifle_Grenade_Throw_*`. These inspected assets are on vendor UE4 skeleton; no exact-MH counterpart surfaced in the audit. Check user-provided additional retargets before creating duplicates.
- Unarmed source: `/Game/MovementAnimsetPro/Animations/InPlace/Throw_*`, `ThrowLoop`, `ThrowEndClose/Far`, `ThrowCancel`. Existing `AnimPro_Throw*` references SurvivalMan; compatible-skeleton metadata is not final visual validation.
- Knife source candidate: `/Game/FightingAnimsetPro/Animations/InPlace/KB_KnifeThrow`. Suitable MH hold/grip/release continuity remains to be established.
- Rifle/pistol throw with **left hand**; unarmed with **right hand**. Do not hardcode every prop to one hand. `Hand_LeftSocket` exists but is not a calibrated universal grip. Do not move existing gun sockets.
- Preserve grounded lower-body locomotion under an in-place upper-body action. Current graph has `RifleFire` in `WeaponFire`; evaluate reuse with mutual exclusion. Avoid an indefinite FullBody loop or RM variant contaminating RootMotionFromEverything. Throw camera/pose context must be separate from firearm `Ability.State.Aiming` and its FOV50/30cm zoom.

Create project-owned data/montage assets; source packs remain unchanged. Measure release, hand-ready/cancel and recovery boundaries per chosen clip on the actual MH body. Single is optional future quick throw, not required for this hold/release mechanic.

## Exactly-once release

Use the inventory reload transaction as the pattern. Proposed APIs/types may be renamed to project conventions; preserve this behavior:

1. RMB Started creates a ThrowActionId and reserves one owned unit, capturing avatar/item/ready/equipment generations and quantity/location revision. No consumption yet.
2. RMB release starts Windup; an early-release latch branches from Start at its verified seam unless cancelled. Freeze accepted aim intent when Windup actually begins.
3. At the measured release cue, authority validates action/montage/phase/source and actual hand clearance. Prepare an inert hidden/non-colliding projectile with one-unit payload, then revalidate after construction callbacks.
4. Commit inventory changes and the release receipt under a guard **before** broadcasting inventory/readiness changes. Activate the projectile only after commit. Repeated cue/RPC returns the existing receipt. Depleting the last unit must not cancel/destroy the released projectile through a reentrant callback.
5. Before release, cancellation spends nothing and destroys only its staged prop/projectile. After release, cancellation never refunds or recalls the projectile. A missing cue cancels; no watchdog-generated throw.
6. Knife transfers its exact GUID/state. A partial stack's emitted unit gets a separate world identity. Settlement/pickup has one item owner, including full-inventory and repeated-pickup cases.

Do not use instant `Server_ConsumeItem` or passive randomized `SpawnDroppedItem` for a throw. Competing drop/split/merge/use/equip operations must respect the reservation. Keep ordinary unrelated inventory behavior and its appearance intact.

## Ballistics and outcomes

One launch-solution builder drives local preview and authoritative release using shared origin calibration, speed, gravity, sphere radius, velocity inheritance and collision policy. Camera supplies bounded aim intent; the actual finalized hand/socket supplies release. Never spawn from camera or teleport through cover to match a marker. Check capsule-interior-to-hand corridor and initial overlap at release. Account for the difference between Loop grip and later release pose through a calibrated preview anchor and measured tolerances.

During Preparing/Aiming, the displayed arc must already use a stable provisional Close/Far candidate and its release transform. Windup freezes that displayed choice; it does not introduce a different origin only after RMB release. Include the intended sphere's response container and ignore rules in collision prediction/launch checks; the native bare channel sweep omits those per-projectile responses.

Use native first-impact prediction and a swept sphere projectile component for the initial model; no concurrent Chaos owner, homing, speed clamp or forces absent from preview. `OverrideGravityZ=0` means world gravity. The predictor does not handle bounces/final rest. Preview is owner-only and pooled, with bounded samples/update rate; no preview projectiles, damage or hearing events.

**Audit the collision configuration before implementation:** legacy `ECC_Projectile` macro points to channel1 while configured Projectile is channel2, and the existing Projectile profile overlaps Pawns. Establish a verified throwable contract without a global unrelated collision migration. Bodies block regardless of whether damage is permitted.

- Stone: bounce/settle, authoritative impact noise, optional damage and one recoverable unit. AZ hearing requires appropriate hostile instigator/team attribution; report at impact but preserve thrower attribution.
- Grenade: recommended fuse begins on server release; no cooking. Server fuse survives rest/ability cancellation and detonates once at current position. Occluded distance-falloff damage goes through existing GAS damage/Vitals, once per target. Native radial Actor.TakeDamage has no verified AZ GAS bridge. Expose self/friendly damage policy; the plan proposes self-damage on and friendly-NPC damage off.
- Knife: one permitted hit outcome, calibrated visual embed/stop, recoverable exact item identity. Target destruction cannot duplicate or erase ownership.

Keep replicated release/fuse times, inventory/impact authority and source attribution sound for listen-server extension. Client-predicted projectile polish may follow the SP-first slice; remote clients must never spawn a second damaging projectile. Server release must work when the character is off screen.

## Execution order and completion

Follow plan phases:0 content/input/collision audit →1 complete stone slice →2 grenade + weapon families →3 unique knife →4 visual/movement/cancellation/performance polish. Reuse the three audit notes; make routine code organization choices yourself. Record evidence and exact content gaps rather than claiming unsupported animations work. Do not call the whole task complete with only a stone proof.

Build using `C:/UnrealEngine/Games/AZ/.agents/skills/cpp-build-livecoding/SKILL.md`; new reflected types/fields need a full build and restart, not a Live Coding reflection shortcut. Verify only affected assets, preserve unrelated edits, and give the user a concise Play checklist only after build success. **Do not add automated tests or start PIE/editor gameplay tests without explicit permission.** User testing plus log inspection is the default.

Completion report must identify changed code/assets, build/load/save evidence, actual skeleton/grip/release frames, item/duplicate-release and cancellation behavior, chosen preview semantics, and remaining user gameplay checks. Required checks are listed in the full plan. Do not replace the user's LMB/RMB controls or Quiet Sage selection with a preferred convention.
