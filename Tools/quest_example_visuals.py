# @Description: Add cosmetic, double-sided TEST signs to the seven owned quest example Blueprints.
"""Import is inert. Root executes ordered stages in the stopped editor:

    api_contract() / inspect()       # read-only
    prepare_material()              # create an owned plain dark-sage material
    # Root compiles/saves the material outside Python.
    add_components()                # new SCS components only; backed up first
    # Root compiles the seven Blueprints outside Python.
    configure()                     # set only our SCS component templates
    # Root compiles/saves the Blueprints outside Python.
    verify()                        # read-back; does not run gameplay

No compile/save/PIE calls, construction-script graph editing, actor placement,
native/inherited component mutation, or gameplay-property changes are hidden
here. The existing native DisplayMesh is deliberately left alone. All visuals
have NoCollision, no overlaps and no navigation contribution. The native root
remains the interaction/reach/checkpoint component.
"""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
spec = importlib.util.spec_from_file_location('az_quest_visual_content', ROOT / 'Tools/quest_content_setup.py')
C = importlib.util.module_from_spec(spec)
spec.loader.exec_module(C)
OUT = C.OUT / 'Visuals'
VISUAL_KEY, VISUAL_OWNER = 'AZ.Quest.ExampleVisuals', 'quest_example_visuals:v1'
VISUAL_STAGE = 'AZ.Quest.ExampleVisualStage'
COMPONENT_TAG = 'AZ.Quest.ExampleVisual'
MATERIAL = C.DEST + '/Art/M_AZ_QuestTestSign'
CUBE = '/Engine/BasicShapes/Cube'
LABELS = {
    'main_offer': ('TEST MAIN<br>[E] ACCEPT', 80.0),
    'main_reach': ('TEST REACH', 110.0),
    'main_use': ('TEST PANEL<br>[E] USE', 80.0),
    'optional_reach': ('OPTIONAL AREA', 110.0),
    'side_offer': ('TEST SIDE<br>[E] ACCEPT', 80.0),
    'side_delivery': ('TEST DELIVERY<br>[E] DELIVER', 80.0),
    'checkpoint': ('SAVE POINT<br>[E] SAVE', 100.0),
}
COMPONENTS = {
    'AZTest_SignBoard': 'StaticMeshComponent',
    'AZTest_SignPost': 'StaticMeshComponent',
    'AZTest_SignFoot': 'StaticMeshComponent',
    'AZTest_SignFront': 'TextRenderComponent',
    'AZTest_SignBack': 'TextRenderComponent',
}


def ue():
    return C.ue()


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')
    return value


def api_contract():
    """Safe first call for root to inspect the actual loaded Python signatures."""
    u = ue()
    return {'add_params': str(u.AddNewSubobjectParams.__doc__),
            'add': str(u.SubobjectDataSubsystem.add_new_subobject.__doc__),
            'rename': str(u.SubobjectDataSubsystem.rename_subobject.__doc__),
            'rename_variable': str(u.SubobjectDataSubsystem.rename_subobject_member_variable.__doc__),
            'associated': str(u.SubobjectDataBlueprintFunctionLibrary.get_associated_object.__doc__),
            'for_blueprint': str(u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint.__doc__),
            'horizontal_alignment': str(u.TextRenderComponent.set_horizontal_alignment.__doc__),
            'vertical_alignment': str(u.TextRenderComponent.set_vertical_alignment.__doc__)}


def blueprint(key):
    path, native = C.BLUEPRINTS[key]
    bp = C.owned(path)
    C.require(str(ue().EditorAssetLibrary.get_metadata_tag(bp, C.STAGE_KEY)) == 'configured', 'Configure example gameplay defaults first: ' + path)
    C.require(bp.generated_class() is not None, 'Compile the example Blueprint first: ' + path)
    C.require(isinstance(ue().get_default_object(bp.generated_class()), getattr(ue(), native)), 'Example native parent differs: ' + path)
    return bp


