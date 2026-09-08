# @Description: Author the first M16 inventory/equipment slice using existing pickup and weapon assets.
"""Run in the editor after building AZEditor. Authors assets only; never starts PIE."""
import gc
from pathlib import Path
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
BACKUP = ROOT / 'Saved/Backups/RifleInventoryFoundation'
RIFLE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
MAGAZINE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BP_Pickup_M16Magazine'
PC = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
MAG_MESH = '/Game/Assets/M16/mesh/UE4_M16_MagazMod'
RIFLE_ICON = '/Game/AZ/Assets/Weapons/AK12_Rifle/UI/Textures/Rifle_PrimaryIcon'
MAG_ICON = '/Game/InventorySystemPro/ExampleContent/Common/Art/Ammo556/T_Ammo_556Icon'


def backup(package):
    relative = package.removeprefix('/Game/') + '.uasset'
    source = ROOT / 'Content' / relative
    destination = BACKUP / relative
    if source.exists() and not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def component_template(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if isinstance(obj, unreal.AZ_Inv_CommonUI_ItemComponent):
            return obj
    raise RuntimeError('Missing CommonUI item template: ' + bp.get_path_name())


def text_fragment(tag, value):
    return '/Script/AZ.AZ_Inv_CommonUI_Text_Fragment(FragmentTag=(TagName="%s"),FragmentText="%s")' % (tag, value)


def manifest(package, magazine_rounds=None):
    is_magazine = magazine_rounds is not None
    size = (1, 2) if is_magazine else (3, 2)
    icon = MAG_ICON if is_magazine else RIFLE_ICON
    title = 'M16 magazine' if is_magazine else 'M16'
    description = 'Detachable 30-round M16 magazine.' if is_magazine else 'M16 rifle with a detachable magazine.'
    fragments = [
        '/Script/AZ.AZ_Inv_CommonUI_GridFragment(GridSize=(X=%d,Y=%d),FragmentTag=(TagName="Item.Fragment.Grid"))' % size,
        '/Script/AZ.AZ_Inv_CommonUI_ImageFragment(Icon="/Script/Engine.Texture2D\'%s.%s\'",IconDimensions=(X=128,Y=64),FragmentTag=(TagName="Item.Fragment.Icon"))' % (icon, icon.rsplit('/', 1)[1]),
        text_fragment('Item.Fragment.Name.StaticText', title),
        text_fragment('Item.Fragment.Ammo.Primary.Name', title),
        text_fragment('Item.Fragment.Text', description),
        text_fragment('Item.Fragment.Description', description),
        text_fragment('Item.Fragment.Ammo.Text', 'Magazine:'),
        text_fragment('Item.Fragment.Ammo.Value', ''),
        text_fragment('Item.Fragment.Ammo.Separator', '/'),
        text_fragment('Item.Fragment.Ammo.MaxValue', ''),
        text_fragment('Item.Fragment.Ammo.StaticText', 'rounds'),
    ]
    if is_magazine:
        fragments.append('/Script/AZ.AZ_Inv_CommonUI_MagazineFragment(MagazineFamily="M16.Standard",Capacity=30,InitialRounds=%d)' % magazine_rounds)
    else:
        fragments.extend([
            '/Script/AZ.AZ_Inv_CommonUI_EquipmentFragment(EquipmentType=(TagName="Item.Type.Weapon.Rifle"),FragmentTag=(TagName="Item.Fragment.Equipment"))',
            '/Script/AZ.AZ_Inv_CommonUI_WeaponStateFragment(bUsesDetachableMagazines=True,MagazineFamily="M16.Standard",WeaponTag=(TagName="Weapon.Rifle"),WeaponActorClass="/Script/Engine.BlueprintGeneratedClass\'/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle.AZ_BP_Rifle_C\'")',
        ])
    item_type = 'Item.Type.Magazine.Rifle' if is_magazine else 'Item.Type.Weapon.Rifle'
    content = '(Fragments=(%s),ItemCategory=Equippable,ItemTypeTag=(TagName="%s"),PickupActorClass="/Script/Engine.BlueprintGeneratedClass\'%s.%s_C\'")' % (','.join(fragments), item_type, package, package.rsplit('/', 1)[1])
    result = unreal.AZ_Inv_CommonUI_ItemManifest()
    if not result.import_text(content):
        raise RuntimeError('Could not author manifest: ' + package)
    return result


def main():
    # Preserve the original rifle pickup, controller defaults, and any prior authored magazines.
    for path in [RIFLE, PC, MAGAZINE, MAGAZINE + '_Partial', MAGAZINE + '_Empty']:
        backup(path)
    for required in [RIFLE, PC, MAG_MESH, RIFLE_ICON, MAG_ICON]:
        if not unreal.load_asset(required):
            raise RuntimeError('Missing content: ' + required)

    for suffix, rounds in [('', 30), ('_Partial', 17), ('_Empty', 0)]:
        path = MAGAZINE + suffix
        bp = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(RIFLE, path)
        if not bp:
            raise RuntimeError('Could not create magazine pickup: ' + path)
        item = component_template(bp)
        item.set_editor_property('pickup_item_manifest', manifest(path, rounds))
        item.set_editor_property('initial_contained_item_manifests', [])
        item.set_editor_property('pickup_message', 'Press E to pick up M16 magazine (%d/30)' % rounds)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.get_component_by_class(unreal.StaticMeshComponent).set_editor_property('static_mesh', unreal.load_asset(MAG_MESH))
        cdo.get_component_by_class(unreal.SkeletalMeshComponent).set_editor_property('skeletal_mesh_asset', None)
        if not unreal.EditorAssetLibrary.save_loaded_asset(bp):
            raise RuntimeError('Save failed: ' + path)
        print('AUTHORED_MAGAZINE', path, rounds)

    rifle_bp = unreal.load_asset(RIFLE)
    item = component_template(rifle_bp)
    rifle_manifest = manifest(RIFLE)
    inserted_manifest = manifest(MAGAZINE, 30)
    item.set_editor_property('pickup_item_manifest', rifle_manifest)
    item.set_editor_property('initial_contained_item_manifests', [inserted_manifest])
    item.set_editor_property('pickup_message', 'Press E to pick up M16')
    if not unreal.EditorAssetLibrary.save_loaded_asset(rifle_bp):
        raise RuntimeError('Save failed: ' + RIFLE)

    pc_bp = unreal.load_asset(PC)
    pc = unreal.get_default_object(pc_bp.generated_class())
    quickbar = pc.get_component_by_class(unreal.AZ_QuickBarComponent)
    slots = list(quickbar.get_editor_property('slots'))
    rifle_slot = unreal.AZ_QuickSlot()
    if not rifle_slot.import_text('(WeaponTag=(TagName="Weapon.Rifle"),bInventoryBacked=True,InventoryItemType=(TagName="Item.Type.Weapon.Rifle"),bStrafeOnEquip=False)'):
        raise RuntimeError('Could not author rifle quick slot')
    if len(slots) == 1:
        slots.append(rifle_slot)
    elif len(slots) > 1:
        slots[1] = rifle_slot
    else:
        raise RuntimeError('Existing fists slot missing; preserve its authored grants')
    quickbar.set_editor_property('slots', slots)
    if not unreal.EditorAssetLibrary.save_loaded_asset(pc_bp):
        raise RuntimeError('Save failed: ' + PC)
    for name in ['AZ_IA_RT_Weapon_0', 'AZ_IA_RT_Weapon_1']:
        path = '/Game/AZ/Blueprints/Input/InputActions/RT/' + name
        backup(path)
        action = unreal.load_asset(path)
        # Selection toggles on one physical press, never again on release.
        trigger = unreal.new_object(unreal.InputTriggerPressed, outer=action)
        action.set_editor_property('triggers', [trigger])
        if not unreal.EditorAssetLibrary.save_loaded_asset(action):
            raise RuntimeError('Save failed: ' + path)
    print('AUTHORED_RIFLE', RIFLE, 'inserted magazine 30/30; slots 0=fists, 1=rifle')


try:
    main()
finally:
    gc.collect()
