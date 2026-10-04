# @Description: Create the weapon grip field asset from the baked lattice and assign it to the weapon BP
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/import_grip_field.py").read(), {"FIELD_JSON": ..., "ASSET": ..., "WEAPON_BP": ...})
import json
import unreal

FIELD_JSON = globals().get("FIELD_JSON", "C:/UnrealEngine/Games/AZ/Saved/wgs/fields/winchester_field.json")
ASSET = globals().get("ASSET", "/Game/AZ/Blueprints/Weapon/Grip/GF_Winchester")
WEAPON_BP = globals().get("WEAPON_BP", "/Game/AZ/Blueprints/Weapon/AZ_BP_Winchester")
EAL = unreal.EditorAssetLibrary

d = json.load(open(FIELD_JSON))
if EAL.does_asset_exist(ASSET):
    field = unreal.load_asset(ASSET)
else:
    folder, name = ASSET.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.AZ_WeaponGripField)
    field = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AZ_WeaponGripField, factory)
o, n = d["origin"], d["dims"]
field.set_editor_property("origin", unreal.Vector(o[0], o[1], o[2]))
field.set_editor_property("spacing", float(d["spacing"]))
field.set_editor_property("dims", unreal.IntVector(n[0], n[1], n[2]))
field.set_editor_property("distances", [float(v) for v in d["distances"]])
print("field", field.get_path_name(), "nodes", len(field.get_editor_property("distances")), "dims", n, "saved", EAL.save_asset(ASSET, only_if_is_dirty=False))

bp = unreal.load_asset(WEAPON_BP)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("grip_field", field)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print("weapon grip_field", unreal.get_default_object(bp.generated_class()).get_editor_property("grip_field"), "saved", EAL.save_asset(WEAPON_BP, only_if_is_dirty=False))
import gc; gc.collect()
