"""Benchmark local LM Studio models on the SAME light agent tasks (speed + correctness).

Uses agent_script.py's tools, system prompt and MCP servers, so the numbers describe the real agent. For each model:
unload everything, load it (lms, fixed context), run every task in a fresh conversation, check the answer against a
known value, unload. Shell (run_command) is disabled here - no model runs anything on the machine during a benchmark.

Run (PowerShell, token set, Unreal Editor open for the Unreal tasks):
    python C:/UnrealEngine/Games/AZ/.agents/bench_models.py --models openai/gpt-oss-20b,meta/muse-glimmer
Options: --ctx 32768   --timeout 600 (seconds per task)   --tasks 1,3   --restore prism-ml/bonsai-27b
Results: printed + appended to .agents/bench_results.md
"""
import argparse
import asyncio
import importlib.util
import os
import re
import subprocess
import sys
import time
from contextlib import AsyncExitStack
from datetime import datetime
from pathlib import Path

from openai import AsyncOpenAI

HERE = Path(__file__).resolve().parent
LMS = Path.home() / ".lmstudio" / "bin" / "lms.exe"
RESULTS = HERE / "bench_results.md"

_spec = importlib.util.spec_from_file_location("agent_script", HERE / "agent_script.py")
A = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(A)
A.run_command = lambda args, auto: "REFUSED: the shell is disabled during the benchmark."

# (short name, prompt, regexes that must ALL match the final answer)
TASKS = [
    ("skills", "How many project skills are there? Call list_skills and answer with just the number.",
     [r"\b9\b"]),
    ("file", "Read the file C:/UnrealEngine/Games/AZ/.agents/agent_script.py and tell me the value of DEFAULT_SERVERS.",
     [r"filesystem\s*,\s*unrealclaude"]),
    ("unreal", "Is the Unreal Editor connected? Check with the Unreal status tool and answer with the engine version.",
     [r"5\.8"]),
    ("memory", "Read the memory file project_weapon_models_import_2026-09-25.md and give me the location (three numbers) "
               "of RightHandShotgunSocket.",
     [r"[-\u2212]\s?28\.74", r"6\.71", r"4\.93"]),
    ("assets", "How many StaticMesh assets are in the Unreal folder /Game/AZ/Temp/WholeTest? Use the Unreal asset search "
               "tool and answer with the number.",
     [r"\b7\b"]),
]


def lms(*args, timeout=900):
    return subprocess.run([str(LMS), *args], capture_output=True, text=True, encoding="utf-8", errors="replace",
                          timeout=timeout)


async def run_task(client, model, tools, sessions, routes, prompt):
    messages = [{"role": "system", "content": A.system_prompt()}, {"role": "user", "content": prompt}]
    calls, prompt_tokens, completion_tokens = [], 0, 0
    for _ in range(A.MAX_STEPS):
        response = await client.chat.completions.create(model=model, messages=messages, tools=tools,
                                                         temperature=A.TEMPERATURE)
        if response.usage:
            prompt_tokens += response.usage.prompt_tokens or 0
            completion_tokens += response.usage.completion_tokens or 0
        msg = response.choices[0].message
        tool_calls = msg.tool_calls or []
        entry = {"role": "assistant", "content": msg.content or ""}
        if tool_calls:
            entry["tool_calls"] = [{"id": c.id, "type": "function",
                                    "function": {"name": c.function.name, "arguments": c.function.arguments or "{}"}}
                                   for c in tool_calls]
        messages.append(entry)
        if not tool_calls:
            return msg.content or "", calls, prompt_tokens, completion_tokens
        for c in tool_calls:
            calls.append(c.function.name)
            out = A.clean(await A.run_tool(sessions, routes, c.function.name, c.function.arguments, False))
            messages.append({"role": "tool", "tool_call_id": c.id, "content": out})
    return "[stopped: too many tool rounds]", calls, prompt_tokens, completion_tokens


