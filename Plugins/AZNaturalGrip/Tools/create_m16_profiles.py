# @Description: Create / update the AZ Natural Grip profiles of the M16 (support hand left, trigger hand right)
# exec(open(r"C:/UnrealEngine/Games/AZ/Plugins/AZNaturalGrip/Tools/create_m16_profiles.py").read())
# Values = the Python solver's M16 runs of 2026-09-30 / 2026-10-01 (Tools/wgs/natgrip): left = lsolve_m16.py stage1/2/final
# (THIN 2 / 1, fine field box -8,12,0 .. 10,34,20 at 0.25), right = place_nat.py + rsolve3.py (THIN 1, fine field box
# -9,-20,-13 .. 8,10,14 at 0.15, NATGRIP_TRIGGER -1.08,6.65,5.0 ...). Saving a data asset is safe from Python.
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
PATH = "/AZNaturalGrip/Profiles"
WEAPON = "/Game/AZ/Assets/M16/SKL/M16_Skeleton"
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
WEAPON_BP = "/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle"


def make(name, props):
    full = PATH + "/" + name
    if EAL.does_asset_exist(full):
        asset = unreal.load_asset(full)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.AZNaturalGripProfile)
        asset = AT.create_asset(name, PATH, unreal.AZNaturalGripProfile, factory)
    asset.set_editor_property("weapon_mesh", unreal.load_asset(WEAPON))
    asset.set_editor_property("hero_mesh", unreal.load_asset(HERO))
    asset.set_editor_property("weapon_key", "m16")
    asset.set_editor_property("weapon_blueprint", unreal.load_asset(WEAPON_BP))
    for k, v in props.items():
        asset.set_editor_property(k, v)
    ok = EAL.save_asset(full, only_if_is_dirty=False)
    print("[create_m16_profiles]", full, "saved", ok)
    return asset


make("NGP_M16_Left", {
    "hand": unreal.AZGripHand.SUPPORT,
    "search_thin": 2, "final_thin": 1,
    "fine_box_min": unreal.Vector(-8.0, 12.0, 0.0), "fine_box_max": unreal.Vector(10.0, 34.0, 20.0), "fine_spacing": 0.25,
})
make("NGP_M16_Right", {
    "hand": unreal.AZGripHand.TRIGGER,
    "search_thin": 1, "final_thin": 1,
    "fine_box_min": unreal.Vector(-9.0, -20.0, -13.0), "fine_box_max": unreal.Vector(8.0, 10.0, 14.0), "fine_spacing": 0.15,
})
import gc
gc.collect()
