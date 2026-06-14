---
name: UE AI/MCP Plugins Comparison
description: Capabilities of SpecialAgentPlugin, UnrealGenAISupport, and unrealclaude MCP — what each can do and when to use which
type: reference
---

## Available UE AI/MCP Tools

### 1. unrealclaude (current setup — .mcp.json)
**Strength: Blueprint & animation editing at deep node level**
- Actor spawning/transform/delete, level actors query
- Blueprint editing: create, add variables/functions, add/delete/connect nodes, set pin values, search, inspect
- Animation BP: state machines, states, transitions, blend spaces, layers, notifies, compile
- Material: create instances, set parameters, assign to actors/meshes
- Enhanced Input: create actions/contexts, add mappings, triggers, modifiers
- Character: create BPs, movement params, data assets
- Asset management: create, duplicate, rename, delete, move, reimport
- Viewport capture (screenshot)
- Python script execution via Content/UnrealClaude/Scripts/
- Output log reading

### 2. SpecialAgent Plugin (free, GitHub: ArtisanGameworks/SpecialAgentPlugin)
**Strength: Level design, environment art, landscape, full Python API**
- 71+ tools across 14 service categories
- World (30+ tools): spawn, transform, duplicate, delete, batch ops, grid/circular/spline/scatter placement
- Landscape (5): sculpt height, flatten, smooth, paint material layers
- Foliage (3): vegetation painting with density control
- Lighting (4): spawn & configure lights, build lightmaps
- Assets (4): content browser search & inspection
- Navigation (2): NavMesh building, pathfinding tests
- Viewport (4): camera control, actor focus, screenshots
- Streaming (4): sub-level loading & visibility
- Performance (3): stats analysis, overlap detection
- Gameplay (2): trigger volumes, player starts
- Python (3): **full unrestricted UE5 Python API execution** (the real power)
- Utility (5): save, undo, redo, selection tools
- UE 5.7.1+, blueprint projects

### 3. UnrealGenAISupport (free, GitHub: prajwalshettydev/UnrealGenAISupport)
**Strength: Runtime LLM calls from C++/Blueprints (AI NPCs, dialogue, dynamic content)**

#### Runtime LLM APIs (C++ & Blueprints):
- OpenAI: GPT-5, GPT-4o, O3/O4-mini, DALL-E, TTS, Vision
- Claude: Opus 4.5, Sonnet 4.5, Haiku — tool use, extended thinking, vision
- DeepSeek: V3.1, R1 reasoning
- Grok: Grok-3 series, reasoning, multimodal
- Gemini: 3.1, 2.5, Imagen, realtime audio, TTS
- ElevenLabs/Inworld: TTS, transcription
- Local: Ollama, OpenRouter, Groq, Qwen, Mistral

#### MCP Editor Control:
- Spawn objects & shapes, transform, materials/colors
- Create blueprints, add functions & variables (basic)
- Python & console command execution
- File/folder operations

#### Pro Version:
- Text/image-to-3D (Meshy AI, TripoSR, Hunyuan3D, Rodin)
- Widget/UI generation (planned)

## When to Use Which

| Need | Best Tool |
|------|-----------|
| Blueprint node-level editing | unrealclaude |
| Animation BP (states, transitions, blend) | unrealclaude |
| Material creation & assignment | unrealclaude |
| Landscape sculpting & foliage | SpecialAgent |
| Batch actor placement patterns | SpecialAgent |
| NavMesh & pathfinding | SpecialAgent |
| Lighting setup & lightmap baking | SpecialAgent |
| Full Python UE5 API access | SpecialAgent |
| Runtime LLM calls in gameplay | UnrealGenAISupport |
| AI NPCs / dialogue / dynamic content | UnrealGenAISupport |
| TTS / voice / audio AI | UnrealGenAISupport |
| 3D model generation from text | UnrealGenAISupport (Pro) |
