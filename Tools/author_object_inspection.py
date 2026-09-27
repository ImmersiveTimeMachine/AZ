"""Create owned inspection prototypes, then place them after Blueprint compilation.
Run assets() first; compile the four returned BPs with the dedicated editor tool;
then run place(). Never starts PIE or modifies source-pack assets.
"""
from pathlib import Path
from datetime import datetime
import ast
import json
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
FOLDER = '/Game/AZ/Blueprints/Items/Inspection'
ENTRIES = [('Photo','11 Picture',True),('Book','09 Book',True),('Note','10 Note',True),('Radio','12 Radio',False)]


def scene():
    ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not ed.get_game_world(), 'Stop PIE before authoring'
    w = ed.get_editor_world()
    assert w.get_path_name().startswith('/Game/AZ/Maps/L_001.'), 'Expected showcase map'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return w, actors, {a.get_actor_label():a for a in actors.get_all_level_actors()}


def assets():
    _, _, existing = scene()
    source = ROOT/'Tools/author_throwable_completion.py'
    tree = ast.parse(source.read_text(encoding='utf-8-sig'))
    function = next(x for x in tree.body if isinstance(x,ast.FunctionDef) and x.name=='build_manifest')
    ns = {}
    exec(compile(ast.Module(body=[function],type_ignores=[]),str(source),'exec'),ns)
    outputs = []
    for name, label, takeable in ENTRIES:
        original = existing['AZ INTERACTION '+label]
        path = FOLDER+'/BP_Inspect_'+name
        bp = unreal.load_asset(path)
        if not bp:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property('parent_class', unreal.AZ_InspectablePickup if takeable else unreal.AZ_InspectableObject)
            bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_Inspect_'+name,FOLDER,unreal.Blueprint,factory)
        assert bp
        cdo = unreal.get_default_object(bp.generated_class())
        bp.modify(); cdo.modify()
        mesh = cdo.get_editor_property('Mesh')
        mesh.modify(); mesh.set_static_mesh(original.static_mesh_component.static_mesh)
        mesh.set_editor_property('relative_scale3d', original.get_actor_scale3d())
        data = cdo.get_editor_property('ObjectActions'); data.modify()
        data.set_editor_property('ObjectName', name)
        actions = [unreal.AZ_ObjectAction.INSPECT]
        if takeable: actions.append(unreal.AZ_ObjectAction.TAKE)
        if name=='Note':
            actions.append(unreal.AZ_ObjectAction.READ)
            data.set_editor_property('FrontText','Meet me at the school. Use the back entrance.')
            data.set_editor_property('BackText','Keep this note.')
        data.set_editor_property('Actions',actions)
        bounds=mesh.static_mesh.get_bounding_box(); size=bounds.max-bounds.min
        axis=min(range(3),key=lambda i:[size.x,size.y,size.z][i])
        rotation=unreal.Rotator(90,0,0) if axis==2 else unreal.Rotator(0,90 if axis==1 else 180,0)
        data.set_editor_property('InitialRotation',rotation)
        if takeable:
            item=cdo.get_editor_property('Item'); item.modify()
            # Temporary existing neutral loot icon; final per-prop icon art is separate.
            text=ns['build_manifest'](name,'Item.Type.Consumable.Story.'+name,'Consumable',1,path,
                '/Game/InventorySystemPro/ExampleContent/Common/Art/Lootbag/T_LootBagIcon')
            manifest=item.get_editor_property('PickupItemManifest')
            manifest.import_text(text)
            assert 'Item.Type.Consumable.Story.'+name in manifest.export_text()
            item.set_editor_property('PickupCaption',name)
        assert unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()],False)
        outputs.append(path)
    print(json.dumps({'compile_these':outputs,'note':'Note reading text is prototype content, not accepted campaign lore.'}))


def place():
    world, subsystem, existing = scene()
    assert world.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Map has unrelated unsaved edits'
    backup=ROOT/'Saved/InteractionAudit'/('inspection-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/AZ/Maps/L_001.umap',backup/'L_001-before.umap')
    rows=[]
    for index,(name,label,takeable) in enumerate(ENTRIES):
        original=existing['AZ INTERACTION '+label]
        bp=unreal.load_asset(FOLDER+'/BP_Inspect_'+name)
        assert bp and bp.get_editor_property('status')==unreal.BlueprintStatus.BS_UP_TO_DATE
        actor=existing.get('AZ INSPECT '+name)
        if not actor:
            actor=subsystem.spawn_actor_from_class(bp.generated_class(),original.get_actor_location(),original.get_actor_rotation())
            assert actor
            actor.set_actor_label('AZ INSPECT '+name)
        actor.set_actor_transform(original.get_actor_transform(),False,True)
        actor.set_folder_path('AZ_Interaction_Showcase/Inspection')
        actor.set_editor_property('tags',['AZ_InspectionShowcase_v1'])
        if takeable:
            item=actor.get_editor_property('Item');item.modify();actor.modify()
            item.get_editor_property('CampaignPickupId').import_text('(A=1093309249,B=345459826,C=366035968,D='+str(1943117900+index)+')')
            # Re-read the class payload, retaining all typed manifest values.
            template=unreal.get_default_object(bp.generated_class()).get_editor_property('Item')
            item.set_editor_property('PickupItemManifest',template.get_editor_property('PickupItemManifest'))
            assert 'Item.Type.Consumable.Story.'+name in item.get_editor_property('PickupItemManifest').export_text()
        original.set_actor_hidden_in_game(True);original.set_actor_enable_collision(False)
        original.set_is_temporarily_hidden_in_editor(True);original.static_mesh_component.set_visibility(False,True)
        rows.append({'name':name,'actor':actor.get_path_name(),'takeable':takeable})
    controller=unreal.load_asset('/Game/AZ/Blueprints/Player/BP_AZ_PlayerController')
    assert controller.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Controller has unrelated changes'
    cdo=unreal.get_default_object(controller.generated_class())
    component=cdo.get_editor_property('Inspection');component.modify();cdo.modify()
    prompt=unreal.load_asset('/Game/AZ/Blueprints/Menu/Common/WBP_FN_ActionPrompt_Overlay')
    component.set_editor_property('PromptClass',prompt.generated_class())
    assert unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost(),controller.get_outermost()],False)
    (backup/'complete.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print('INSPECTION_SHOWCASE_SAVED objects=4; user_playtest_required=True')
