"""PreToolUse guard: refuse shell commands that delete files git no longer tracks.

On 2026-09-22 a filter-branch strip deleted 731 files from disk (hero face mesh, UI Design/, a Fab plugin).
"""
import json
import re
import sys

GIT = r"\bgit(?:\s+-[Cc]\s+\S+|\s+--\S+)*\s+"
SEGMENT_END = r"[^\n;&|]*"

RULES = [
    (re.compile(GIT + r"filter-(?:branch|repo)\b|\bgit-filter-repo\b", re.I),
     "rewrites git history"),
    (re.compile(r"(?:^|[;&|(]\s*|-jar\s+\S*?)bfg(?:-[\d.]+)?(?:\.jar)?(?=\s|$)", re.I | re.M),
     "rewrites git history (BFG)"),
    (re.compile(GIT + r"clean\b" + SEGMENT_END, re.I),
     "deletes untracked/ignored files (Content, UI Design and Plugins live there)"),
]

DRY_RUN = re.compile(r"(?:\s-[a-zA-Z]*n[a-zA-Z]*\b|\s--dry-run\b)")

# Prose that merely MENTIONS these commands (commit messages, here-strings, heredocs) must not trip the guard.
PROSE = [
    re.compile(r"@'[\s\S]*?'@|@\"[\s\S]*?\"@"),
    re.compile(r"<<-?\s*['\"]?(\w+)['\"]?[\s\S]*?^\s*\1\s*$", re.M),
    re.compile(r"(?:\s-m|\s--message)[\s=]+(?:\"(?:[^\"\\]|\\.)*\"|'[^']*')"),
]


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        return
    command = (payload.get("tool_input") or {}).get("command") or ""
    for prose in PROSE:
        command = prose.sub(" ", command)
    for pattern, why in RULES:
        match = pattern.search(command)
        if not match:
            continue
        if why.startswith("deletes") and DRY_RUN.search(match.group(0)):
            continue
        reason = (
            f"BLOCKED by the AZ project guard: this command {why}. On 2026-09-22 a history rewrite deleted the "
            "hero face mesh and 730 other files from disk. Do not work around this guard (no other shell, no "
            "script, no alias). If it is truly needed: stop, explain to Artur, copy the affected paths aside, and "
            "let him run it himself. See memory feedback_history_rewrite_deletes_files.md and AGENTS.md."
        )
        json.dump({"hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": reason,
        }}, sys.stdout)
        return


if __name__ == "__main__":
    main()
