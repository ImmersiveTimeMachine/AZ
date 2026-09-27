
## 2026-09-25 18:28  (context 32768, servers filesystem,unrealclaude, 34 tools)

| model | task | result | seconds | tool calls | tokens in/out | answer |
|---|---|---|---|---|---|---|
| openai/gpt-oss-20b | skills | FAIL | 0.3 | 0 | 0/0 | [error] Error code: 400 - {'error': 'Engine protocol predict request returned 500: {"error |
| openai/gpt-oss-20b | file | FAIL | 0.0 | 0 | 0/0 | [error] Error code: 400 - {'error': 'Engine protocol predict request returned 500: {"error |
| openai/gpt-oss-20b | unreal | FAIL | 0.0 | 0 | 0/0 | [error] Error code: 400 - {'error': 'Engine protocol predict request returned 500: {"error |
| openai/gpt-oss-20b | memory | FAIL | 0.0 | 0 | 0/0 | [error] Error code: 400 - {'error': 'Engine protocol predict request returned 500: {"error |
| openai/gpt-oss-20b | assets | FAIL | 0.0 | 0 | 0/0 | [error] Error code: 400 - {'error': 'Engine protocol predict request returned 500: {"error |

| model | correct | total seconds |
|---|---|---|
| openai/gpt-oss-20b | 0/5 | 0 |

## 2026-09-25 18:57  (context 32768, servers filesystem,unrealclaude, 19 tools)

| model | task | result | seconds | tool calls | tokens in/out | answer |
|---|---|---|---|---|---|---|
| meta/muse-glimmer | skills | OK | 42.1 | 1 | 17465/184 | to=self</message/>How many project skills are there? Call list_skills and answer with just |
| meta/muse-glimmer | file | OK | 51.0 | 1 | 19402/289 | to=self</message/>Read the file C:/UnrealEngine/Games/AZ/.agents/agent_script.py and tell  |
| meta/muse-glimmer | memory | OK | 70.5 | 1 | 17815/444 | to=self</message/>Read the memory file project_weapon_models_import_2026-09-25.md and give |
| prism-ml/bonsai-27b | skills | OK | 6.8 | 1 | 17278/158 | <think>The user asked me to call `list_skills` and answer with just the number. I can see  |
| prism-ml/bonsai-27b | file | OK | 4.6 | 1 | 19430/223 | <think>The file has been read successfully. I can see the value of `DEFAULT_SERVERS` in th |
| prism-ml/bonsai-27b | memory | OK | 5.5 | 1 | 17815/367 | <think>The user asked for the location (three numbers) of RightHandShotgunSocket. Looking  |
| qwen/qwen3.5-9b | skills | FAIL | 1.8 | 0 | 8041/2 | 3 |
| qwen/qwen3.5-9b | file | OK | 2.2 | 1 | 19378/138 | В файле `C:/UnrealEngine/Games/AZ/.agents/agent_script.py` значение переменной `DEFAULT_SE |
| qwen/qwen3.5-9b | memory | OK | 2.3 | 1 | 17758/169 | Согласно файлу памяти `project_weapon_models_import_2026-09-25.md`, координаты сокета `Rig |

| model | correct | total seconds |
|---|---|---|
| meta/muse-glimmer | 3/3 | 164 |
| prism-ml/bonsai-27b | 3/3 | 17 |
| qwen/qwen3.5-9b | 2/3 | 6 |
