"""Author only the CHALK rifle reticle leaf and the existing HUD's centered host.

Run through ProgrammaticToolset.execute_tool_script, which injects execute_tool.
Default MODE is read-only. Before authoring, run hud_reticle_assign.main('backup')
and set BACKUP_READY=True in the submitted source. Native reticle classes must
already be built and loaded. No Blueprint compilation, saves, gameplay tests,
inventory edits, or source-pack edits happen here. Compile the leaf and HUD via
the dedicated BlueprintTools tool after this script returns; assignment of the
definition and rifle manifests is a separate Unreal Python step.
"""
import copy
import json

MODE = "audit"
BACKUP_READY = False
UMG = "UMGToolSet.UMGToolSet."
OBJ = "editor_toolset.toolsets.object.ObjectTools."
FOLDER = "/Game/AZ/Blueprints/Menu/HUD/Reticles"
LEAF_NAME = "WBP_AZ_Reticle_Rifle"
LEAF = FOLDER + "/" + LEAF_NAME + "." + LEAF_NAME
PARENT = "/Script/AZ.AZ_HUDReticleWidget"
HUD = "/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD"
TEXTURES = "/Game/ProHUDV2_Horror/Textures/Crosshair/V1/Crosshair1/"
ARMS = (("Up", (19, 4, 2, 10)), ("Down", (19, 26, 2, 10)),
        ("Left", (4, 19, 10, 2)), ("Right", (26, 19, 10, 2)))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def call(prefix, name, **kwargs):
    result = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(result, dict) and "returnValue" in result,
            "Tool did not return a value: " + name + " " + str(result))
    return result["returnValue"]


def ref(path):
    return {"refPath": path}


def merge(existing, changes):
    result = copy.deepcopy(existing)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def properties(instance, changes=None, names=None):
    schema = json.loads(call(OBJ, "list_properties", instance=instance))
    selected = list(changes) if changes is not None else names
    if selected is None:
        return schema
    require(not (set(selected) - set(schema)), "Unknown properties: " + str(set(selected) - set(schema)))
    before = json.loads(call(OBJ, "get_properties", instance=instance, properties=selected))
    if changes is None:
        return before
    require(call(OBJ, "set_properties", instance=instance, values=json.dumps(merge(before, changes))),
            "Property write failed: " + str(instance))
    return json.loads(call(OBJ, "get_properties", instance=instance, properties=selected))


def tree(path):
    return call(UMG, "GetWidgets", widgetBlueprint=ref(path))


def canvas_slot(x, y, width, height, centered=False, z=0, auto=False):
    anchor = .5 if centered else 0
    return {"layoutData": {"offsets": {"left": x, "top": y, "right": width, "bottom": height},
            "anchors": {"minimum": {"x": anchor, "y": anchor}, "maximum": {"x": anchor, "y": anchor}},
            "alignment": {"x": anchor, "y": anchor}}, "bAutoSize": auto, "zOrder": z}


def add(path, name, widget_class, parent=None, props=None, slot=None, variable=False):
    args = {"widgetBlueprint": ref(path), "widgetClass": ref(widget_class), "widgetDisplayName": name}
    if parent:
        args["parentWidget"] = parent
    info = call(UMG, "AddWidget", **args)
    require(info and isinstance(info.get("widget"), dict), "Cannot add " + name)
    if info["widgetName"] != name:
        info = call(UMG, "RenameWidget", widgetBlueprint=ref(path), widget=info["widget"], newDisplayName=name)
    properties(info["widget"], props or {"visibility": "HitTestInvisible"})
    if isinstance(info.get("slot"), dict):
        properties(info["slot"], slot) if slot else properties(info["slot"])
    call(UMG, "ToggleWidgetAsVariable", widgetBlueprint=ref(path), widget=info["widget"], bIsVariable=variable)
    return info["widget"]


def rgba(value, alpha=1):
    return {"r": value, "g": value, "b": value, "a": alpha}


def image(name, parent, box, texture, shadow=False):
    # Native presentation applies the definition tint once to the whole leaf.
    # White source arms retain that tint; black underlays retain black.
    tint = rgba(0, .65) if shadow else rgba(1)
    brush = {"drawAs": "Image", "resourceObject": ref(texture),
             "imageSize": {"x": box[2], "y": box[3]},
             "tintColor": {"specifiedColor": rgba(1), "colorUseRule": "UseColor_Specified"}}
    return add(LEAF, name, "/Script/UMG.Image", parent,
               {"brush": brush, "colorAndOpacity": tint, "visibility": "HitTestInvisible"},
               canvas_slot(*box, z=0 if shadow else 1))


