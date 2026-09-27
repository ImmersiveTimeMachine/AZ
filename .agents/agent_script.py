"""AZ local agent: an LM Studio model with (as far as a script can) the same kit as Claude Code in this project.

What the model gets:
  * every MCP server of the project: filesystem, unrealclaude, unreal-mcp, rider, gimp-mcp (same config as LM Studio's
    mcp.json and Claude Code's .mcp.json);
  * run_command: PowerShell on this machine. Every command is shown and needs your "y" (start with --auto to skip
    that). Commands the project git guard refuses (history rewrite, git clean) are refused here too - the SAME hook
    file decides, .claude/hooks/guard_git_destructive.py;
  * the project skills (.claude/skills, .agents/skills): list_skills / read_skill, loaded on demand like Claude Code;
  * the project rules (AGENTS.md) and the memory index (MEMORY.md) in the system prompt; read_memory for one file.

LM Studio's OpenAI-compatible endpoint does not run MCP tools itself; this script is the MCP host: it executes every
tool call the model makes and returns the result, until the model answers without a tool call.

Setup (PowerShell):  setx LM_STUDIO_API_KEY "<token from LM Studio>"   then open a new terminal
Run:                 python C:/UnrealEngine/Games/AZ/.agents/agent_script.py
Options:             --model qwen/qwen3.6-27b   --servers all | filesystem,unrealclaude,rider   --auto   --list-tools
Default servers: filesystem + unrealclaude (34 tools with the built-ins). All five = 194 tools: measured 2026-09-25,
bonsai-27b spent 10 min on that prompt and returned nothing - enable the others only when the task needs them.
One-shot:            --task "..." or --task-file path.md  (full trace saved to .agents/runs/; shell off unless --auto)
In the chat:         exit | /reset (forget the conversation) | /tools (list tool names)
"""
import argparse
import asyncio
import json
import os
import re
import subprocess
import sys
import time
from contextlib import AsyncExitStack
from datetime import datetime
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamablehttp_client
from openai import AsyncOpenAI

# ------------------------------------------------------------------ settings
LM_BASE_URL = "http://localhost:1234/v1"
DEFAULT_MODEL = "prism-ml/bonsai-27b"
TEMPERATURE = 0.1
MAX_STEPS = 40              # tool rounds per task before the script stops the model
MAX_TOOL_CHARS = 60000      # a full task spec must fit (whole-weapon-meshes.md is ~24k chars; 12k cut it)
COMMAND_TIMEOUT = 300       # seconds per run_command
DEFAULT_SERVERS = "filesystem,unrealclaude"

PROJECT = Path("C:/UnrealEngine/Games/AZ")
MEMORY_DIR = Path("C:/Users/Artur/.claude/projects/C--UnrealEngine-Games-AZ/memory")
SKILL_DIRS = [PROJECT / ".claude" / "skills", PROJECT / ".agents" / "skills"]      # first wins on a name clash
GIT_GUARD = PROJECT / ".claude" / "hooks" / "guard_git_destructive.py"
SERVER_LOG = PROJECT / ".agents" / "mcp_servers.log"
RUNS_DIR = PROJECT / ".agents" / "runs"              # one full trace per --task run
INTERACTIVE = True                                   # False in --task mode: nobody is there to approve a command

SERVERS = {
    "filesystem": {
        "command": "node",
        "args": ["C:/Users/Artur/AppData/Roaming/npm/node_modules/@modelcontextprotocol/server-filesystem/dist/index.js",
                 "C:/UnrealEngine/Games/AZ"],
    },
    "unrealclaude": {
        # the C:/UE57 copy is the INSTALLED bridge (the C:/UnrealEngine copy has no node_modules)
        "command": "node",
        "args": ["C:/UE57/Engine/Plugins/Marketplace/UnrealClaude/UnrealClaude/Resources/mcp-bridge/index.js"],
        "env": {"UNREAL_MCP_URL": "http://localhost:3000"},
    },
    "unreal-mcp": {"url": "http://127.0.0.1:8001/mcp"},
    "rider": {"url": "http://127.0.0.1:64343/stream"},
    "gimp-mcp": {
        "command": "C:/Users/Artur/.local/bin/uv.exe",
        "args": ["run", "--directory", "F:/Downloads/gimp-mcp-main", "gimp_mcp_server.py"],
    },
}


