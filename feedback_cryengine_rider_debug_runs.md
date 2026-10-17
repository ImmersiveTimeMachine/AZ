---
name: feedback-cryengine-rider-debug-runs
description: "Artur's rules for running and fixing things in CryEngine AZ during autonomous work - launch the game from Rider under the debugger, end it by stopping the session (no quit), and push render/engine bugs to tech debt instead of drifting from the main goal."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 8f8d09b9-3f5e-408a-8d76-c4e4c3910763
  modified: 2026-10-17T07:07:29.526Z
---

**Rule (Artur, 2026-10-17 night):**
- Launch CryEngine AZ game checks from Rider **in debug**, not with a bare GameLauncher. Use the shared run config "AZ GameLauncher test (Profile build)", started with `xdebug_start_debugger_session`.
- End the game by stopping the debug session (`xdebug_control_session --action stop`). Never end it with `quit`: the launcher crashes on quit (TD-009, likely assets, not our UI).
- Render and engine bugs found on the way go to `docs/TECH_DEBT.md`; do not spend the night on them. Exception: if a fix is already built, «раз собрано, доводи до конца».

**Why:**
- Under the debugger a crash stops with a stack, so there is no guessing from logs.
- `quit` leaves the program hanging or crashed.
- Artur wants the stated goal (quests, then inventory) finished, not side quests.

**How to apply:**
- Use `AZ/tools/ui/run_game_test.py --prepare`, then the Rider session, then `--wait-end`, then stop, then `--collect`.
- Run windowed 1920x1080: a 4K fullscreen run stopped on a `Present()` assert dialog.
- If the user plays during a run (shooting crates, for example), physical targets may move: do not treat that as a bug.

Related: [[project_cryengine_noesis_ui]], [[feedback_keep_stated_goal_scope]], [[feedback_rider_debugger_ensure_resume]].
