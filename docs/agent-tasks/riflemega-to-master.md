# Task: copy the RifleMega animations onto the master skeleton (chunks 0-22)

Executor: a local model. Follow these steps exactly, in order. Do not improvise, do not "improve" anything,
do not skip steps. When something does not match what this document says, STOP and report (section 6).

## 1. Goal

446 rifle animations must be copied onto the project's master skeleton, each with one extra track (the gun path).
The work is done by a script that is ALREADY installed: `C:/UnrealEngine/Games/AZ/Tools/az_master_riflemega.py`.
The clips are pre-split into 23 chunks (0..22). You run chunks 0 to 22, one at a time, and check the result line
of each. Chunks 0-21 have 20 clips each, chunk 22 has 6.
Before the first chunk, call the Unreal status tool once. If it says the editor is not connected, STOP and report -
do not retry it.

## 2. Rules - never break these

1. Do NOT create, edit, move or delete any file. Do NOT touch the script. You only run it and read its report.
2. Do NOT run any other Python code than the code in section 4, and do NOT change anything in it except the chunk
   number.
3. Do NOT save the level. If the editor asks anything, stop and report.
4. Run ONE command per tool call and wait for its result line before the next call.
5. Never report success without the result lines that prove it. Quote the lines exactly.

## 3. Environment

- Unreal Editor 5.8 is already running with the project `C:/UnrealEngine/Games/AZ` open. Do not close or restart it.
- Python runs INSIDE the editor through the MCP server `unrealclaude`, tool `unreal_execute_script`, parameters:
  `script_type` = `"python"`, `script_content` = the code.
- The tool answers immediately with `Script execution queued. Task ID: ...`. That is NOT the result.
  The result is in the report file `C:/UnrealEngine/Games/AZ/Saved/az_master_riflemega_report.txt`
  (read the whole file with the filesystem tool). The script rewrites this file on every run: its first line is
  `MODE convert` and, when the chunk is finished, its LAST line is `CONVERT chunk N: X/Y OK` where N is the chunk
  you ran.
- A chunk takes up to about 40 seconds. After starting a chunk, read the report file. If its last line is not yet
  `CONVERT chunk N: ...` with YOUR chunk number N, read it again. Up to 10 reads per chunk.

## 4. The command (one per chunk)

For N = 0, 1, 2, ... 22 in this order, call `unreal_execute_script` with `script_type` = `"python"` and
`script_content` = exactly these four lines, with N replaced by the chunk number:

```
# @Description: RifleMega to master skeleton - convert chunk N
MODE = "convert"
CHUNK = N
exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_master_riflemega.py", encoding="utf-8").read())
```

## 5. The check after each chunk

Read the report file (section 3). The chunk is GOOD only if ALL of these hold:
- the last line is `CONVERT chunk N: 20/20 OK` (for chunk 22: `CONVERT chunk 22: 6/6 OK`), with your N;
- no line of the file starts with `FAIL` or `SKIP`.

GOOD -> go to the next chunk. Anything else (a different line, a FAIL or SKIP line, X different from Y, no
`CONVERT chunk N` line after 10 reads, an error message from the tool) -> STOP immediately and report (section 6).

## 6. Final report

Answer with exactly:
1. `DONE` if all 23 chunks were GOOD, otherwise `STOPPED AT CHUNK N`.
2. The `CONVERT chunk N: ...` line of every chunk you ran, copied exactly, one per line.
3. If you stopped: the full content of the report file at that moment, copied exactly, and what the tool answered.
Nothing else. Do not summarize, do not guess, do not explain.
