"""Migrate the five accepted example pickups to editable content slots; no PIE."""
from pathlib import Path
from datetime import datetime
import shutil
import unreal


def run(allow_own_dirty=False):
    ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ed.get_editor_world()
    assert not ed.get_game_world(), 'Stop PIE before content setup'
    assert world.get_path_name().startswith('/Game/AZ/Maps/L_001.')
    assert allow_own_dirty or world.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Map has unsaved changes'
    assert hasattr(unreal, 'AZ_ContainerContentSlot'), 'Rebuild and restart required'
    folder = Path('C:/UnrealEngine/Games/AZ/Saved/InteractionAudit')/('contents-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    folder.mkdir(parents=True)
    shutil.copy2('C:/UnrealEngine/Games/AZ/Content/AZ/Maps/L_001.umap', folder/'L_001-before.umap')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    pickups = {}
    for a in actors:
        c = a.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent)
        if c: pickups[c.get_editor_property('CampaignPickupId').to_string()] = a
    handles = {'Drawer A':(27,32,89), 'Drawer B':(27,-32,89),
               'Locker Left':(4,-6,0), 'Locker Right':(-2,-6,0), 'Chest':(43,0,8), 'Door':(70,2,100)}
    updated = 0
    for a in actors:
        if not isinstance(a, unreal.AZ_InteractiveDoor): continue
        name = a.get_actor_label().removeprefix('AZ INTERACTIVE ')
        if name in handles:
            a.set_editor_property('bUseCustomInteractionPoint', True)
            a.set_editor_property('InteractionPoint', unreal.Vector(*handles[name]))
        ids = a.get_editor_property('ContentPickupIds')
        if not ids: continue
        # Re-running must not reset positions or quantities already configured by the designer.
        if a.get_editor_property('Contents'): continue
        slots = []
        for identity in ids:
            pickup = pickups[identity.to_string()]
            slot = unreal.AZ_ContainerContentSlot()
            slot.set_editor_property('PickupClass', pickup.get_class())
            slot.set_editor_property('Quantity', 1)
            slot.get_editor_property('PickupId').import_text(identity.export_text())
            moving = name.startswith('Drawer ')
            slot.set_editor_property('bMovesWithPanel', moving)
            parent = a.get_editor_property('DoorPivot') if moving else a.get_editor_property('root_component')
            relative = unreal.MathLibrary.make_relative_transform(pickup.get_actor_transform(), parent.get_world_transform())
            if moving:
                relative.translation = relative.translation - a.get_editor_property('OpenTranslation')
            slot.set_editor_property('LocalTransform', relative)
            slots.append(slot)
        a.set_editor_property('Contents', slots)
        a.apply_content_setup()
        updated += 1

    # Use an owned copy of the pack's progress renderer, not its input manager.
    hud = unreal.load_asset('/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD')
    assert hud.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'HUD has unrelated edits'
    progress_path = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_InteractionHold'
    progress = unreal.load_asset(progress_path)
    if not progress:
        progress = unreal.EditorAssetLibrary.duplicate_asset('/Game/InteractionEssentials/Widgets/ProgressBar/WB_CircularPB', progress_path)
    assert progress
    defaults = unreal.get_default_object(progress.generated_class())
    defaults.set_editor_property('CurrentPercent', 0.0)
    defaults.set_editor_property('bUseTargetPercent', False)
    defaults.set_editor_property('bUseMarquee', False)
    defaults.set_editor_property('FillColor', unreal.LinearColor(.855,.823,.745,1))
    unreal.get_default_object(hud.generated_class()).set_editor_property('PickupHoldWidgetClass', progress.generated_class())
    assert unreal.EditorLoadingAndSavingUtils.save_packages([progress.get_outermost(), hud.get_outermost()],False)
    assert unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()],False)
    print('CONTENT_SLOTS_CONFIGURED',updated,'user_playtest_required=True')


run()
