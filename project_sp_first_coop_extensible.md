---
name: AZ scope — single-player first, co-op extensible (basic decisions, not full implementation)
description: Project-level scope commitment (2026-05-11). CHALK ships single-player first. Co-op is a *possible future* — not all features need to be co-op, but the basic architectural decisions should not block extension to co-op later. This file lists the concrete rules: what to ALWAYS do correctly (cheap, no SP cost, unblocks co-op), what to AVOID (creates retrofit work later), and what can be safely DEFERRED until co-op work begins.
type: project
originSessionId: 6c1c8fd6-056f-42e0-87d3-c943f4c8cf3d
---
# Scope: single-player first, co-op extensible

CHALK is single-player first. Co-op is a possible future — **not all features need to be co-op**. The architectural commitment is:

> **A listen-server PIE with 2 local players should "just work" without code refactor**, even before any explicit co-op work has been done. We pay no full co-op cost upfront, but we never write code that would *block* co-op extension.

This is the middle path between "build SP-only, refactor for co-op" (most common, very expensive) and "build full co-op upfront" (wasteful for an SP-first game).

## ALWAYS do correctly (cheap, no SP cost, unblocks co-op)

These are the design defaults. Free during SP development; forced retrofit if skipped.

1. **No `UGameplayStatics::GetPlayerController(World, 0)` anywhere.** Code that needs "the player" gets it from context — an interaction's Instigator, an ability's owning ASC, a PC reference parameter. Banning index-0 lookups is the single highest-leverage rule.
2. **No singletons for player-owned state.** No `UAZ_GameInstance::GetMainPlayer()`. No "the player" globals. Inventory, attributes, ability state already live on PlayerState (correct AZ pattern) — keep it that way.
3. **State location follows scope:**
   - **PlayerState** — per-player: ASC, attributes, inventory, personal quest progress, kill counts
   - **GameState** — shared world: outbreak phase, district timers, world quest progress, weather, day/night
   - **GameInstance** — cross-level persistent: save slot binding, settings, profile
   - **LocalPlayer subsystem** — per-local-client UI/input/camera prefs
