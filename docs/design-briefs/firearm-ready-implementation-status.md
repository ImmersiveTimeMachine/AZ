# Firearm Ready — implemented and loaded

2026-09-11. User approved the Ready gameplay plan. Both pistol and rifle now use
the existing shared firing ability to raise/fire from LMB without precision zoom.
RMB retains its own aiming/zoom lifetime. No ammo store or input action was added.

## Behavior

- A cold trigger begins Ready with an initial configurable raise delay (.12s).
  One initial shot is retained for a quick tap. Only authority's timer executes
  that shot, after rechecking action/source/item/generation/mode and magazine
  identity/revision. No round is spent at press or raise time.
- Actual accepted shots refresh Ready for3s. SINGLE stays one shot per press;
  AUTO repeats only while held. Held AUTO owns a separate token hold so low-rate
  weapons cannot be lowered between shots. Releasing resumes the remaining
  accepted-shot deadline, without refreshing it.
- Ready and explicit Aiming both drive raised poses, facing/strafe, weapon socket,
  reticle, recoil and fire eligibility. Precision camera selection still reads
  only Ability.State.Aiming. Releasing RMB does not stop a valid Ready burst.
- Reload entered while raised owns an exact action-token hold. It keeps the grip
  through the action; successful completion starts a fresh Ready window. Relaxed
  reloads stay relaxed. Inventory's committed bit is read before releasing the
  reservation, including reentrant inventory notification/cancellation cases.
- Inventory/quick-select capture, switching, dropping, death/hard blocks, melee,
  possession changes and sprint clear owned readiness and pending firing. Existing
  reload retains priority over a new sprint request. Primary presses during a
  weapon switch are discarded instead of entering the melee buffer and later
  firing the incoming weapon. Unarmed/melee input otherwise remains unchanged.

## Ownership and network details

Equipment owns Ready and its counted Movement.Strafe contribution, timers,
source/item/generation/lifetime IDs and AUTO/reload holds. It removes only its
contributions. The existing fists Combat.Ready effect is not reused.

New implementation file:
C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentReady.cpp.
Native tag Ability.State.FirearmReady; WeaponState fields ReadyDurationSeconds
and FirearmRaiseDelaySeconds default3 and.12, with editable timing metadata.

FireGA binds WaitInputRelease before callbacks or timers. Its owning client sends
the normal GAS release event and waits for authority's End; it never sends a
normal replicated End early enough to cancel the server's delayed first shot.
Explicit cancellation still follows GAS normally. This favors correctness over
maximal rapid-click responsiveness at high round-trip latency; the game is SP-first.

Accepted-shot receipt carries server time and GAS preparation prediction key.
Equipment keeps a bounded key set for the current Ready lifetime, rejecting old
receipts after cancel/reprepare without falsely rejecting a legitimate current
shot due to client clock-estimate skew. Duplicate valid receipts are idempotent;
the recoil path separately deduplicates shot IDs. Server checks remain authoritative.
Cancelled, unfulfilled preparation is rolled back without clearing accepted-shot
readiness, independent RMB ownership or a pending reload handoff.

Remote Sprint needed an authority preactivation fix because Sprint's existing
Blueprint blocks Movement.Strafe. AZ ASC overrides the existing Sprint activation
RPC implementation to validate the current owned firearm and release Ready/aim/fire
before calling Super with the same handle and prediction key. No new Sprint RPC
or side-effecting CanActivate query was introduced.

Reload presentation carries exact item ID, selection generation, raised-at-start
and server start time. Owner holds reject stale requests after explicit cancellation.
Ready renewal avoids a transient tag-zero pulse at the expiry boundary, and initial
grace includes the raise delay so low timeout settings cannot expire before shooting.

## Build and validation

Full build passed. A concurrent weapon-switch compile error (local Hero shadowing)
was fixed while preserving that workstream. The latest normal build before the
last small guard completed in47.35s; main DLL22:14:01UTC, editor PID42508 started
22:14:14UTC. The final primary-during-switch function-body guard was then Live Coded:
UBT Result:Succeeded31.00s, AZ patch successful22:16:20.895UTC, patch session finished
22:16:23.019UTC. No new class-layout change was applied through Live Coding.

Live native readback confirmed Ability.State.FirearmReady exists, both rifle and
pistol manifests have ReadyDurationSeconds3.0/FirearmRaiseDelaySeconds.12, and both
fire Blueprint CDOs use LocalPredicted with remote cancellation enabled. RPC
signatures/call sites and source whitespace checks passed. No gameplay assets or
input mappings needed reassignment. No automated tests were added/run; Codex did
not start PIE or invoke gameplay tests. The user was already in Play during the
final read-only check. Gameplay/visual acceptance remains for the user.

The current editor has the complete change. A normal build before the next editor
launch will retain the final small input guard, because Live Coding patches do not
replace the main DLL. Full Ready functionality is already in that main build.

User checks: tap LMB while relaxed; confirm exactly one shot after raise and no zoom;
wait3s to relax; hold RMB past3s; release RMB after shooting; hold rifle AUTO; reload
while raised; switch/sprint/open inventory during raise; check ammunition remains
exact. Exposed timings live in Pickup Item Manifest → Weapon State Fragment.
