---
name: reference_zaggoth_lh_ik_tutorial
description: Zaggoth's UE4 tutorial on left-hand weapon IK using FABRIK in bone space relative to hand_r with IK_Hand socket
type: reference
---

**Source:** https://zaggoth.wordpress.com/author/zaggoth/ — "UE4 Tutorial: The Right Way to Do Left-Hand Weapon IK"

## Key Technique
Uses FABRIK node (not Two Bone IK) with bone-space-relative approach to eliminate 1-frame lag between hands.

## Setup Steps

1. **Socket:** Add an `IK_Hand` socket to weapon skeletal mesh root bone (or animated bone for pump shotguns etc.). Position where left hand should grip. All weapons use the same socket name for versatility.

2. **AnimBP Event Graph:**
   - Float variable `Weapon FABRIK Alpha` (default 1.0) to toggle IK on/off
   - Get weapon skeletal mesh socket transform (world space)
   - Convert world-space socket location to **Bone Space** relative to `Hand_R`
   - Store result in `LeftHandIKTransform` variable

3. **AnimBP Anim Graph — FABRIK Node:**
   - Effector Transform: `LeftHandIKTransform`
   - Effector Transform Space: **Bone Space**
   - Effector Target: `Hand_R`
   - Tip Bone: `Hand_L`
   - Root Bone: `UpperArm_L`
   - Alpha: `Weapon FABRIK Alpha`

## Why It Works
Left hand moves explicitly relative to the right hand, so they stay in sync with no lag. Same principle as the Two Bone IK bone-space approach already used in AZ project (see feedback_ik_setup.md).

## AZ Project Application
Current C++ AnimInstance computes LeftHandIKTransform in component space (relative to CharMesh). Should convert to bone space relative to hand_r instead, matching this tutorial. The weapon mesh already has grip sockets (LeftHandGrip / LeftHandGripAim) — these serve the same purpose as Zaggoth's IK_Hand socket.