def gathered(bp):
    u = ue()
    subsystem = u.get_engine_subsystem(u.SubobjectDataSubsystem)
    C.require(subsystem is not None, 'SubobjectDataSubsystem is unavailable.')
    lib = u.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        if not lib.is_scene_component(data):
            continue
        is_native = lib.is_native_component(data)
        is_inherited = lib.is_inherited_component(data)
        # GetObjectForBlueprint can CREATE an inherited SCS override (engine
        # SubobjectData.cpp:256). Never request that for inherited/native nodes.
        obj = lib.get_object(data) if (is_native or is_inherited) else lib.get_object_for_blueprint(data, bp)
        if obj is None and not (is_native or is_inherited):
            obj = lib.get_associated_object(data)
        rows.append({'handle': handle, 'data': data, 'object': obj,
                     'name': str(lib.get_variable_name(data)),
                     'root': lib.is_root_component(data), 'native': is_native, 'inherited': is_inherited})
    roots = [row for row in rows if row['root']]
    C.require(len(roots) == 1 and roots[0]['object'] is not None, 'Expected one existing scene root: ' + bp.get_path_name())
    return subsystem, lib, roots[0], rows


def own_template(bp, row, expected_class):
    obj = row['object']
    C.require(obj is not None and isinstance(obj, getattr(ue(), expected_class)), 'Unexpected cosmetic component class: ' + row['name'])
    C.require(not row['native'] and not row['inherited'], 'Refusing inherited/native mutation: ' + row['name'])
    C.require(obj.get_package() == bp.get_package(), 'Component belongs to another package: ' + obj.get_path_name())
    C.require(COMPONENT_TAG in [str(tag) for tag in obj.get_editor_property('component_tags')], 'Component is not tagged as our visual: ' + row['name'])
    return obj


def inspect():
    C.idle()
    result = {}
    for key in LABELS:
        bp = blueprint(key)
        _, _, root, rows = gathered(bp)
        result[key] = {'blueprint': bp.get_path_name(), 'native_root': root['object'].get_path_name(),
                       'label': LABELS[key][0], 'actor_origin_above_floor': LABELS[key][1],
                       'components': [{'name': r['name'], 'object': r['object'].get_path_name() if r['object'] else None,
                                       'native': r['native'], 'inherited': r['inherited']} for r in rows]}
    return write('preflight', result)


def prepare_material():
    C.idle()
    u = ue()
    if u.EditorAssetLibrary.does_asset_exist(MATERIAL):
        mat = C.owned(MATERIAL)
        C.require(u.EditorAssetLibrary.get_metadata_tag(mat, VISUAL_KEY) == VISUAL_OWNER, 'Unowned sign material.')
        return {'material': MATERIAL, 'created': False, 'next': 'Root verifies/compiles/saves material externally.'}
    mat = u.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL.rsplit('/', 1)[1], MATERIAL.rsplit('/', 1)[0], u.Material, u.MaterialFactoryNew())
    C.require(mat is not None, 'Could not create sign material.')
    u.EditorAssetLibrary.set_metadata_tag(mat, C.OWNER_KEY, C.OWNER)
    u.EditorAssetLibrary.set_metadata_tag(mat, VISUAL_KEY, VISUAL_OWNER)
    mat.set_editor_property('blend_mode', u.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', False)
    # sRGB #26382E, converted to linear. The sign is a simple prototype surface.
    srgb = (38 / 255.0, 56 / 255.0, 46 / 255.0)
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in srgb]
    expression = u.MaterialEditingLibrary.create_material_expression(mat, u.MaterialExpressionVectorParameter, -260, 0)
    C.require(expression is not None, 'Could not add material colour parameter.')
    expression.set_editor_property('parameter_name', 'SignColor')
    expression.set_editor_property('default_value', u.LinearColor(*linear, 1.0))
    C.require(u.MaterialEditingLibrary.connect_material_property(expression, '', u.MaterialProperty.MP_EMISSIVE_COLOR), 'Could not connect sign colour.')
    return write('material-created', {'material': MATERIAL, 'created': True, 'color_srgb': '#26382E',
        'next': 'Root compiles/saves the material externally. No recompile_material/save call was made.'})


