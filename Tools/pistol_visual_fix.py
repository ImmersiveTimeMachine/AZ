"""Repair only pistol visual references using UE5.8 native mesh setters.

Run main('author'), compile its five Blueprints through the native editor tool,
save those packages and L_001, then main('verify'). No gameplay or PIE is started.
"""
import gc
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/PistolVisualFix'


def main(mode='verify', resume=False):
    try:
        assert mode in ('author', 'verify')
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, 'Finish Play before modifying editor visuals'
        helpers = runpy.run_path(str(ROOT / 'Tools/pistol_inventory_setup.py'), run_name='pistol_visual_helpers')
        paths = [helpers['WEAPON'], helpers['PICKUP']] + [p for p, _ in helpers['MAGAZINES']]
        blueprints = [helpers['owned_or_missing'](p) for p in paths]
        assert all(blueprints)
        classes = {bp.generated_class(): p for bp, p in zip(blueprints, paths)}
        weapon_mesh = unreal.load_asset(helpers['MESH'])
        pickup_mesh = unreal.load_asset(helpers['PICKUP_MESH'])
        magazine_mesh = unreal.load_asset(helpers['MAGAZINE_MESH'])
        assert weapon_mesh and pickup_mesh and magazine_mesh
        OUTPUT.mkdir(parents=True, exist_ok=True)
        before_inventory = helpers['capture'](helpers['helpers']())
        if mode == 'author':
            dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            assert resume or not dirty.intersection(paths), 'Pistol assets have unsaved changes'
            backup = ROOT / 'Saved/Backups/PistolVisualFix' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup.mkdir(parents=True)
            for path in paths + ['/Game/AZ/Maps/L_001']:
                file = ROOT / 'Content' / (path.removeprefix('/Game/') + ('.umap' if path.endswith('/L_001') else '.uasset'))
                assert file.is_file(), str(file)
                shutil.copy2(file, backup / file.name)
            (backup / 'inventory-before.json').write_text(json.dumps(before_inventory, indent=2))
            for bp in blueprints:
                bp.modify()
        rows = []
        # Include CDOs, persistent placed actors, and already-created thumbnail
        # actors. Their editor alias may be correct while actual SkinnedAsset is stale.
        for actor in unreal.ObjectIterator(unreal.Actor):
            path = classes.get(actor.get_class())
            if path is None:
                continue
            meshes = []
            for component in actor.get_components_by_class(unreal.MeshComponent):
                expected = None
                if isinstance(component, unreal.SkeletalMeshComponent):
                    if path == helpers['WEAPON'] and component.get_name() == 'WeaponMesh3P':
                        expected = weapon_mesh
                    if mode == 'author':
                        component.modify()
                        component.set_skeletal_mesh_asset(expected)
                    actual = component.get_skeletal_mesh_asset()
                    assert actual == expected, 'Actual skeletal mesh mismatch: ' + component.get_path_name()
                elif isinstance(component, unreal.StaticMeshComponent):
                    if path != helpers['WEAPON'] and component.get_name() == 'Mesh':
                        expected = pickup_mesh if path == helpers['PICKUP'] else magazine_mesh
                    if mode == 'author':
                        component.modify()
                        component.set_static_mesh(expected)
                    # StaticMesh is the actual property; only the skeletal editor
                    # alias needs the separate native getter in UE5.8.
                    actual = component.get_editor_property('static_mesh')
                    assert actual == expected, 'Actual static mesh mismatch: ' + component.get_path_name()
                else:
                    continue
                meshes.append({'component': component.get_path_name(), 'mesh': actual.get_path_name() if actual else None})
            rows.append({'actor': actor.get_path_name(), 'blueprint': path, 'meshes': meshes})
        after_inventory = helpers['capture'](helpers['helpers']())
        # Deliberately leave item identities, magazine counts and combat tuning alone.
        for before, after in zip(before_inventory, after_inventory):
            for field in ('manifest', 'contained', 'prompt', 'input', 'required', 'sockets', 'weapon_tag', 'mesh_fire'):
                assert before.get(field) == after.get(field), 'Nonvisual field changed: ' + field
        report = {'mode': mode, 'actors': rows, 'blueprints': paths,
                  'compile_after_return': [p + '.' + p.rsplit('/', 1)[1] for p in paths]}
        if mode == 'author':
            report['backup'] = str(backup)
        (OUTPUT / (mode + '.json')).write_text(json.dumps(report, indent=2))
        print('PISTOL_VISUALS ' + json.dumps({'mode': mode, 'actor_count': len(rows), 'blueprints': paths}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('verify')
