import bpy, json
from pathlib import Path
from mathutils import Vector
root=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.lens=50
            space.clip_start=.01
            space.region_3d.view_distance=1.3
            space.region_3d.view_location=Vector((0,.16,1.10))
            space.region_3d.update()
            area.tag_redraw()
bpy.ops.wm.save_as_mainfile(filepath=str(root/'Backpack2_Separated.blend'),check_existing=False)
report=json.loads((root/'inspection/separation_report.json').read_text())
print(json.dumps({'file':bpy.data.filepath,'parts':len(report['parts']),
                  'total_faces':sum(p['faces'] for p in report['parts']),
                  'normals_uv_weights_topology_exact':all(p['normals_bit_exact'] and p['uv_weights_materials_topology_exact'] for p in report['parts']),
                  'world_max_error':max(p['world_max_component_error'] for p in report['parts'])}))