# ------------------------------------------------------------------ project context: rules, memory, skills
def read_text(path, limit=None):
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    return text if limit is None else text[:limit]


def front_matter(text):
    out = {}
    if text.startswith("---"):
        end = text.find("\n---", 3)
        for line in text[3:end].splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                out[key.strip()] = value.strip().strip('"')
    return out


def load_skills():
    skills = {}
    for root in SKILL_DIRS:
        if not root.is_dir():
            continue
        for skill_file in sorted(root.glob("*/SKILL.md")):
            name = skill_file.parent.name
            if name not in skills:
                meta = front_matter(read_text(skill_file, 4000))
                skills[name] = {"path": skill_file, "description": meta.get("description", "")}
    return skills


SKILLS = load_skills()


def system_prompt():
    rules = read_text(PROJECT / "AGENTS.md")
    memory_index = read_text(MEMORY_DIR / "MEMORY.md")
    skill_lines = "\n".join("- %s: %s" % (n, s["description"][:200]) for n, s in SKILLS.items())
    return (
        "You are an agent working on the Unreal Engine 5.8 game project CHALK at C:/UnrealEngine/Games/AZ (Windows 11). "
        "You act ONLY through your tools; never claim you did something you did not do with a tool, and quote real "
        "tool output as evidence.\n"
        "Tools: filesystem__* read/write/search files inside C:/UnrealEngine/Games/AZ. unrealclaude__* control the "
        "running Unreal Editor (unrealclaude__unreal_execute_script runs Python inside the editor and answers "
        "'queued' - that is not the result; read the result afterwards, e.g. from the output log or a report file). "
        "unreal-mcp__*, rider__*, gimp-mcp__* are the Unreal toolsets, the Rider IDE and GIMP. run_command runs "
        "PowerShell (the user approves each command). list_skills/read_skill load project skills: before a task a "
        "skill covers, read that skill first and follow it. read_memory reads one memory file named in the index.\n"
        "Follow instructions exactly; if a step fails, stop and report the exact error. Answer the user in Russian.\n\n"
        "=== PROJECT RULES (AGENTS.md) ===\n%s\n\n"
        "=== SKILLS (read_skill <name> to load one) ===\n%s\n\n"
        "=== MEMORY INDEX (read_memory <file.md> for details; entries may be outdated - verify before relying) ===\n%s"
        % (rules, skill_lines, memory_index)
    )


