"""Author owned throwable data and pickups. Run inside Unreal, never starts PIE.
Save only named output packages; unrelated editor changes are not included.
"""
import gc
import json
import os
import unreal

ROOT = '/Game/AZ/Blueprints/Throwables'
ITEMS = '/Game/AZ/Blueprints/Items/Throwables'
REPORT = 'C:/UnrealEngine/Games/AZ/Saved/ThrowableCompletion/asset-authoring.json'


def require(path):
    obj = unreal.load_asset(path)
    if not obj:
        raise RuntimeError('Missing asset: ' + path)
    return obj


def save(obj):
    # Blueprint save can report success without writing when dirty-only is used.
    package = obj.get_outermost()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([package], False):
        raise RuntimeError('Save failed: ' + obj.get_path_name())
    rel = obj.get_path_name().split('.')[0].replace('/Game/', 'Content/') + '.uasset'
    if not os.path.isfile('C:/UnrealEngine/Games/AZ/' + rel):
        raise RuntimeError('No asset file after save: ' + rel)


def duplicate(source, dest):
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return require(dest)
    obj = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
    if not obj:
        raise RuntimeError('Duplicate failed: ' + dest)
    return obj


def component_objects(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj:
            yield obj


def build_manifest(name, tag, category, maximum, bp_path, icon_path, definition_path=None):
    # Fixed game data. No textual source replacement or user-supplied T3D is executed.
    parts = [
        '/Script/AZ.AZ_Inv_CommonUI_GridFragment(GridSize=(X=1,Y=1),FragmentTag=(TagName="Item.Fragment.Grid"))',
        '/Script/AZ.AZ_Inv_CommonUI_ImageFragment(Icon="/Script/Engine.Texture2D\'' + icon_path + '.' + icon_path.rsplit('/', 1)[1] + '\'",IconDimensions=(X=100,Y=100),FragmentTag=(TagName="Item.Fragment.Icon"))',
        '/Script/AZ.AZ_Inv_CommonUI_Stackable_Fragment(MaxStackSize=' + str(maximum) + ',StackCount=1,FragmentTag=(TagName="Item.Fragment.Stackable"))',
        '/Script/AZ.AZ_Inv_CommonUI_Text_Fragment(FragmentText=NSLOCTEXT("AZThrowables","' + name.replace(' ', '') + '","' + name + '"),FragmentTag=(TagName="Item.Fragment.Ammo.Primary.Name"))',
        '/Script/AZ.AZ_Inv_CommonUI_Text_Fragment(FragmentText=NSLOCTEXT("AZThrowables","' + name.replace(' ', '') + 'Title","' + name + '"),FragmentTag=(TagName="Item.Fragment.Name.StaticText"))',
    ]
    if definition_path:
        parts.append('/Script/AZ.AZ_Inv_CommonUI_ThrowableFragment(ThrowableDefinition="/Script/AZ.AZ_ThrowableDefinition\'' + definition_path + '.' + definition_path.rsplit('/', 1)[1] + '\'",FragmentTag=(TagName="Item.Fragment.Throwable"))')
    return '(Fragments=(' + ','.join(parts) + '),ItemCategory=' + category + ',ItemTypeTag=(TagName="' + tag + '"),PickupActorClass="/Script/Engine.BlueprintGeneratedClass\'' + bp_path + '.' + bp_path.rsplit('/', 1)[1] + '_C\'")'


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError('PIE running; no writes performed')

    stone_mesh = require('/Game/PostDistrict/Models/Nature/Rocks/SM_Rock_Small_1')
    bottle_mesh = require('/Game/ClassicMansion/Meshes/SM_WineBottleA')
    flame = require('/Game/StarterContent/Particles/P_Fire')
    glass_fx = require('/Game/NextGenDestruction/FX/Destruction/Spawnable/NS_Breaking_Glass')
    glass_sound = require('/Game/NextGenDestruction/Audio/Destruction/Glass/Glass_Break_SFX')
    definitions = {}
    for key, mesh, size, recoverable, shatter, fire in [
        ('Stone', stone_mesh, 12.0, True, False, False),
        ('Bottle', bottle_mesh, 28.0, True, True, False),
        ('Incendiary', require('/Game/AZ/Assets/Throwables/SM_ThrowIncendiary'), 28.0, False, True, True),
    ]:
        definition = duplicate(ROOT + '/DA_Throwable_Grenade', ROOT + '/DA_Throwable_' + key)
        definition.set_editor_property('HeldMesh', mesh)
        definition.set_editor_property('HeldSkeletalMesh', None)
        definition.set_editor_property('HeldMeshSize', size)
        definition.set_editor_property('ImpactBehavior', unreal.AZ_ThrowImpactBehavior.SHATTER if shatter else unreal.AZ_ThrowImpactBehavior.BOUNCE_AND_SETTLE)
        definition.set_editor_property('bRecoverable', recoverable)
        definition.set_editor_property('CollisionRadius', 8.0 if key == 'Stone' else 16.0)
        definition.set_editor_property('HeldPropOffset', unreal.Transform(location=unreal.Vector(0,0,-3.7 if key=='Stone' else -14.0)))
        definition.set_editor_property('ImpactLoudness', 1.0)
        definition.set_editor_property('ImpactNoiseTag', 'ThrowImpact')
        definition.set_editor_property('bIgnitesOnImpact', fire)
        definition.set_editor_property('ShatterMinImpactSpeed', 0.0 if fire else 400.0)
        definition.set_editor_property('ShatterEffect', glass_fx if shatter else None)
        definition.set_editor_property('ShatterSound', glass_sound if shatter else None)
        if fire:
            tag = unreal.GameplayTag()
            tag.import_text('(TagName="Item.Type.Craftable.Tool.Igniter")')
            definition.set_editor_property('RequiredIgnitionTool', tag)
            definition.set_editor_property('GroundFireParticles', flame)
            definition.set_editor_property('BurningTargetParticles', flame)
            definition.set_editor_property('HeldIgnitionParticles', flame)
            definition.set_editor_property('FireLoopSound', require('/Game/InventorySystemPro/ExampleContent/Common/Sounds/Fire/SC_Campfire_Burning_01'))
        save(definition)
        definitions[key] = definition

    # Separate game resources. Mesh/icon choices below are prototype art requiring visual review.
    icon_root = '/Game/InventorySystemPro/ExampleContent/Common/Art/'
    entries = [
        ('Stone', 'Stone', 'Item.Type.Consumable.Throwable.Stone', 'Equippable', 5, stone_mesh, 0.08175, icon_root + 'IronOre/T_SteelOreIcon', 'Stone'),
        ('Bottle', 'Glass Bottle', 'Item.Type.Consumable.Throwable.Bottle', 'Equippable', 3, bottle_mesh, 0.90767, icon_root + 'Potions/T_WaterBottle_Icon', 'Bottle'),
        ('Incendiary', 'Incendiary Bottle', 'Item.Type.Consumable.Throwable.Incendiary', 'Equippable', 2, require('/Game/AZ/Assets/Throwables/SM_ThrowIncendiary'), 0.94442, icon_root + 'WoodenSign/Torch/T_TorchIcon', 'Incendiary'),
        ('Fabric', 'Fabric', 'Item.Type.Craftable.Material.Fabric', 'Craftable', 10, require('/Game/Safe_House/meshes/SM_cloth_02_01'), 0.15, icon_root + 'Lootbag/T_LootBagIcon', None),
        ('Fuel', 'Fuel', 'Item.Type.Craftable.Material.Fuel', 'Craftable', 10, require('/Game/Safe_House/meshes/SM_weapon_props_lighter_fluid'), 1.0, icon_root + 'HerbLiquid/T_LiquidHealthRedIcon', None),
        ('Igniter', 'Igniter', 'Item.Type.Craftable.Tool.Igniter', 'Craftable', 1, require('/Game/AZ/Assets/Throwables/SM_ThrowIgniter'), 1.0, icon_root + 'Key/T_HotelKeyIcon', None),
    ]
    outputs = []
    for key, name, tag, category, maximum, mesh, scale, icon, def_key in entries:
        icon = '/Game/AZ/UI/FieldNotes/Throwables/T_Throwable_' + key
        require(icon)
        path = ITEMS + '/BP_Pickup_' + key
        bp = duplicate(ITEMS + '/BP_Pickup_Grenade', path)
        text = build_manifest(name, tag, category, maximum, path, icon,
                              ROOT + '/DA_Throwable_' + def_key if def_key else None)
        if not unreal.AZ_Inv_AuthoringUtils.set_pickup_manifest_text(bp, text):
            raise RuntimeError('Manifest import failed: ' + path)
        cdo = unreal.get_default_object(bp.generated_class())
        visual = cdo.get_editor_property('mesh_component')
        visual.set_static_mesh(mesh)
        visual.set_editor_property('relative_scale3d', unreal.Vector(scale, scale, scale))
        cdo.get_editor_property('skeletal_mesh_component').set_skeletal_mesh_asset(None)
        for obj in component_objects(bp):
            if isinstance(obj, unreal.AZ_Inv_CommonUI_ItemComponent):
                obj.set_editor_property('PickupCaption', name)
        saved_text = unreal.AZ_Inv_AuthoringUtils.get_pickup_manifest_text(bp)
        if tag not in saved_text or (def_key and 'DA_Throwable_' + def_key not in saved_text):
            raise RuntimeError('Manifest readback failed: ' + path)
        save(bp)
        outputs.append({'asset':path,'manifest':saved_text})

    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w', encoding='utf-8') as out:
        json.dump({'definitions':[d.get_path_name() for d in definitions.values()], 'pickups':outputs,
                   'note':'Owned icons; recipe/placed instance synchronization and user visual review still required after authoring.'}, out, indent=2)
    print('THROWABLE_ASSETS_SAVED definitions=3 pickups=6')


try:
    main()
finally:
    gc.collect()


