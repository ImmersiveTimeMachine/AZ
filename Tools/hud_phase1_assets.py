"""Author CHALK Phase 1 UMG assets using the editor tool registry.

Feed this source to ProgrammaticToolset.execute_tool_script AFTER the native
HUD/vitals classes have been rebuilt and loaded. The registry injects
execute_tool. This file does not run from ordinary Unreal Python.

run() authors the new HUD, existing inventory health bridge, and currency
placeholder cleanup. It
never compiles, saves, changes the controller, starts PIE, or modifies source
pack assets. Compile through the dedicated BlueprintTools tool once this call
has returned, validate/read back, then save the three explicit asset paths.
Both existing inventory BPs must have filesystem backups before running.

All layout is independent editable UMG widgets, in the approved 1920x1080
reference composition. Runtime data is deliberately empty until native binds.
"""
import copy
import json

UMG = "UMGToolSet.UMGToolSet."
BP = "editor_toolset.toolsets.blueprint.BlueprintTools."
OBJ = "editor_toolset.toolsets.object.ObjectTools."
HUD_FOLDER = "/Game/AZ/Blueprints/Menu/HUD"
HUD_NAME = "WBP_AZ_GameHUD"
HUD = HUD_FOLDER + "/" + HUD_NAME + "." + HUD_NAME
VITALS = "/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBPCharacterVitalsPanel.AZ_WBPCharacterVitalsPanel"
INVENTORY_SWITCHER = "/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventorySwitcher.AZ_WBP_GameInventorySwitcher"
HUD_PARENT = "/Script/AZ.AZ_Inv_CommonUI_InventoryHudWidget"
LINEAR = "/Game/HQUI_ProgressBars/Widgets/ProgressBarLinear/ProgressBarLinear.ProgressBarLinear_C"
FONT = "/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font"
BODY_FONT = "/Engine/EngineFonts/Roboto.Roboto"
# Root exports these exact native GIMP shapes and imports the two textures
# before authoring. No generated scenery or flattened HUD is imported.
HEART = "/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Heart.T_HUD_Heart"
HEALTH_MASK = "/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_HealthMask.T_HUD_HealthMask"
MODE = "author"


def call(prefix, name, **kwargs):
    return execute_tool(prefix + name, json.dumps(kwargs)).get("returnValue")


def ref(path):
    return {"refPath": path}