def add_components(keys=None):
    C.idle()
    keys = list(keys) if keys is not None else list(LABELS)
    C.require(keys and len(keys) == len(set(keys)) and set(keys) <= set(LABELS), 'Unknown or duplicate example keys.')
    u = ue()
    assets = {key: blueprint(key) for key in keys}
    # The root just saved these configured assets; this captures that exact starting point.
    backup = C.backup_packages([C.BLUEPRINTS[key][0] for key in keys], 'test-visual-components')
    result = {}
    for key, bp in assets.items():
        visual_owner = u.EditorAssetLibrary.get_metadata_tag(bp, VISUAL_KEY)
        C.require(not visual_owner or visual_owner == VISUAL_OWNER, 'Another author owns this visual stage.')
        _, _, _, existing = gathered(bp)
        existing_names = {row['name'] for row in existing}
        C.require(visual_owner or not (existing_names & set(COMPONENTS)), 'Name collision with an unowned component.')
        u.EditorAssetLibrary.set_metadata_tag(bp, VISUAL_KEY, VISUAL_OWNER)
        created = []
        for name, class_name in COMPONENTS.items():
            subsystem, lib, root, current = gathered(bp)
            found = [row for row in current if row['name'] == name]
            if found:
                C.require(len(found) == 1, 'Duplicate component variable: ' + name)
                own_template(bp, found[0], class_name)
                continue
            params = u.AddNewSubobjectParams()
            params.set_editor_property('parent_handle', root['handle'])
            params.set_editor_property('new_class', getattr(u, class_name).static_class())
            params.set_editor_property('blueprint_context', bp)
            params.set_editor_property('skip_mark_blueprint_modified', True)
            handle, reason = subsystem.add_new_subobject(params)
            C.require(lib.is_handle_valid(handle), 'Component add failed: ' + str(reason))
            C.require(subsystem.rename_subobject(handle, u.Text(name)), 'Component rename refused: ' + name)
            data = lib.get_data(handle)
            if str(lib.get_variable_name(data)) != name:
                subsystem.rename_subobject_member_variable(bp, handle, u.Name(name))
                data = lib.get_data(handle)
            C.require(str(lib.get_variable_name(data)) == name, 'Native component variable name differs: ' + name)
            obj = lib.get_object_for_blueprint(data, bp)
            C.require(obj is not None and obj.get_package() == bp.get_package() and not lib.is_native_component(data)
                      and not lib.is_inherited_component(data), 'New component template does not belong to this Blueprint.')
            obj.set_editor_property('component_tags', [u.Name(COMPONENT_TAG)])
            obj.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            obj.set_editor_property('generate_overlap_events', False)
            created.append(name)
        bp.modify()
        u.EditorAssetLibrary.set_metadata_tag(bp, VISUAL_STAGE, 'components')
        result[key] = {'blueprint': bp.get_path_name(), 'created': created}
    return write('components-added', {'backup': backup, 'assets': result, 'compile_after_return': [C.BLUEPRINTS[k][0] for k in keys],
        'next': 'Native Blueprint compile outside Python, then configure(keys). No native DisplayMesh was modified.'})


def transforms(origin_height):
    # Engine Cube is 100cm per side. Every sign foot ends at local Z=-origin_height.
    return {'AZTest_SignBoard': ((0, 0, 154 - origin_height), (0.08, 1.60, 0.72)),
            'AZTest_SignPost': ((0, 0, 67 - origin_height), (0.08, 0.08, 1.34)),
            'AZTest_SignFoot': ((0, 0, 2 - origin_height), (0.32, 0.32, 0.04))}


