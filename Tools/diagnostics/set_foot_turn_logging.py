"""Enable only existing foot/turn logs; call set_logging(False) to disable.

Run once after review in the editor Python tool. This does not start/stop PIE,
install hooks, change animation behavior, or save assets. CVars are process-wide.
"""
import json
import unreal


def set_logging(enable=True):
    if not isinstance(enable, bool):
        raise TypeError("enable must be True or False")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() or editor.get_editor_world()
    if world is None:
        raise RuntimeError("No existing game/editor world; no commands sent")
    values = {"az.Aim.Debug": 1, "az.Cam.Debug": 1, "az.TipRate.Debug": 2}
    if not enable:
        values = {name: 0 for name in values}
    before = {name: unreal.SystemLibrary.get_console_variable_string_value(name)
              for name in values}
    missing = [name for name, value in before.items() if value == ""]
    if missing:
        raise RuntimeError("CVars unavailable; no commands sent: " + ", ".join(missing))
    for name, value in values.items():
        unreal.SystemLibrary.execute_console_command(world, name + " " + str(value))
    after = {name: unreal.SystemLibrary.get_console_variable_string_value(name)
             for name in values}
    report = {"world": world.get_path_name(), "requested": values,
              "before": before, "after": after,
              "verified": all(after[name] == str(value) for name, value in values.items())}
    print("[FootTurnLogging] " + json.dumps(report, separators=(",", ":")))
    return report


if __name__ == "__main__":
    set_logging()
