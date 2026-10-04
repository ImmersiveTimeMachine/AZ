# @Description: Insert AZ Weapon Body Clearance between Local To Component and AZ Weapon Grip in the hero MHC ABP (no compile/save)
"""
Post-build step for the weapon body-clearance node.

Chain before:  ... ControlRig_0 -> LocalToComponent -> AZ Weapon Grip -> ComponentToLocal -> ...
Chain after:   ... ControlRig_0 -> LocalToComponent -> AZ Weapon Body Clearance -> AZ Weapon Grip -> ComponentToLocal -> ...

Pins bound to the anim instance (UAZ_MoverAnimInstance):
    Markers              <- WeaponGripMarkers
    WeaponBoneName       <- WeaponGripBone
    ClearanceAlpha       <- WeaponGripAlpha
    ShoulderContactAlpha <- AimAlpha

Does NOT compile or save the AnimBP (Python GC crash rule) - the user compiles / saves it.
Idempotent: if a body-clearance node already exists it only re-wires and re-binds it.
"""
import unreal

ABP = "/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC"
NODE_CLASS = "/Script/AZEditor.AnimGraphNode_AZWeaponBodyClearance"
L2C_GUID = "D1ECEC0343CD473EB3422FA70D428384"    # Local To Component (output "ComponentPose")
GRIP_GUID = "4B1C87F8456E6EB3D56DFCB3DA89585D"   # AZ Weapon Grip (input "ComponentPose")

U = unreal.AZ_AnimGraphNodeUtils


def find_nodes(fragment):
    out = []
    for line in U.list_anim_graph_nodes(ABP):
        if fragment in line:
            out.append(line.split(" ")[0].replace("GUID=", ""))
    return out


nodes = U.list_anim_graph_nodes(ABP)
assert any(L2C_GUID in n for n in nodes), "Local To Component node not found - graph changed, stop"
assert any(GRIP_GUID in n for n in nodes), "AZ Weapon Grip node not found - graph changed, stop"

existing = find_nodes("AnimGraphNode_AZWeaponBodyClearance")
if existing:
    guid = existing[0]
    unreal.log("[BodyClearance] node already present: %s - re-wiring" % guid)
else:
    guid = U.add_anim_graph_node(ABP, NODE_CLASS, -400, 0)
    unreal.log("[BodyClearance] added node %s" % guid)
assert guid, "AddAnimGraphNode failed (is the AZEditor module rebuilt?)"

# Wire. Pose outputs connect to one input only (schema breaks the old link), so the order below leaves
# L2C -> Clearance -> Grip with nothing dangling.
ok1 = U.connect_pose_link(ABP, L2C_GUID, guid, "ComponentPose")
ok2 = U.connect_pose_link(ABP, guid, GRIP_GUID, "ComponentPose")

b1 = U.set_pin_binding(ABP, guid, "Markers", "WeaponGripMarkers", False)
b2 = U.set_pin_binding(ABP, guid, "WeaponBoneName", "WeaponGripBone", False)
b3 = U.set_pin_binding(ABP, guid, "ClearanceAlpha", "WeaponGripAlpha", False)
b4 = U.set_pin_binding(ABP, guid, "ShoulderContactAlpha", "AimAlpha", False)

unreal.log("[BodyClearance] links L2C->Clearance=%s Clearance->Grip=%s | bindings %s %s %s %s"
           % (ok1, ok2, b1, b2, b3, b4))
for n in U.list_anim_graph_nodes(ABP):
    if "Weapon" in n or L2C_GUID in n:
        unreal.log("[BodyClearance]   " + n)

import gc
gc.collect()