async def main():
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    parser = argparse.ArgumentParser()
    parser.add_argument("--models", required=True, help="comma list of LM Studio model keys")
    parser.add_argument("--ctx", type=int, default=32768)
    parser.add_argument("--timeout", type=int, default=600, help="seconds per task")
    parser.add_argument("--tasks", default="", help="comma list of task numbers (1-based), default all")
    parser.add_argument("--servers", default=A.DEFAULT_SERVERS)
    parser.add_argument("--restore", default="", help="model key to load again at the end")
    opts = parser.parse_args()
    key = os.environ.get("LM_STUDIO_API_KEY")
    if not key:
        print('No token. Run once:  setx LM_STUDIO_API_KEY "<token>"   then open a new terminal.')
        return
    picked = [TASKS[int(i) - 1] for i in opts.tasks.split(",") if i.strip()] if opts.tasks else TASKS
    models = [m.strip() for m in opts.models.split(",") if m.strip()]
    client = AsyncOpenAI(base_url=A.LM_BASE_URL, api_key=key)
    rows = []

    async with AsyncExitStack() as stack:
        wanted = set() if opts.servers.lower() == "all" else {s.strip() for s in opts.servers.split(",") if s.strip()}
        sessions, mcp_tools, routes = await A.open_servers(stack, wanted)
        tools = mcp_tools + A.BUILTIN_TOOLS
        print("tools: %d | tasks: %d | context: %d\n" % (len(tools), len(picked), opts.ctx))
        for model in models:
            lms("unload", "--all")
            t0 = time.perf_counter()
            loaded = lms("load", model, "-c", str(opts.ctx), "--parallel", "1", "-y")   # 4 slots x 32k KV spilled gpt-oss to CPU (6 tok/s)
            load_s = time.perf_counter() - t0
            if loaded.returncode != 0:
                print("[%s] LOAD FAILED: %s" % (model, (loaded.stderr or loaded.stdout).strip()[-300:]))
                rows.append((model, "load failed", "", "", "", "", ""))
                continue
            print("[%s] loaded in %.0f s" % (model, load_s))
            for name, prompt, checks in picked:
                t0 = time.perf_counter()
                try:
                    answer, calls, ptok, ctok = await asyncio.wait_for(
                        run_task(client, model, tools, sessions, routes, prompt), timeout=opts.timeout)
                    ok = all(re.search(c, answer, re.I) for c in checks)
                except asyncio.TimeoutError:
                    answer, calls, ptok, ctok, ok = "[timeout]", [], 0, 0, False
                except Exception as e:
                    answer, calls, ptok, ctok, ok = "[error] %s" % e, [], 0, 0, False
                secs = time.perf_counter() - t0
                short = " ".join(answer.split())[:90]
                print("   %-7s %-4s %6.1f s  tools %-38s | %s" % (name, "OK" if ok else "FAIL", secs,
                                                                  ",".join(c.split("__")[-1] for c in calls)[:38], short))
                rows.append((model, name, "OK" if ok else "FAIL", "%.1f" % secs, str(len(calls)),
                             "%d/%d" % (ptok, ctok), short.replace("|", "/")))
            lms("unload", "--all")
        if opts.restore:
            lms("load", opts.restore, "-y")

    stamp = datetime.now().strftime("%Y-%m-%d %H:%M")
    lines = ["", "## %s  (context %d, servers %s, %d tools)" % (stamp, opts.ctx, opts.servers, len(tools)), "",
             "| model | task | result | seconds | tool calls | tokens in/out | answer |",
             "|---|---|---|---|---|---|---|"]
    lines += ["| %s |" % " | ".join(r) for r in rows]
    summary = {}
    for r in rows:
        if r[1] in ("load failed",):
            continue
        s = summary.setdefault(r[0], [0, 0, 0.0])
        s[0] += r[2] == "OK"
        s[1] += 1
        s[2] += float(r[3] or 0)
    lines += ["", "| model | correct | total seconds |", "|---|---|---|"]
    lines += ["| %s | %d/%d | %.0f |" % (m, s[0], s[1], s[2]) for m, s in summary.items()]
    with open(RESULTS, "a", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines[-(len(summary) + 3):]))
    print("\nFull table appended to %s" % RESULTS)


if __name__ == "__main__":
    asyncio.run(main())