def color(hex_value, alpha=1.0):
    rgb = [int(hex_value[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    rgb = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in rgb]
    return dict(zip(("r", "g", "b", "a"), rgb + [alpha]))


WHITE = color("EEEAE0")
SOFT = color("B9B9B9")
CRITICAL = color("E07768")


def slate(c):
    return {"specifiedColor": c, "colorUseRule": "UseColor_Specified"}


def merge(existing, changes):
    result = copy.deepcopy(existing)
    for key, value in changes.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = merge(result[key], value)
        else:
            result[key] = copy.deepcopy(value)
    return result


def properties(instance, changes=None):
    # Required UMG workflow: discover EACH object and slot before read/write.
    schema = json.loads(call(OBJ, "list_properties", instance=instance))
    if changes is None:
        return schema
    missing = [name for name in changes if name not in schema]
    if missing:
        raise RuntimeError("Unknown properties on " + str(instance) + ": " + str(missing))
    before = json.loads(call(OBJ, "get_properties", instance=instance, properties=list(changes)))
    values = merge(before, changes)
    if not call(OBJ, "set_properties", instance=instance, values=json.dumps(values)):
        raise RuntimeError("Property write failed: " + str(instance))
    after = json.loads(call(OBJ, "get_properties", instance=instance, properties=list(changes)))
    return after


def tree(bp):
    return call(UMG, "GetWidgets", widgetBlueprint=bp)


def read_properties(instance, names):
    schema = properties(instance)
    missing = [name for name in names if name not in schema]
    if missing:
        raise RuntimeError("Missing readback properties: " + str(missing))
    return json.loads(call(OBJ, "get_properties", instance=instance, properties=names))


def add(bp, name, widget_class, parent=None, props=None, slot=None, variable=False):
    kwargs = {"widgetBlueprint": bp, "widgetClass": ref(widget_class), "widgetDisplayName": name}
    if parent:
        kwargs["parentWidget"] = parent
    info = call(UMG, "AddWidget", **kwargs)
    if not info or not isinstance(info.get("widget"), dict):
        raise RuntimeError("Could not add " + name)
    widget = info["widget"]
    # Rename, if editor display-name normalization changed a BindWidget name.
    if info["widgetName"] != name:
        info = call(UMG, "RenameWidget", widgetBlueprint=bp, widget=widget, newDisplayName=name)
        widget = info["widget"]
    properties(widget, props or {"visibility": "HitTestInvisible"})
    if isinstance(info.get("slot"), dict):
        properties(info["slot"], slot) if slot else properties(info["slot"])
    call(UMG, "ToggleWidgetAsVariable", widgetBlueprint=bp, widget=widget, bIsVariable=variable)
    return widget


def canvas_slot(x, y, width, height, anchor=(0, 0), align=(0, 0)):
    return {"layoutData": {"offsets": {"left": x, "top": y, "right": width, "bottom": height},
            "anchors": {"minimum": {"x": anchor[0], "y": anchor[1]}, "maximum": {"x": anchor[0], "y": anchor[1]}},
            "alignment": {"x": align[0], "y": align[1]}}, "bAutoSize": False}


def stretch_slot():
    return {"layoutData": {"offsets": {"left": 0, "top": 0, "right": 0, "bottom": 0},
            "anchors": {"minimum": {"x": 0, "y": 0}, "maximum": {"x": 1, "y": 1}},
            "alignment": {"x": 0, "y": 0}}, "bAutoSize": False}


def text(bp, name, parent, box, px, value="", tint=None, font=FONT, align="Left", variable=True):
    # Slate font points at 96 DPI: GIMP reference pixel size * 72 / 96.
    return add(bp, name, "/Script/UMG.TextBlock", parent,
               {"text": value, "font": {"fontObject": ref(font), "typefaceFontName": "Default" if font == FONT else "Regular",
                "size": px * 0.75, "letterSpacing": 0, "skewAmount": 0,
                "outlineSettings": {"outlineSize": 0}},
                "colorAndOpacity": slate(tint or WHITE), "shadowColorAndOpacity": color("000000", 0.72),
                "shadowOffset": {"x": 1, "y": 1}, "justification": align, "visibility": "HitTestInvisible"},
               canvas_slot(*box), variable)


def image(bp, name, parent, box, texture=None, tint=None, angle=0, variable=False):
    brush = {"drawAs": "Image", "tintColor": slate(color("FFFFFF")),
             "imageSize": {"x": box[2], "y": box[3]}}
    if texture:
        brush["resourceObject"] = ref(texture)
    props = {"brush": brush, "colorAndOpacity": tint or WHITE, "visibility": "HitTestInvisible"}
    if angle:
        props["renderTransform"] = {"angle": angle}
    return add(bp, name, "/Script/UMG.Image", parent, props, canvas_slot(*box), variable)


def author_hud():
    # A rerun may resume a complete tree; never clear/delete an existing tree.
    bp = call(UMG, "CreateWidgetBlueprint", folderPath=HUD_FOLDER, assetName=HUD_NAME, parentClass=ref(HUD_PARENT))
    if not isinstance(bp, dict):
        bp = ref(HUD)
    existing = tree(bp)
    if existing["info"]["widgetCount"]:
        raise RuntimeError("HUD tree already exists; inspect and resume explicitly instead of replacing it.")
    root = add(bp, "HUDRoot", "/Script/UMG.CanvasPanel", props={"visibility": "HitTestInvisible"})
    safe = add(bp, "HUDSafeArea", "/Script/UMG.SafeZone", root,
               {"visibility": "HitTestInvisible", "padLeft": True, "padRight": True, "padTop": True, "padBottom": True}, stretch_slot())
    layout = add(bp, "HUDLayout", "/Script/UMG.CanvasPanel", safe,
                 {"visibility": "HitTestInvisible"}, {"hAlign": "HAlign_Fill", "vAlign": "VAlign_Fill"})
    core = add(bp, "CoreHUD", "/Script/UMG.CanvasPanel", layout,
               {"visibility": "HitTestInvisible"}, canvas_slot(-66, -45, 399, 215, (1, 1), (1, 1)))
    # A faint editable scrim keeps the small readout readable on bright scenery.
    image(bp, "CoreContrast", core, (20, 20, 359, 180), tint=color("101515", 0.17))
    weapon = add(bp, "WeaponContainer", "/Script/UMG.CanvasPanel", core,
                 {"visibility": "Collapsed"}, canvas_slot(52, 13, 285, 135), True)
    weapon_icon = image(bp, "WeaponIcon", weapon, (0, 0, 174, 87), variable=True)
    properties(weapon_icon, {"visibility": "Collapsed"})
    text(bp, "WeaponNameText", weapon, (190, 22, 140, 30), 20, tint=SOFT)
    text(bp, "AmmoRoundsText", weapon, (6, 71, 66, 76), 58)
    text(bp, "AmmoCapacityText", weapon, (73, 98, 110, 40), 25, tint=SOFT)
    # Native formats count + MAGS; counts remain individual magazine objects.
    text(bp, "SpareMagazinesText", weapon, (197, 87, 111, 48), 27, align="Right")
    health = add(bp, "HealthContainer", "/Script/UMG.CanvasPanel", core,
                 {"visibility": "Collapsed"}, canvas_slot(59, 161, 275, 50), True)
    image(bp, "HealthIcon", health, (0, 0, 18, 18), texture=HEART, variable=True)
    add(bp, "HealthProgressBar", LINEAR, health,
        {"visibility": "HitTestInvisible", "size": {"x": 241, "y": 10}, "currentPercent": 0,
         "targetPercent": 0, "useTargetPercent": False, "progressMethod": "Static", "interpTimeCurrent": 0.12,
         "fillColorCurrent": WHITE, "backgroundTint": color("717875", 0.52), "backgroundThickness": 0,
         "blendMask": ref(HEALTH_MASK), "effects": [], "bUseCustomMarquee": False,
         "bUseDefaultMarquee": False, "bIsSeparated": False, "fillColorUseGradient": False,
         "pauseWithGamePause": False, "bIsFocusable": False}, canvas_slot(33, 6, 241, 10), True)
    critical = text(bp, "LowHealthText", health, (112, 29, 163, 28), 19, "LOW HEALTH", CRITICAL, align="Right")
    properties(critical, {"visibility": "Collapsed"})
    pickup = add(bp, "PickupContainer", "/Script/UMG.CanvasPanel", layout,
                 {"visibility": "Collapsed"}, canvas_slot(0, -100, 440, 48, (0.5, 1), (0.5, 1)), True)
    text(bp, "PickupText", pickup, (0, 0, 440, 48), 24, font=BODY_FONT, align="Center")
    info = add(bp, "InfoContainer", "/Script/UMG.CanvasPanel", layout,
               {"visibility": "Collapsed"}, canvas_slot(0, -170, 580, 60, (0.5, 1), (0.5, 1)), True)
    text(bp, "InfoText", info, (0, 0, 580, 60), 24, font=BODY_FONT, align="Center")
    hit = add(bp, "HitMarker", "/Script/UMG.CanvasPanel", root,
              {"visibility": "Collapsed"}, canvas_slot(0, 0, 30, 30, (0.5, 0.5), (0.5, 0.5)), True)
    for name, box, angle in [("HitTopLeft", (2, 5, 8, 2), 45), ("HitTopRight", (20, 5, 8, 2), -45),
                             ("HitBottomLeft", (2, 23, 8, 2), -45), ("HitBottomRight", (20, 23, 8, 2), 45)]:
        image(bp, name, hit, box, angle=angle)
    return bp


def node_info(node):
    return call(BP, "get_node_infos", nodes=[node])[0]


def pin(node, name, output=False):
    matches = [p["pin_id"] for p in node_info(node)["output_pins" if output else "input_pins"] if p["name"] == name]
    if len(matches) != 1:
        raise RuntimeError("Pin not uniquely resolved: " + str(node) + "." + name)
    return matches[0]


def wire(source, source_name, destination, destination_name):
    call(BP, "connect_pins", output_pin=pin(source, source_name, True), input_pin=pin(destination, destination_name))


def default(node, name, value):
    call(BP, "set_pin_value", pin=pin(node, name), value=value)


def author_health_bridge(bp):
    # Match the native event before authoring, so stale DLLs cannot silently
    # turn this into an uncalled custom event with no parameter pins.
    events = call(BP, "list_events", blueprint=bp)
    event_infos = [e for e in events if e["name"] == "ApplyHealthPresentation"]
    if len(event_infos) != 1:
        raise RuntimeError("Loaded native parent lacks ApplyHealthPresentation; rebuild/reopen editor first.")
    event = call(BP, "add_event", blueprint=bp, event_name="ApplyHealthPresentation", position={"x": 0, "y": 600})
    event_data = node_info(event)
    if any(p["connected_pins"] for p in event_data["output_pins"] if p["name"] == "then"):
        raise RuntimeError("Health bridge already wired; inspect before modifying.")
    for name in ("Percent", "FillColor", "bImmediate"):
        pin(event, name, True)
    graph = call(BP, "get_graph", blueprint=bp, graph_name="EventGraph")

    def create(type_id, x, y):
        return call(BP, "create_node", graph=graph, type_id=type_id, pos={"x": x, "y": y})

    getters = call(BP, "find_node_types", graph=graph, type_id_filter="GetHealthProgressBar", context_pins=[])
    parent_class = tree(bp)["info"]["parentClass"]
    parent_name = parent_class["refPath"].split(".")[-1].replace("_", "").lower()
    getter_types = sorted(set(t for t in getters if t.endswith("|GetHealthProgressBar")
                              and parent_name in t.replace("_", "").lower()))
    if len(getter_types) != 1:
        raise RuntimeError("Ambiguous HealthProgressBar getter: " + str(getter_types))
    getter = call(BP, "create_node", graph=graph, type_id=getter_types[0],
                  pos={"x": 200, "y": 970}, declaring_class=parent_class)
    branch = create("Utilities|FlowControl|Branch", 290, 600)
    direct = create("ProgressBarGeneral|Set|PBSetProgressMethod(Message)", 530, 500)
    interpolated = create("ProgressBarGeneral|Set|PBSetProgressMethod(Message)", 530, 750)
    percent = create("ProgressBarGeneral|Set|PBSetPercent(Message)", 830, 600)
    fill = create("ProgressBarFillLayer|Set|PBSetFillColor(Message)", 1110, 600)
    # Exact raw enum names verified through AZ_ChooserUtils.DecodeEnum.
    default(direct, "EProgressMethod", "NewEnumerator0")
    default(interpolated, "EProgressMethod", "NewEnumerator1")
    wire(event, "then", branch, "execute")
    wire(event, "bImmediate", branch, "Condition")
    wire(branch, "then", direct, "execute")
    wire(branch, "else", interpolated, "execute")
    for method in (direct, interpolated):
        wire(method, "then", percent, "execute")
    wire(percent, "then", fill, "execute")
    wire(event, "Percent", percent, "Value")
    wire(event, "FillColor", fill, "Color")
    for message in (direct, interpolated, percent, fill):
        wire(getter, "HealthProgressBar", message, "self")
    return {"event": event, "nodes": [event, getter, branch, direct, interpolated, percent, fill]}


def prepare_inventory_health():
    bp = ref(VITALS)
    widgets = tree(bp)["widgets"]
    health = [w for w in widgets if w["widgetName"] == "HealthProgressBar"]
    if len(health) != 1:
        raise RuntimeError("Expected one existing inventory HealthProgressBar")
    properties(health[0]["widget"], {"currentPercent": 0, "targetPercent": 0, "useTargetPercent": False,
               "progressMethod": "Static", "interpTimeCurrent": 0.12, "visibility": "Collapsed"})
    return author_health_bridge(bp)


def author_designer_preview():
    """Cosmetic preview only. The false IsDesignTime branch never writes game values."""
    bp = ref(HUD)
    graph = call(BP, "get_graph", blueprint=bp, graph_name="EventGraph")
    parent = tree(bp)["info"]["parentClass"]
    parent_name = parent["refPath"].split(".")[-1].replace("_", "").lower()
    event = call(BP, "add_event", blueprint=bp, event_name="PreConstruct", position={"x": 0, "y": -500})
    if any(p["connected_pins"] for p in node_info(event)["output_pins"] if p["name"] == "then"):
        raise RuntimeError("Designer preview already wired; inspect instead of duplicating.")
    branch = call(BP, "create_node", graph=graph, type_id="Utilities|FlowControl|Branch", pos={"x": 260, "y": -500})
    wire(event, "then", branch, "execute")
    wire(event, "IsDesignTime", branch, "Condition")
    previous = branch
    created = [event, branch]

    def getter(name, x):
        candidates = call(BP, "find_node_types", graph=graph, type_id_filter="Get" + name, context_pins=[])
        types = sorted(set(t for t in candidates if t.endswith("|Get" + name)
                           and parent_name in t.replace("_", "").lower()))
        if len(types) != 1:
            raise RuntimeError("Designer getter is ambiguous: " + name + str(types))
        result = call(BP, "create_node", graph=graph, type_id=types[0], pos={"x": x, "y": -250}, declaring_class=parent)
        created.append(result)
        return result

    actions = [
        ("Widget|SetVisibility", "/Script/UMG.Widget", "HealthContainer", {"InVisibility": "HitTestInvisible"}),
        ("Widget|SetVisibility", "/Script/UMG.Widget", "WeaponContainer", {"InVisibility": "HitTestInvisible"}),
        ("Widget|SetVisibility", "/Script/UMG.Widget", "WeaponIcon", {"InVisibility": "HitTestInvisible"}),
        ("Appearance|SetBrushfromTexture", "/Script/UMG.Image", "WeaponIcon", {"Texture": "/Game/AZ/Assets/Weapons/AK12_Rifle/UI/Textures/Rifle_PrimaryIcon.Rifle_PrimaryIcon", "bMatchSize": "false"}),
        ("Widget|SetText(Text)", "/Script/UMG.TextBlock", "WeaponNameText", {"InText": "M16"}),
        ("Widget|SetText(Text)", "/Script/UMG.TextBlock", "AmmoRoundsText", {"InText": "17"}),
        ("Widget|SetText(Text)", "/Script/UMG.TextBlock", "AmmoCapacityText", {"InText": "/ 30"}),
        ("Widget|SetText(Text)", "/Script/UMG.TextBlock", "SpareMagazinesText", {"InText": "2 MAGS"}),
        ("ProgressBarGeneral|Set|PBSetProgressMethod(Message)", None, "HealthProgressBar", {"EProgressMethod": "NewEnumerator0"}),
        ("ProgressBarGeneral|Set|PBSetPercent(Message)", None, "HealthProgressBar", {"Value": "0.72"}),
    ]
    for i, (type_id, owner, target, values) in enumerate(actions):
        x = 510 + i * 300
        kwargs = {"graph": graph, "type_id": type_id, "pos": {"x": x, "y": -500}}
        if owner:
            kwargs["declaring_class"] = ref(owner)
        node = call(BP, "create_node", **kwargs)
        widget = getter(target, x)
        for name, value in values.items():
            default(node, name, value)
        wire(widget, target, node, "self")
        wire(previous, "then", node, "execute")
        previous = node
        created.append(node)
    return {"nodes": created, "scope": "IsDesignTime=true only; runtime remains empty until native data binding."}


def refine_contrast():
    """Each backdrop follows its own data-driven container, including unarmed play."""
    bp = ref(HUD)
    widgets = {w["widgetName"]: w for w in tree(bp)["widgets"]}
    backdrop = widgets["CoreContrast"]
    weapon = widgets["WeaponContainer"]["widget"]
    if backdrop["parent"] != weapon:
        backdrop = call(UMG, "MoveWidget", widgetBlueprint=bp, widget=backdrop["widget"], newParent=weapon, childIndex=0)
    properties(backdrop["slot"], merge(canvas_slot(-10, 15, 325, 123), {"zOrder": -1}))
    properties(backdrop["widget"], {"colorAndOpacity": color("101515", 0.17)})
    health = widgets["HealthContainer"]["widget"]
    if "HealthContrast" not in widgets:
        image(bp, "HealthContrast", health, (-8, -5, 294, 28), tint=color("101515", 0.25))
    current = {w["widgetName"]: w for w in tree(bp)["widgets"]}
    properties(current["HealthContrast"]["slot"], {"zOrder": -1})
    return {"weapon_backdrop": backdrop, "health_backdrop": current["HealthContrast"]}


def currency_audit():
    wanted = {"CurrencyContentHBox", "CurrencyAmountText", "CurrencyNameText", "CurrencyIcon",
              "CurrencyBarOverlay", "SectionLabelText", "ActiveSelectionLabelText"}
    widgets = tree(ref(INVENTORY_SWITCHER))["widgets"]
    selected = {w["widgetName"]: w for w in widgets if w["widgetName"] in wanted}
    if set(selected) != wanted:
        raise RuntimeError("Inventory cleanup widgets changed: " + str(wanted - set(selected)))
    result = {}
    for name, info in selected.items():
        fields = ["visibility", "text"] if name.endswith("Text") else ["visibility"]
        values = read_properties(info["widget"], fields)
        siblings = [w for w in widgets if w["parent"] == info["parent"]]
        result[name] = {"widget": info["widget"], "parent": info["parent"],
                        "child_index": [w["widget"] for w in siblings].index(info["widget"]),
                        "properties": values}
        if name == "SectionLabelText":
            slot_schema = properties(info["slot"])
            fields = [field for field in ("padding", "horizontalAlignment", "verticalAlignment", "size") if field in slot_schema]
            result[name]["slot_properties"] = read_properties(info["slot"], fields)
    return result


def cleanup_inventory_currency():
    """Keep the real BACKPACK header and hide only unsupported currency children.

    Return a before/after receipt including parent/index/slot and text values
    so the operation can be reversed without deleting or reparenting widgets.
    Persist that returned receipt next to the pre-edit .uasset backup.
    """
    before = currency_audit()
    for name, sample in (("CurrencyAmountText", "100000"), ("CurrencyNameText", "CAD")):
        value = before[name]["properties"]["text"]
        if value not in (sample, ""):
            raise RuntimeError("Currency value is no longer the audited sample: " + name + "=" + value)
    header = before["SectionLabelText"]
    group = before["CurrencyContentHBox"]["widget"]
    if header["parent"] != group:
        raise RuntimeError("BACKPACK header has an unexpected parent; inspect before cleanup.")
    if header["properties"]["text"] != "BACKPACK":
        raise RuntimeError("BACKPACK header text changed; inspect before cleanup.")
    for name in ("CurrencyAmountText", "CurrencyNameText"):
        properties(before[name]["widget"], {"text": "", "visibility": "Collapsed"})
    properties(before["CurrencyIcon"]["widget"], {"visibility": "Collapsed"})
    after = currency_audit()
    assert_currency_cleanup(after)
    if after["SectionLabelText"] != before["SectionLabelText"]:
        raise RuntimeError("BACKPACK header or layout changed unexpectedly.")
    if after["CurrencyContentHBox"] != before["CurrencyContentHBox"]:
        raise RuntimeError("Currency/header container changed unexpectedly.")
    if after["ActiveSelectionLabelText"] != before["ActiveSelectionLabelText"]:
        raise RuntimeError("Inventory category header changed unexpectedly.")
    return {"asset": INVENTORY_SWITCHER, "before": before, "after": after,
            "restore": "Restore the recorded properties of CurrencyAmountText, CurrencyNameText, and CurrencyIcon. No widgets or slots were deleted or moved."}


def assert_currency_cleanup(audit):
    for name in ("CurrencyAmountText", "CurrencyNameText", "CurrencyIcon"):
        if audit[name]["properties"]["visibility"] != "Collapsed":
            raise RuntimeError("Unsupported currency child remains visible: " + name)
    for name in ("CurrencyAmountText", "CurrencyNameText"):
        if audit[name]["properties"]["text"]:
            raise RuntimeError("Sample currency text remains: " + name)
    header = audit["SectionLabelText"]
    if header["parent"] != audit["CurrencyContentHBox"]["widget"] or header["properties"]["text"] != "BACKPACK":
        raise RuntimeError("BACKPACK header was not preserved in its original container.")
    if audit["CurrencyContentHBox"]["properties"]["visibility"] in ("Collapsed", "Hidden"):
        raise RuntimeError("BACKPACK header container is hidden.")
    if header["properties"]["visibility"] in ("Collapsed", "Hidden"):
        raise RuntimeError("BACKPACK header is hidden.")


def verify_assets():
    required = {"HealthContainer", "HealthIcon", "HealthProgressBar", "WeaponContainer", "WeaponIcon",
                "WeaponNameText", "AmmoRoundsText", "AmmoCapacityText", "SpareMagazinesText", "LowHealthText",
                "PickupContainer", "PickupText", "InfoContainer", "InfoText", "HitMarker"}
    hud = tree(ref(HUD))
    names = {w["widgetName"] for w in hud["widgets"]}
    if required - names:
        raise RuntimeError("Missing native BindWidget fields: " + str(required - names))
    currency = currency_audit()
    # The user retained the authored inventory appearance after reviewing Phase 1.
    # Read its state without treating placeholders as permission to hide panels.
    return {"hud": hud, "inventory": tree(ref(VITALS)), "inventory_appearance": currency}


def run():
    if MODE == "refine_contrast":
        return {"contrast": refine_contrast()}
    if MODE == "designer_preview":
        return {"designer_preview": author_designer_preview()}
    if MODE == "verify":
        return verify_assets()
    if MODE == "inventory_bridge":
        return {"inventory_bridge": prepare_inventory_health()}
    if MODE == "hud_bridge":
        return {"hud_bridge": author_health_bridge(ref(HUD))}
    if MODE == "hud_tree":
        return {"hud": author_hud()}
    if MODE in ("inventory_cleanup", "inventory_cleanup_verify"):
        raise RuntimeError("Inventory cleanup was reversed after user review. Preserve its authored appearance; use inventory_appearance for readback.")
    if MODE == "inventory_appearance":
        return {"inventory_appearance": currency_audit()}
    if MODE != "author":
        raise RuntimeError("Unknown MODE: " + MODE)
    bp = author_hud()
    refine_contrast()
    hud_bridge = author_health_bridge(bp)
    designer_preview = author_designer_preview()
    inventory_bridge = prepare_inventory_health()
    inventory_appearance = currency_audit()
    return {"hud_bridge": hud_bridge, "designer_preview": designer_preview, "inventory_bridge": inventory_bridge,
            "inventory_appearance": inventory_appearance, "authored_assets": [HUD, VITALS],
            "next": "Dedicated compile, readback, explicit save; no PIE."}
