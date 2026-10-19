---
name: feedback-creativity-deep-understanding
description: "Artur (2026-10-18): maximum creativity and deep understanding of what happens on screen, not literal task execution; judge every feature through the player's eyes and add the missing reactions/feedback. Applies to all work and all agents."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 0cf39d47-f3b0-4a50-9b24-845e8ec0506f
  modified: 2026-10-18T06:01:39.675Z
---

**Rule (Artur, 2026-10-18 ~01:58, for every agent and every task):**
- Work with maximum creativity and a deep understanding of the process, not literal execution of the task.
- Judge every feature by what a player would see and feel at that moment. If it would look unnatural, the work is not done, even when the spec and the log are satisfied.
- Add the missing reactions and feedback a real game would have instead of stopping at the requested mechanics.

**Why:** in an NPC test run a Chalkie bit the player, and the first-person player just stood with the rifle staring at it — no flinch, no step back, no reaction. Artur called that "сидишь как чурбан".

**How to apply:**
- For any interaction, list what the player should feel. For a hit: flinch of arms and weapon, camera kick or push-back, aim disturbance, sound, HUD direction cue, a struggle on grab.
- Then check which existing assets cover it (GameSDK, Crysis, Unreal CHALK clips) and implement the best combination.
- Coordinate cross-owner changes through agents-chat.md.
- Report what still looks wrong, not only what passed.
- Playtest yourself when it matters (Artur 02:0x, optional, not urgent). Take your own Rider Debug run under the lock, with scripted or real input, and capture screenshots in sequence. Judge how the game feels and let that drive fixes and improvements.

Related: [[feedback_aaa_design_first]], [[feedback_deliver_result_not_analysis]], [[feedback-cryengine-agents-lock-planning]].