def configure(keys=None):
    C.idle()
    keys = list(keys) if keys is not None else list(LABELS)
    C.require(keys and len(keys) == len(set(keys)) and set(keys) <= set(LABELS), 'Unknown or duplicate example keys.')
    u = ue()
    cube, material = C.load(CUBE), C.owned(MATERIAL)
    C.require(isinstance(cube, u.StaticMesh), 'Expected Engine Cube static mesh.')
    C.require(u.EditorAssetLibrary.get_metadata_tag(material, VISUAL_KEY) == VISUAL_OWNER, 'Unowned sign material.')
    result = {}
    for key in keys:
        bp = blueprint(key)
        C.require(u.EditorAssetLibrary.get_metadata_tag(bp, VISUAL_KEY) == VISUAL_OWNER, 'Run the visual component stage first.')
        _, _, root, rows = gathered(bp)
        by_name = {row['name']: row for row in rows}
        label, origin_height = LABELS[key]
        parts = {name: own_template(bp, by_name[name], cls) for name, cls in COMPONENTS.items() if name in by_name}
        C.require(set(parts) == set(COMPONENTS), 'Visual components missing; compile the component stage first.')
        for obj in parts.values():
            obj.set_editor_property('can_ever_affect_navigation', False)
            obj.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            obj.set_editor_property('generate_overlap_events', False)
            obj.set_editor_property('cast_shadow', False)
            obj.set_editor_property('hidden_in_game', False)
            obj.set_editor_property('visible', True)
            obj.set_mobility(u.ComponentMobility.MOVABLE)
        for name, (position, scale) in transforms(origin_height).items():
            obj = parts[name]
            obj.set_static_mesh(cube)
            C.require(obj.get_editor_property('static_mesh') == cube, 'Could not assign cube: ' + name)
            obj.set_material(0, material)
            obj.set_editor_property('relative_location', u.Vector(*position))
            obj.set_editor_property('relative_rotation', u.Rotator(0, 0, 0))
            obj.set_editor_property('relative_scale3d', u.Vector(*scale))
        for name, x, yaw in (('AZTest_SignFront', 4.4, 0.0), ('AZTest_SignBack', -4.4, 180.0)):
            text = parts[name]
            text.set_text(u.Text(label))
            text.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
            text.set_vertical_alignment(u.VerticalTextAligment.EVRTA_TEXT_CENTER)
            text.set_world_size(16.0)
            text.set_text_render_color(u.Color(238, 234, 224, 255))
            text.set_editor_property('relative_location', u.Vector(x, 0, 154 - origin_height))
            text.set_editor_property('relative_rotation', u.Rotator(pitch=0, yaw=yaw, roll=0))
            text.set_editor_property('relative_scale3d', u.Vector(1, 1, 1))
            # Keep the native distance-field font/text material defaults, not a UMG runtime font.
            C.require(text.get_editor_property('font') is not None and text.get_editor_property('text_material') is not None,
                      'TextRender native font/material is absent; inspect before continuing.')
        bp.modify()
        u.EditorAssetLibrary.set_metadata_tag(bp, VISUAL_STAGE, 'configured')
        result[key] = {'blueprint': bp.get_path_name(), 'label': label, 'origin_height': origin_height,
                       'world_board_center_above_floor': 154.0, 'native_root_preserved': root['object'].get_path_name(),
                       'components': [obj.get_path_name() for obj in parts.values()]}
    return write('configured', {'assets': result, 'compile_after_return': [C.BLUEPRINTS[k][0] for k in keys],
        'next': 'Native compile/save seven Blueprints, then verify(). Root inspects the placed signs; no PIE required for appearance.'})


def verify():
    C.idle()
    u = ue()
    result = {}
    for key, (label, origin_height) in LABELS.items():
        bp = blueprint(key)
        C.require(u.EditorAssetLibrary.get_metadata_tag(bp, VISUAL_STAGE) == 'configured', 'Visual configuration is incomplete: ' + key)
        _, _, root, rows = gathered(bp)
        by_name = {row['name']: row for row in rows}
        parts = {}
        for name, cls in COMPONENTS.items():
            C.require(name in by_name, 'Missing visual component: ' + name)
            obj = own_template(bp, by_name[name], cls)
            C.require(obj.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION, 'Cosmetic collision changed: ' + name)
            C.require(not obj.get_editor_property('generate_overlap_events'), 'Cosmetic overlap generation changed: ' + name)
            C.require(not obj.get_editor_property('can_ever_affect_navigation'), 'Cosmetic navigation contribution changed: ' + name)
            position = obj.get_editor_property('relative_location')
            rotation = obj.get_editor_property('relative_rotation')
            parts[name] = {'object': obj.get_path_name(), 'location': [position.x, position.y, position.z],
                           'rotation': [rotation.pitch, rotation.yaw, rotation.roll],
                           'collision': str(obj.get_collision_enabled())}
            if isinstance(obj, u.TextRenderComponent):
                C.require(str(obj.get_editor_property('text')) == label, 'Text differs: ' + name)
                parts[name]['text'] = label
        C.require(abs(parts['AZTest_SignFront']['rotation'][1]) < 0.01 and abs(abs(parts['AZTest_SignBack']['rotation'][1]) - 180) < 0.01,
                  'Expected opposite +X/-X text faces.')
        result[key] = {'blueprint': bp.get_path_name(), 'native_root': root['object'].get_path_name(),
                       'origin_above_floor': origin_height, 'components': parts}
    return write('verified-readback', {'assets': result, 'saved_by_this_script': False, 'gameplay_verified': False})
