# @Description: Capture the current test level once on Slate's game-thread tick for map authoring.
"""No PIE or gameplay test. A single temporary capture actor is removed in finally."""
from pathlib import Path
import json
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'UI Design/CHALK_QuestMap_v01/sources/map'
RECEIPT = ROOT / 'Saved/QuestMapImplementation/map-capture-final.json'


def schedule():
    OUT.mkdir(parents=True, exist_ok=True)
    state = {}

    def capture_once(delta):
        unreal.unregister_slate_post_tick_callback(state['handle'])
        actor = None
        result = {'completed': False}
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        try:
            response = unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'IsPIERunning', '{}')
            assert response.is_complete and not response.error and not json.loads(response.value)['returnValue'], 'Play must be stopped'
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            assert world.get_path_name() == '/Game/AZ/Maps/L_001.L_001'
            actor = subsystem.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-1435, 3620, 12000),
                unreal.Rotator(pitch=-90, yaw=-90, roll=0), transient=True)
            assert actor
            actor.set_actor_label('AZ_QuestMap_TEMP_CAPTURE_FINAL')
            component = actor.get_component_by_class(unreal.SceneCaptureComponent2D)
            component.set_editor_property('capture_every_frame', False)
            component.set_editor_property('capture_on_movement', False)
            component.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
            component.set_editor_property('ortho_width', 12400.0)
            component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            target = unreal.RenderingLibrary.create_render_target2d(world, 2048, 2048,
                unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0, 0, 0, 1))
            component.set_editor_property('texture_target', target)
            for candidate in subsystem.get_all_level_actors():
                if candidate.get_components_by_class(unreal.SkeletalMeshComponent) or candidate.get_components_by_class(unreal.AZ_Inv_CommonUI_ItemComponent):
                    component.hide_actor_components(candidate)
            component.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), 'L001_FinalColor.png')
            result = {'completed': True, 'world': world.get_path_name(), 'origin': [-1435, 3620, 0],
                      'size': [12400, 12400], 'rotation': 0, 'flip_u': False, 'flip_v': False,
                      'camera_right': str(actor.get_actor_right_vector()), 'camera_up': str(actor.get_actor_up_vector()),
                      'image': str(OUT/'L001_FinalColor.png'), 'bytes': (OUT/'L001_FinalColor.png').stat().st_size}
        except Exception as error:
            result['error'] = str(error)
        finally:
            if actor:
                result['temporary_actor_removed'] = subsystem.destroy_actor(actor)
            RECEIPT.write_text(json.dumps(result, indent=2), encoding='utf-8')

    state['handle'] = unreal.register_slate_post_tick_callback(capture_once)
    return 'One capture scheduled on the next Slate tick; inspect map-capture-final.json.'
