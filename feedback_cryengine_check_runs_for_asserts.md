---
name: feedback-cryengine-check-runs-for-asserts
description: "Artur's rule (2026-10-18): watch every own CryEngine AZ launch for 'Assertion Failed' dialogs and Rider pauses; never leave a run blocked. run_game_test.py now catches them."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 0cf39d47-f3b0-4a50-9b24-845e8ec0506f
  modified: 2026-10-18T18:18:08.769Z
---

**Rule (Artur, 2026-10-18 ~14:05):** «Постоянно проверяйте ваши запуски, там вылетает ошибка постоянно». A run sat on an «Assertion Failed» dialog and «ждёт вашей активности». Earlier, Rider paused on the same assert in another agent's run.

**Why:** a CryEngine assert dialog, or a debugger break under Rider, blocks the game. The agent's `--wait-end` then just times out, and Artur has to click «Ignore All» himself.

**How to apply:**
- The harness `C:/CryEngine/AZ/tools/ui/run_game_test.py` (since 2026-10-18 14:33, team rule Q-LAUNCH-GUARD: no auto-Ignore):
  - It binds the run to its exact GameLauncher (pid and creation time in state.json).
  - On an 'Assertion Failed' dialog it logs `pid | file:line | condition | reason` to asserts.txt.
  - `--wait-end` then exits with code 3 ('ASSERT NEEDS ATTENTION') and leaves the dialog up.
  - Capture the stack with `--assert break` plus Rider `xdebug_get_stack` / `xdebug_get_frame_values`, then `--assert ignore|continue|stop` or stop the session.
  - `--on-assert ignore` is an explicit, logged opt-in only.
  - `--collect` prints `asserts: none` or `WARNING: N`.
- A Rider pause is different: the dialog is not shown, the debugger session is paused. Check with `xdebug_get_debugger_status` and `xdebug_get_stack`, then fix the cause and use `xdebug_control_session --action resume`.
- A dialog outside the harness can be clicked through user32 (`BM_CLICK` on control id 1005 = Ignore). Controls: 1000 line, 1001 file, 1002 condition, 1004 reason.

Related: [[feedback-cryengine-rider-debug-runs]], [[project-cryengine-dx12]].
