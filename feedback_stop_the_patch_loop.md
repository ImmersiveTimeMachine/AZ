---
name: feedback_stop_the_patch_loop
description: "★★★ USER RULE (2026-08-28): stop iterating blindly. Hard limits on patch-test-patch loops — 2-strike rule, write the prediction BEFORE the fix, one variable per test, always diff against the last known-good log, and escalate the DESIGN instead of shipping fix #3."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-28T04:57:11.039Z
---

# STOP THE PATCH LOOP

**User, 2026-08-28:** *"you go into the loop you don't observe this, create some rules that will not allow
you to go in incorrect loops"* — said after **six** fix→PIE→fail rounds on one feature (CMC idle jump),
during which a previously WORKING land silently regressed and I did not notice.

**Why this matters:** each round costs the user a PIE and a context switch. Six rounds of one-line
guesses is worse than one round of proper diagnosis, and the loop hides regressions because attention is
on the newest symptom only.

## The hard rules

### L1 — TWO-STRIKE RULE ★★★
After **two** failed fixes for the same symptom, **STOP PATCHING**. The third attempt is forbidden until
one of these has happened:
- a full per-frame trace of the actual path (every function that touches the value, in execution order), or
- new instrumentation that prints the DECIDING INPUTS, not the outcome, or
- an explicit statement to the user that the current mechanism may be the wrong one, with the alternative.

Two failures mean the MODEL is wrong, not the value. Another value change cannot fix a wrong model.

### L2 — Write the falsifiable prediction BEFORE the fix
State, in the message that ships the fix: *"the log will show X"*. Then compare.
- Prediction matched, symptom gone → done.
- Prediction matched, symptom remains → the fix was correct AND irrelevant; the cause is elsewhere.
- **Prediction did NOT match → THE MODEL IS WRONG. Go to L1. Do not patch again.**
Without a written prediction there is no way to tell these apart, and every round becomes a guess.

### L3 — ONE variable per test cycle
Never change code AND notifies AND asset data in the same round. When two things change and the result is
still wrong, neither can be attributed and the next round starts from a worse position than the last.

### L4 — DIFF AGAINST THE LAST KNOWN-GOOD ★★★
Before reporting any result, compare the new log to the previous one for things that USED to work.
Regression is invisible when only the target symptom is being watched — this is exactly how a working
land was lost while chasing which land clip was chosen.
Keep an explicit list of what currently works; re-check it every round.

### L5 — Do not request a PIE without a stated expected line
Every "please test" must name the exact log line that decides pass/fail. If that line cannot be named, the
instrumentation is insufficient — add it first. A PIE that cannot produce a verdict is a wasted round.

### L6 — Log the DECIDING INPUTS before the second attempt
"What was chosen" cannot distinguish *wrong candidate won* from *right candidate was never available* —
and those need opposite fixes. Print the candidate set / the gate inputs FIRST, not after the third wrong
fix. (See R7 in [[feedback_posesearch_mm_mechanism_rules]].)

### L7 — Escalate the DESIGN, not the patch count
If the same class of bug keeps recurring, the mechanism is probably wrong for the job. Say so out loud
and offer the alternative instead of shipping another fix.
*The concrete miss:* GASP direct-plays jump takeoffs (`bUseMM=False`) precisely because MM cannot be
trusted to pick a one-shot at an exact moment. I noted that, then kept patching the MM path five more
times instead of raising it as a design decision.

### L8 — Name the loop out loud
When round 3 on one symptom arrives, say so plainly: *"this is the third attempt, I am looping, here is
what I actually do not understand."* Being stuck is information the user needs, and hiding it inside a
confident-sounding fix wastes their time.

## Self-check before shipping any fix

1. What number attempt is this on this symptom? (≥3 → L1 applies)
2. What exact log line will prove it worked?
3. What worked before that this could break? Did I re-check it?
4. Am I changing one thing?
5. Did my last prediction come true? If not, why am I not re-modelling?

Related: [[feedback_verify_never_presume]], [[feedback_posesearch_mm_mechanism_rules]],
[[feedback_seam_trace_before_pie]], [[feedback_aaa_design_first]].
