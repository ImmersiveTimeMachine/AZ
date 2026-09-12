# Proposed firearm readiness and left-click shooting

**Approved and implemented:** current source/build/readback status is in
C:/UnrealEngine/Games/AZ/docs/design-briefs/firearm-ready-implementation-status.md.
The proposal below is retained as the accepted design rationale.

2026-09-11. User asked for a plan before implementing gameplay: with a weapon,
left-click should raise/fire using the aiming posture with no or reduced zoom;
after roughly three seconds without shooting, return to relaxed. Gameplay below
is proposed only. The separately requested icon repairs are implemented.

## Recommended behavior

| State | Entry / input | Presentation |
|---|---|---|
| Relaxed | No recent shooting and RMB released | Existing relaxed pose and normal camera |
| Ready | LMB with an active firearm | Raise into existing aiming posture, show reticle, normal camera/no added zoom |
| Precision aim | Hold RMB | Existing aimed posture, shoulder camera and zoom |

Default readiness duration:3seconds after the last accepted shot. Every actual
shot refreshes it. Held RMB always keeps precision aim active. Releasing RMB
returns to Ready if its timer remains; otherwise it fades to Relaxed. Keep the
camera stable for LMB-only shooting initially; reduced zoom can be tuned later.

A click from Relaxed starts a short configurable raise (~.12–.18s), retaining one
initial shot so a quick click is not lost. Already-raised firing has no extra raise
delay. SINGLE remains one shot per click; AUTO repeats only while the trigger is
held. Keep the accepted-shot cadence and ammo transaction authoritative.

Existing reload, ammo persistence, damage and recoil remain. A reload that starts
from Ready must not lower the weapon or change its grip halfway through: retain
that readiness through the action, then restart its3s window after completion.
A reload started while relaxed can retain the current relaxed presentation.
Dry firing does not debit ammo or apply recoil; use the existing empty reload
request when a compatible loaded magazine exists, without an endless retry loop.

Weapon switch, drop, inventory opening, death, hard interruption or an explicit
sprint request cancel the pending first shot and clear temporary readiness.
LMB from sprint first stops sprint and raises the gun. No delayed shot may fire
from another weapon after switching. Existing unarmed/melee input is unchanged.

## Implementation approach after approval

The current Ability.State.Aiming drives BOTH camera precision and the raised
posture/facing/firing/recoil/reticle gates. Simply disabling bRequiresAimToFire
would permit shooting from a lowered pose and leave the other systems tied to RMB.

Add a separate source-owned timed firearm-ready GAS state. Do not reuse fists'
Combat.Ready state/effect. Use a per-selection effect handle or equivalent owned
lifetime with weapon ID/generation checks and cleanup; do not introduce another
ammo store or persistent item field for temporary presentation.

Use Ready OR explicit Aiming for raised character poses, appropriate aim movement/
facing, weapon socket, reticle, recoil and firing eligibility. Use explicit Aiming
alone for precision-camera zoom. Reload keeps its latched source and grip policy.
Update the existing RMB-release recoil clearing and aim-loss fire-stop paths to
respect readiness, so releasing RMB does not cancel an otherwise valid burst.

Stages: define state/cleanup; implement raise + first-shot request; connect pose/
camera/reticle/recoil; preserve reload and cancel behavior; build; user gameplay
checks for pistol and rifle SINGLE/AUTO. No gameplay code or input changes have
been made for this proposal, and no automated tests/PIE were started.
