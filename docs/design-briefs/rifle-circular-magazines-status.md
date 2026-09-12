# Circular rifle magazines and inventory loading

Latest user correction: keep the main HUD magazine icon/count. It is restored, and dropped-magazine text now reads current rounds; magazine inventory footprints are 1x1. See C:/UnrealEngine/Games/AZ/docs/design-briefs/magazine-display-fixes-status.md. The inserted-only HUD visibility described below is superseded; circular reload behavior remains unchanged.

Updated September 9 local / September 10 UTC, 2026. User explicitly approved this
policy and requested implementation. It supersedes the earlier fullest-magazine
selection and the spare-magazine HUD readout.

## Behavior

- Inventory owns every physical magazine's identity, remaining rounds, location,
  and link to the rifle. Shots debit that inserted magazine. A magazine with17
  rounds has12 after five accepted shots and retains12 when removed, dropped or
  picked up again. Reload never refills, pools, duplicates or discards ammunition.
- Manual R cycles compatible magazines in a stable identity order, wrapping after
  the last. Full, equal, lower-count and empty magazines are allowed. There must
  be another owned compatible magazine and space to return the current one.
- Automatic empty reload and a fresh dry trigger follow the same order while
  skipping empty magazines. No held-trigger resume and no repeated automatic
  retry loop. Manually inserting an empty magazine is allowed; a subsequent dry
  trigger can request a nonempty one.
- Inventory's magazine action is **Load into rifle**. It targets that exact
  magazine for the active compatible rifle, closes inventory, and uses the same
  reload ability, animation and timed commit as R. No instant inventory ammo swap.
  The rifle must be selected first; an unavailable action is disabled.
- HUD shows only the inserted magazine's rounds/capacity, EMPTY via0/capacity,
  NO MAG or unavailable. Spare-magazine text and its icon are hidden. Inventory
  continues showing each backpack magazine's current rounds/capacity; the rifle
  badge and description show its linked inserted magazine and ammunition.
- Dropping remains an inventory operation. Reload returns the outgoing magazine
  to inventory; an inserted magazine travels with its dropped rifle, preserving
  its identity and current rounds.

## Implementation

The ring sorts magazine GUIDs lexicographically. The current inserted GUID is its
cursor, so no duplicate replicated selection list, round count or persistent
cursor is needed. Pickup/drop preserves identities; grid movement does not reorder
the ring. A rifle with no magazine begins at the first eligible identity. Exact
inventory selection never falls back to a different magazine. The next R resumes
the ring after the newly inserted identity.

Inventory preflight and reservation take optional RequestedMagazineId and
bSkipEmpty parameters. Commit retains ownership, source/generation, item revision,
round count and placement checks. It removes the old full-magazine and
incoming-rounds-greater-than-current restrictions. Completion still moves the
same objects and their locations without changing their round counts.

The inventory action captures the expected rifle/source/generation, target ID and
revision, and currently inserted ID/revision before closing the menu. Authority
rejects stale receipts and requests the source-owned reload ability. The ability
captures request options for its activation before callback-producing operations.
Ordinary R remains the default manual request. Automatic requests retain their
exact empty-shot receipt and transport the skip-empty policy to authority.

Existing inventory item badges, descriptions and HUD all derive from the same
inventory items and update on inventory notifications. Magazine loading reuses
the popup action button's presentation with a separate load callback; magazines
do not receive equipment fragments or character equipment slots.

## Validation / continuation

Implementation and source review complete. The first build at01:10:57UTC failed
inside UHT parsing the unchanged engine NavMovementComponent.h with a
NullReferenceException. Before further diagnostics, the user reported a PC crash
and restarted/rebuilt. No cause for the PC crash was established.

The user's normal build at01:22:17UTC succeeded in35.26s, compiling the inventory,
popup/grid, HUD, equipment and reload changes and linking the main AZ DLL at
01:22:49UTC. Unreal Editor PID16668 started at01:23:05UTC. No source file is newer
than that DLL. Generated code contains both new server RPCs and the popup handler
carrying the magazine GUID. Focused git diff --check passed.

Final review fixed a stale-popup callback: each popup captures its own magazine
GUID and passes it with the cell index. The grid rejects mismatches with the
current popup/occupant, and destruction unbinds the load delegate. The inventory
request independently retains incoming and inserted magazine revisions across
menu-close callbacks. These fixes are included in the successful build.

Crash follow-up found only docs/design-briefs/README.md entirely zeroed among
text source/docs/tools files. Backed up its bytes under
C:/UnrealEngine/Games/AZ/Saved/Backups/CircularMagazines/README-crash-zeroed.md and
reconstructed the index from its committed baseline plus surviving brief titles.
No source file corruption was found in that check.

No asset edits or new Blueprint wiring are required. The reopened editor has the
new native implementation; no further rebuild is needed for this slice. Gameplay
acceptance remains pending. Codex did not start PIE or editor tests and did not
add automated tests.

User gameplay checks after the successful build:

1. With at least three compatible magazines, press R repeatedly while SINGLE,
   including when full. Confirm a complete repeating cycle. Repeat in AUTO.
2. Shoot five rounds from a known magazine, cycle away and back. Confirm exactly
   five fewer rounds on the HUD and the inventory rifle/magazine readouts.
3. Select a particular spare in inventory and choose Load into rifle. Menu closes;
   reload animation completes before ownership/readouts change. The old magazine
   returns to inventory with its remaining rounds, even if the incoming has fewer.
4. Empty a magazine with an empty spare ahead of a loaded spare in the ring.
   Automatic reload skips the empty spare. With only empty spares it stops.
5. Switch weapon, change crouch stance, reopen inventory or drop the rifle before
   completion. No late swap, ammunition gain/loss or stale selection afterward.
6. Drop and pick up partial spare magazines and the loaded rifle from inventory;
   the identities, links and remaining rounds must be preserved.

Existing aimed reload is2s; standing relaxed is5.3s. Timing/prop polish remains
separate from this inventory-selection change.