4. **All gameplay state mutations through GAS or replicated events.** No direct pawn-to-pawn function calls that change state. Pickup = `GA_PickupItem` activated by the picking player's ASC. Door open = server RPC on the door actor, server replicates state. Already implied by AZ's GAS adoption — extend the discipline to everything.
5. **Mover for ALL movement.** Hero, NPCs, future vehicles. Mover's NetworkPrediction handles client-server reconciliation. Don't add custom `bReplicateMovement` paths.
6. **Determinism in trajectory and chooser inputs.** No `World->GetTimeSeconds()`-based randomness, no per-frame `FMath::Rand` in the anim/trajectory hot path. PoseSearch is deterministic given the same trajectory — same anims on both ends, free.
7. **Interaction owner is always known.** `IInteractableInterface::Interact(AActor* Instigator)` — even in SP. No "find the player and call them" — the interaction *carries* the player.
8. **AI perception per-player.** Stimulus from each player's pawn. BT services iterate `World->GetPlayerControllerIterator()`, never assume one player. Free in SP (one PC in the loop); works in co-op (two PCs in the loop).
9. **Per-player camera, per-player HUD.** ULocalPlayer subsystem for input. Each PC has its own HUD. Standard UE — just don't break it by putting camera state on GameState.
10. **AGameMode handles spawn.** `ChoosePlayerStart_Implementation` per player. Default GameMode behavior — don't override with a hard-coded "spawn at origin."
11. **Cutscene scope tagged from day 1.** Each Sequencer cinematic is either *personal* (binds only the triggering player's camera) or *world* (locks all players). Decide per cinematic. v2 architecture already uses PC-bound Sequencer — extend by tagging scope.
12. **Pickups / world objects are server-authoritative actors.** Even in SP. Listen-host = server, zero perf cost. In co-op, zero refactor. Avoid client-only BP state for world-interactive objects.
13. **Save system splits world vs character.** World state serialized from GameState (shared); character state serialized per-PlayerState. SP save = 1 player serialized; co-op save = N players serialized. Same code path.

## AVOID (these create retrofit pain — never write them, even in SP)

- Reaching for the player via `(World, 0)` index. Use context.
- "Global player" singletons or static accessors.
- Direct `Cast<>` between unrelated actors when an interface would do — interfaces let server-side and client-side resolve to different concrete classes without breaking calls.
- Tick polling of player position from arbitrary actors. Use perception, overlap, or bound events.
- Save serialization that walks the world and writes "the player pawn." Walk PlayerStates instead.
- Putting per-player UI state on GameState (it'll show on all players' HUDs in co-op).
- Custom replication channels for things GAS or Mover already handle. Don't reinvent.
- `IsStandalone()`-gated debug paths that quietly break in PIE-MP. Make debug HUDs per-PC and they work in either mode.

## DEFER until co-op work begins (real cost, SP doesn't need it)

Don't pay for these until co-op is actually on the table. The above ALWAYS rules make sure these are *additive*, not retrofitting.

- Lobby UI / matchmaking / Steam relay
- Voice chat / proximity audio
- Late-join handling, host migration, disconnect recovery
- Dedicated server build target (listen-server ships SP and co-op both — dedicated comes later if needed)
- PIE multiplayer test pass
- Latency compensation tweaks for fast events (TIP commit, hit detection)
- Cosmetic smoothing on remote pawns (PoseSearch trajectory noise dampening)
- Anti-cheat / server validation tightening beyond GAS defaults
- Bandwidth optimization passes
- Per-player scoreboard / player-count UI / lobby pause-on-join etc.
- Anti-grief / kick / report systems

## OK to be SP-only (no co-op extension needed)

Some features make no sense in co-op or are tedious to extend. Explicitly mark them SP-only:

- **Photo mode / pause-the-world** — SP-only. Co-op variant would need design (pause for one player vs all).
- **Cinematic-driven QTE sequences** — likely SP-only or "personal" cinematics with co-op partner spectating.
- **Save anywhere mid-mission** — SP-only typically; co-op uses checkpoints.
- **Story-critical dialogue cutscenes with one player as protagonist** — design decision; either personal-scope or partner-spectates.
- **Single-protagonist narrative arcs** — if the story is "Artur's character," co-op is "Artur + friend tags along" not "two protagonists."

If a feature ends up SP-only, that's fine — just write it on top of the per-player infrastructure (use `APlayerController*` parameters, not globals) so a future co-op-aware variant can replace it without ripping out callers.

## What this means for current v2 work

The hero pawn (`AAZ_PawnMoverHeroCharacter`) and Step 2+ work proceeds with **SP as the validation target**, but every decision passes the "would this break with 2 players?" check:

- Step 2 (input wiring) — pawn-side `SetupPlayerInputComponent` + IMC per pawn. **Already co-op-friendly** (each PC pushes its pawn's IMC to its own EnhancedInput subsystem).
- Step 3 (Mover mode + `ResolveRotationTarget`) — already co-op-friendly (per-pawn instance, no global state).
- Step 4 (GAS link to PlayerState ASC) — already co-op-friendly (per-player ASC).
- Step 5+ per-state implementations — co-op-friendly as long as state mutations go through ASC/Mover and not direct pawn manipulation.

**No extra co-op work for the v2 character system.** It's already on the right rails.

**One known MP gap found 2026-05-29 (anim, not movement):** the v2 AnimInstance derives `bIsMoving` from `UMoverComponent::GetLastInputCmd()` (player move input). Input exists on the owning client (local) and the server (NetworkPrediction replays it) but is **NOT sent to simulated proxies** (other clients) by default — so on a replica the capsule moves (Mover replicates) yet the anim stays idle. **Fix = `bSyncInputsForSimProxy = true` on the Mover component** (engine flag built for "input used to hint at an anim graph"; ships input to sim proxies via the sync state). Everything else the AnimInstance reads already replicates (Mover sync state, GAS OwnedTags) or computes locally (curves, PoseHistory/MM off replicated velocity). Lesson for future anim/state work: **anything the AnimInstance reads must come from replicated state (Mover sync / GAS tags) or be explicitly synced — never assume input is present on sim proxies.** See [[project_v2_locomotion_progress]].

## What this means for upcoming game systems

When designing CHALK-specific systems (interaction, quest, save, faction reputation, vehicle possession, AI director, drug-as-mechanic, etc.) — **default to per-player state on PlayerState** and **shared world state on GameState**. Cross the boundary explicitly via GAS effects or replicated events.

Concrete examples for CHALK:
- **Quest "find Elysium stash"** — PlayerState progress if personal; GameState progress if shared world objective.
- **Outbreak phase timer** — GameState.
- **Player infection meter (if drug-as-mechanic ships)** — PlayerState attribute on player ASC.
- **NPC faction reputation** — likely GameState (shared world reaction to player actions) for SP; could split into per-player in co-op.
- **Vehicle ownership** — vehicle pawn has a "current driver" pointer (PlayerState reference, not pawn reference) so the vehicle survives a player respawn.

## The single bar

> Two listen-server PIE instances. Press play. No crash, no missing state, no "only the host can pick up items." If that test passes incidentally, the SP build is co-op-ready. If it doesn't, the failure point is the next thing to fix.

We don't run that test until co-op work is on the table — but we never write code that would fail it.
