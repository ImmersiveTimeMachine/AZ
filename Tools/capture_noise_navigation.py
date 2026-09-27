"""Bounded read-only recorder for a user-run PIE session. Never starts or controls gameplay."""
import builtins
import datetime
import json
import math
from pathlib import Path
import time
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ/Saved/ThrowableCompletion')


def install():
    required = [(unreal.AIController, 'get_controlled_pawn'),
                (unreal.AIController, 'get_move_status'),
                (unreal.AIController, 'get_path_following_component'),
                (unreal.PathFollowingComponent, 'get_path_destination'),
                (unreal.CapsuleComponent, 'get_scaled_capsule_half_height')]
    for cls, method in required:
        if not hasattr(cls, method):
            raise RuntimeError('Unavailable diagnostic API: '+cls.__name__+'.'+method)
    previous = getattr(builtins, '_az_noise_navigation_probe', None)
    if previous and previous.get('handle') is not None:
        unreal.unregister_slate_post_tick_callback(previous['handle'])
    ROOT.mkdir(parents=True, exist_ok=True)
    output = ROOT / ('navigation-live-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '.jsonl')
    state = dict(handle=None, seen_play=False, last_poll=0.0, deadline=time.monotonic()+600,
                 count=0, signatures={}, output=str(output))
    builtins._az_noise_navigation_probe = state

    def write(row):
        with output.open('a', encoding='utf-8') as stream:
            stream.write(json.dumps(row, ensure_ascii=False)+'\n')

    def stop(reason):
        if state['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
        write(dict(event='stopped', reason=reason, samples=state['count']))

    def vec(v):
        return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)] if v else None

    def tick(_delta):
        now = time.monotonic()
        if now >= state['deadline'] or state['count'] >= 500:
            stop('limit'); return
        if now-state['last_poll'] < .2: return
        state['last_poll'] = now
        try:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:
                if state['seen_play']: stop('PIE ended')
                return
            state['seen_play'] = True
            building = unreal.NavigationSystemV1.is_navigation_being_built(world)
            for controller in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AZ_InfectedAIController):
                pawn = controller.get_controlled_pawn()
                bb = controller.get_editor_property('blackboard')
                if not pawn or not bb: continue
                goal = bb.get_value_as_vector('LastKnownLocation')
                if not all(math.isfinite(v) and abs(v)<1e7 for v in vec(goal)): continue
                position = pawn.get_actor_location()
                move = str(controller.get_move_status())
                signature = (tuple(vec(goal)), tuple(round(v/100) for v in vec(position)), move, building)
                name = pawn.get_name()
                if state['signatures'].get(name) == signature: continue
                state['signatures'][name] = signature
                start = unreal.Vector(position.x, position.y, position.z)
                capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
                if capsule: start.z -= capsule.get_scaled_capsule_half_height()
                projected = unreal.NavigationSystemV1.project_point_to_navigation(world, goal, None, controller.default_navigation_filter_class)
                projected_start = unreal.NavigationSystemV1.project_point_to_navigation(world, start, None, controller.default_navigation_filter_class)
                path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, goal, controller, controller.default_navigation_filter_class)
                pf = controller.get_path_following_component()
                row = dict(event='sample', elapsed=round(now-(state['deadline']-600),2), pawn=name,
                           position=vec(position), start=vec(start), goal=vec(goal), move=move,
                           building=building, projected=vec(projected), projected_start=vec(projected_start),
                           current_destination=vec(controller.get_immediate_move_destination()) if pf else None,
                           path_valid=path.is_valid() if path else False,
                           path_partial=path.is_partial() if path else None,
                           path_points=[vec(v) for v in path.path_points] if path else [])
                write(row); state['count'] += 1
        except Exception as error:
            write(dict(event='error', error=str(error)))
            stop('error')

    write(dict(event='installed', limit_seconds=600, limit_samples=500))
    state['handle'] = unreal.register_slate_post_tick_callback(tick)
    print('NAVIGATION_RECORDER_READY '+str(output))


if __name__ == '__main__':
    install()