def author_leaf():
    created = call(UMG, "CreateWidgetBlueprint", folderPath=FOLDER, assetName=LEAF_NAME, parentClass=ref(PARENT))
    current = tree(LEAF)
    require(current["info"]["parentClass"] == ref(PARENT), "Reticle leaf has an unexpected parent")
    require(current["info"]["widgetCount"] == 0,
            "Reticle leaf already has widgets; inspect/resume explicitly, never replace its tree")
    # Native HUD inserts a runtime ScaleBox around this 40x40 design so a
    # future Definition.Size scales the geometry without changing its center.
    design = add(LEAF, "ReticleDesignSize", "/Script/UMG.SizeBox",
                 props={"visibility": "HitTestInvisible", "bOverride_WidthOverride": True, "widthOverride": 40,
                        "bOverride_HeightOverride": True, "heightOverride": 40})
    canvas = add(LEAF, "ReticleCanvas", "/Script/UMG.CanvasPanel", design,
                 {"visibility": "HitTestInvisible"},
                 {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill"})
    for direction, box in ARMS:
        texture_name = "T_CH1_" + direction + "_H"
        texture = TEXTURES + texture_name + "." + texture_name
        x, y, w, h = box
        image("Outline" + direction, canvas, (x - 1, y - 1, w + 2, h + 2), texture, True)
        image("Arm" + direction, canvas, box, texture)
    return {"created": created, "leaf": tree(LEAF), "native_parent": PARENT}


def author_host():
    before = tree(HUD)
    # GetWidgets also lists unbound inherited BindWidget properties with
    # widget=None. A reflected declaration is not an authored tree element.
    real_before = [w for w in before["widgets"] if isinstance(w.get("widget"), dict)]
    widgets = {w["widgetName"]: w for w in real_before}
    require("HUDRoot" in widgets and "HitMarker" in widgets, "Expected current Phase 1 HUD structure")
    require("ReticleHost" not in widgets, "ReticleHost already exists; verify or resume explicitly")
    require(widgets["HitMarker"]["parent"] == widgets["HUDRoot"]["widget"],
            "HitMarker is no longer a direct HUDRoot child")
    hit_before = properties(widgets["HitMarker"]["slot"], names=["layoutData", "bAutoSize", "zOrder"])
    add(HUD, "ReticleHost", "/Script/UMG.SizeBox", widgets["HUDRoot"]["widget"],
        {"visibility": "Collapsed", "bOverride_WidthOverride": True, "widthOverride": 40,
         "bOverride_HeightOverride": True, "heightOverride": 40},
        canvas_slot(0, 0, 40, 40, centered=True, z=10, auto=True), True)
    properties(widgets["HitMarker"]["slot"], {"zOrder": 20})
    after = tree(HUD)
    old_after = [w for w in after["widgets"] if w["widgetName"] != "ReticleHost" and isinstance(w.get("widget"), dict)]
    require(old_after == real_before, "An existing HUD widget's structure changed")
    hit_after = properties(widgets["HitMarker"]["slot"], names=["layoutData", "bAutoSize", "zOrder"])
    require(hit_after == merge(hit_before, {"zOrder": 20}), "HitMarker geometry changed")
    return {"hud": after, "hit_marker_slot_before": hit_before, "hit_marker_slot_after": hit_after,
            "scope": "Only new ReticleHost and HitMarker slot zOrder; no core/inventory changes"}


def verify():
    hud, leaf = tree(HUD), tree(LEAF)
    widgets = {w["widgetName"]: w for w in hud["widgets"]}
    require(leaf["info"]["parentClass"] == ref(PARENT), "Wrong reticle leaf parent")
    require(set(w["widgetName"] for w in leaf["widgets"]) ==
            {"ReticleDesignSize", "ReticleCanvas"} |
            {prefix + direction for direction, _ in ARMS for prefix in ("Arm", "Outline")},
            "Reticle leaf structure differs from the authored rifle design")
    require(widgets["ReticleHost"]["parent"] == widgets["HUDRoot"]["widget"], "Reticle host is not at viewport root")
    host_slot = properties(widgets["ReticleHost"]["slot"], names=["layoutData", "bAutoSize", "zOrder"])
    require(host_slot == canvas_slot(0, 0, 40, 40, centered=True, z=10, auto=True), "Reticle host is not centered/autosized")
    hit_slot = properties(widgets["HitMarker"]["slot"], names=["zOrder"])
    require(hit_slot["zOrder"] == 20, "Hit confirmation must draw above the reticle")
    return {"hud": hud, "leaf": leaf, "host_slot": host_slot, "hit_marker_slot": hit_slot}


def run():
    if MODE == "audit":
        return {"hud": tree(HUD), "planned_leaf": LEAF, "planned_parent": PARENT,
                "next": "Backup via hud_reticle_assign, load native classes, then leaf/host authoring"}
    if MODE == "verify":
        return verify()
    require(BACKUP_READY, "Run hud_reticle_assign.main('backup') before setting BACKUP_READY=True")
    if MODE == "leaf":
        return author_leaf()
    if MODE == "host":
        return author_host()
    if MODE == "author":
        return {"leaf": author_leaf(), "host": author_host(),
                "compile_after_return": [LEAF, HUD], "save": "Separate dedicated tools after compile and assignment"}
    raise RuntimeError("Unknown MODE: " + MODE)