# ------------------------------------------------------------------ built-in tools
BUILTIN_TOOLS = [
    {"type": "function", "function": {
        "name": "run_command",
        "description": "Run a PowerShell command on this Windows machine and return stdout, stderr and the exit code. "
                       "The user approves every command. Working directory defaults to the project root.",
        "parameters": {"type": "object", "properties": {
            "command": {"type": "string", "description": "PowerShell command line"},
            "cwd": {"type": "string", "description": "optional working directory"}},
            "required": ["command"]}}},
    {"type": "function", "function": {
        "name": "list_skills",
        "description": "List the project skills (name and when to use it).",
        "parameters": {"type": "object", "properties": {}}}},
    {"type": "function", "function": {
        "name": "read_skill",
        "description": "Load the full instructions of one project skill by name.",
        "parameters": {"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}}},
    {"type": "function", "function": {
        "name": "read_memory",
        "description": "Read one file from the project memory directory, e.g. 'project_weapon_models_import_2026-09-25.md'.",
        "parameters": {"type": "object", "properties": {"file": {"type": "string"}}, "required": ["file"]}}},
]


def guard_refusal(command):
    """Ask the project's own git guard hook; returns its refusal text or None."""
    if not GIT_GUARD.is_file():
        return None
    payload = json.dumps({"tool_input": {"command": command}})
    try:
        out = subprocess.run([sys.executable, str(GIT_GUARD)], input=payload, capture_output=True, text=True,
                             timeout=30).stdout
    except Exception:
        return None
    if '"deny"' in out:
        try:
            return json.loads(out)["hookSpecificOutput"]["permissionDecisionReason"]
        except (ValueError, KeyError):
            return "BLOCKED by the project git guard."
    return None


def run_command(args, auto):
    command = (args.get("command") or "").strip()
    cwd = args.get("cwd") or str(PROJECT)
    if not command:
        return "ERROR: empty command"
    refusal = guard_refusal(command)
    if refusal:
        return "REFUSED: " + refusal
    if not auto and not INTERACTIVE:
        return "REFUSED: the shell is off in one-shot mode (start with --auto to allow it)."
    if not auto:
        print("\n  [?] The model wants to run (in %s):\n      %s" % (cwd, command))
        try:
            answer = input("      Run it? [y/N] ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            answer = ""
        if answer not in ("y", "yes", "д", "да"):
            return "REFUSED by the user: the command was not run."
    try:
        done = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", command], cwd=cwd,
                              capture_output=True, text=True, encoding="utf-8", errors="replace",
                              timeout=COMMAND_TIMEOUT)
    except subprocess.TimeoutExpired:
        return "ERROR: timed out after %d s" % COMMAND_TIMEOUT
    except Exception as e:
        return "ERROR: %s" % e
    return "exit code %d\n--- stdout\n%s\n--- stderr\n%s" % (done.returncode, done.stdout, done.stderr)


def run_builtin(name, args, auto):
    if name == "run_command":
        return run_command(args, auto)
    if name == "list_skills":
        return "\n".join("%s: %s" % (n, s["description"]) for n, s in SKILLS.items()) or "(no skills)"
    if name == "read_skill":
        skill = SKILLS.get((args.get("name") or "").strip())
        if not skill:
            return "ERROR: no skill named %r. Known: %s" % (args.get("name"), ", ".join(SKILLS))
        return read_text(skill["path"])
    if name == "read_memory":
        file_name = Path(args.get("file") or "").name          # only files inside the memory dir
        return read_text(MEMORY_DIR / file_name) or "ERROR: memory file %r not found" % file_name
    return "ERROR: unknown built-in tool %s" % name


# ------------------------------------------------------------------ MCP side
def tool_key(server, tool):
    """OpenAI function names allow [a-zA-Z0-9_-], max 64 chars."""
    return re.sub(r"[^a-zA-Z0-9_-]", "_", "%s__%s" % (server, tool))[:64]


def fix_schema(node):
    """Give every array parameter an 'items' entry. Some MCP tools omit it, and strict chat templates (gpt-oss:
    `if param_spec['items']` then hits the dict method items() - "Function is not a bool value", HTTP 500) refuse
    the whole request because of one tool."""
    if isinstance(node, dict):
        if node.get("type") == "array" and "items" not in node:
            node["items"] = {"type": "string"}
        for value in node.values():
            fix_schema(value)
    elif isinstance(node, list):
        for value in node:
            fix_schema(value)
    return node


async def connect(stack, cfg, errlog):
    if "url" in cfg:
        read, write, _ = await stack.enter_async_context(streamablehttp_client(cfg["url"]))
    else:
        env = dict(os.environ)
        env.update(cfg.get("env", {}))
        params = StdioServerParameters(command=cfg["command"], args=cfg.get("args", []), env=env)
        read, write = await stack.enter_async_context(stdio_client(params, errlog=errlog))
    session = await stack.enter_async_context(ClientSession(read, write))
    await session.initialize()
    return session


async def open_servers(stack, wanted):
    errlog = stack.enter_context(open(SERVER_LOG, "a", encoding="utf-8"))   # server chatter goes here, not the chat
    sessions, tools, routes = {}, [], {}
    for name, cfg in SERVERS.items():
        if wanted and name not in wanted:
            continue
        try:
            session = await asyncio.wait_for(connect(stack, cfg, errlog), timeout=30)
            listed = (await session.list_tools()).tools
        except Exception as e:   # a server that is not running must not kill the agent
            print("  [!] %s: not connected (%s)" % (name, str(e) or type(e).__name__))
            continue
        sessions[name] = session
        for t in listed:
            key = tool_key(name, t.name)
            routes[key] = (name, t.name)
            tools.append({"type": "function", "function": {
                "name": key,
                "description": (t.description or "")[:1024],
                "parameters": fix_schema(t.inputSchema or {"type": "object", "properties": {}}),
            }})
        print("  [+] %s: %d tools" % (name, len(listed)))
    return sessions, tools, routes


async def run_mcp(sessions, routes, key, args):
    server, tool = routes[key]
    try:
        result = await sessions[server].call_tool(tool, args)
    except Exception as e:
        return "ERROR: %s failed: %s" % (key, e)
    parts = []
    for item in result.content:
        text = getattr(item, "text", None)
        parts.append(text if text is not None else "[%s content]" % getattr(item, "type", "non-text"))
    out = "\n".join(parts) or "(no output)"
    return ("ERROR: " + out) if getattr(result, "isError", False) else out


async def run_tool(sessions, routes, key, raw_args, auto):
    try:
        args = json.loads(raw_args) if raw_args else {}
    except json.JSONDecodeError as e:
        return "ERROR: tool arguments are not valid JSON (%s): %s" % (e, raw_args[:500])
    if key in routes:
        out = await run_mcp(sessions, routes, key, args)
    elif any(t["function"]["name"] == key for t in BUILTIN_TOOLS):
        out = run_builtin(key, args, auto)
    else:
        out = "ERROR: unknown tool %s" % key
    if len(out) > MAX_TOOL_CHARS:
        out = out[:MAX_TOOL_CHARS] + "\n...[cut, %d chars total]" % len(out)
    return out


# ------------------------------------------------------------------ agent loop
def clean(text):
    """Drop lone surrogates (broken console input) that would make the request fail to encode."""
    return (text or "").encode("utf-8", "replace").decode("utf-8")


async def solve(client, model, messages, tools, sessions, routes, auto, trace=None, max_steps=MAX_STEPS):
    trace = trace if trace is not None else []
    for _ in range(max_steps):
        response = await client.chat.completions.create(model=model, messages=messages, tools=tools,
                                                         temperature=TEMPERATURE)
        msg = response.choices[0].message
        calls = msg.tool_calls or []
        entry = {"role": "assistant", "content": msg.content or ""}
        if calls:
            entry["tool_calls"] = [{"id": c.id, "type": "function",
                                    "function": {"name": c.function.name, "arguments": c.function.arguments or "{}"}}
                                   for c in calls]
        messages.append(entry)
        if not calls:
            return msg.content or ""
        if msg.content and msg.content.strip():
            print("\n[model] %s" % msg.content.strip())
            trace.append("**model:** %s" % msg.content.strip())
        for c in calls:
            print("\n  -> %s %s" % (c.function.name, (c.function.arguments or "")[:300]))
            out = clean(await run_tool(sessions, routes, c.function.name, c.function.arguments, auto))
            print("  <- %s" % out[:400].replace("\n", " | "))
            trace.append("**call** `%s` `%s`\n```\n%s\n```" % (c.function.name, c.function.arguments or "{}", out))
            messages.append({"role": "tool", "tool_call_id": c.id, "content": out})
    return "[stopped: %d tool rounds without a final answer]" % max_steps


async def one_shot(client, opts, task_text, messages, tools, sessions, routes):
    """Run one task non-interactively and keep the full trace for review (RUNS_DIR/<time>.md)."""
    global INTERACTIVE
    INTERACTIVE = False
    started = time.perf_counter()
    trace = []
    messages.append({"role": "user", "content": task_text})
    try:
        answer = await solve(client, opts.model, messages, tools, sessions, routes, opts.auto, trace, opts.max_steps)
    except Exception as e:
        answer = "[error] %s" % e
    seconds = time.perf_counter() - started
    print("\nAgent:\n%s\n" % answer)
    RUNS_DIR.mkdir(parents=True, exist_ok=True)
    path = RUNS_DIR / ("%s.md" % datetime.now().strftime("%Y%m%d-%H%M%S"))
    calls = sum(t.startswith("**call**") for t in trace)
    path.write_text("# Run %s\nmodel: %s | servers: %s | %.0f s | tool calls: %d\n\n## Task\n%s\n\n## Trace\n%s\n\n## Answer\n%s\n"
                    % (path.stem, opts.model, opts.servers, seconds, calls, task_text, "\n\n".join(trace), answer),
                    encoding="utf-8")
    print("Run log: %s (%.0f s, %d tool calls)" % (path, seconds, calls))


async def main():
    for stream in (sys.stdin, sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    parser = argparse.ArgumentParser(description="AZ local agent (LM Studio + MCP + project skills)")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--servers", default=DEFAULT_SERVERS, help="all, or a comma list, e.g. filesystem,unrealclaude,rider")
    parser.add_argument("--auto", action="store_true", help="run shell commands without asking (the git guard still applies)")
    parser.add_argument("--list-tools", action="store_true", help="connect, print the tools and exit")
    parser.add_argument("--task", default="", help="one-shot: run this task, print the answer, exit")
    parser.add_argument("--task-file", default="", help="one-shot: read the task text from this file")
    parser.add_argument("--max-steps", type=int, default=MAX_STEPS)
    opts = parser.parse_args()
    wanted = set() if opts.servers.strip().lower() == "all" else {s.strip() for s in opts.servers.split(",") if s.strip()}

    async with AsyncExitStack() as stack:
        print("Connecting MCP servers (their logs: %s)..." % SERVER_LOG)
        sessions, mcp_tools, routes = await open_servers(stack, wanted)
        tools = mcp_tools + BUILTIN_TOOLS
        print("  [+] built-in: %d tools (run_command, skills: %d, memory)" % (len(BUILTIN_TOOLS), len(SKILLS)))
        if opts.list_tools:
            for t in tools:
                print("  %s - %s" % (t["function"]["name"], t["function"]["description"][:90].replace("\n", " ")))
            return

        key = os.environ.get("LM_STUDIO_API_KEY")
        if not key:
            print('No token. Run once:  setx LM_STUDIO_API_KEY "<token>"   then open a new terminal.')
            return
        client = AsyncOpenAI(base_url=LM_BASE_URL, api_key=key)
        messages = [{"role": "system", "content": system_prompt()}]
        task_text = opts.task or (read_text(opts.task_file) if opts.task_file else "")
        if task_text:
            await one_shot(client, opts, clean(task_text), messages, tools, sessions, routes)
            return
        print("\nAgent ready: model %s, %d tools%s. Commands: exit, /reset, /tools\n"
              % (opts.model, len(tools), ", shell AUTO" if opts.auto else ""))
        while True:
            try:
                task = clean(input("Task: ")).strip()
            except (EOFError, KeyboardInterrupt):
                break
            if task.lower() in ("exit", "quit"):
                break
            if task == "/reset":
                messages = [{"role": "system", "content": system_prompt()}]
                print("(conversation cleared)\n")
                continue
            if task == "/tools":
                print(", ".join(t["function"]["name"] for t in tools) + "\n")
                continue
            if not task:
                continue
            messages.append({"role": "user", "content": task})
            try:
                answer = await solve(client, opts.model, messages, tools, sessions, routes, opts.auto)
                print("\nAgent:\n%s\n" % answer)
            except Exception as e:
                print("Error: %s\n" % e)


if __name__ == "__main__":
    asyncio.run(main())
