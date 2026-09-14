"""Set a clean graphical review view in the newly opened holster-only window."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02')
target=root/'CHALK_Holster01_Rebuilt.blend'
assert Path(bpy.data.filepath).resolve()==target.resolve()
def frame_review():
    count=0
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active
                space.overlay.show_overlays=False
                space.shading.type='MATERIAL'
                space.shading.studiolight_rotate_z=1.0
                space.shading.studiolight_intensity=.7
                space.region_3d.view_location=Vector((-.020,.025,.123))
                space.region_3d.view_rotation=bpy.context.scene.camera.rotation_euler.to_quaternion()
                space.region_3d.view_distance=.72
                space.region_3d.view_perspective='ORTHO'
                space.region_3d.update()
                area.tag_redraw();count+=1
    if count==0:return .5
    bpy.ops.wm.save_as_mainfile(filepath=str(target),check_existing=False)
    (root/'review_opened.json').write_text(json.dumps({'file':bpy.data.filepath,'viewports_framed':count,'scene':bpy.context.scene.name},indent=2))
    return None
bpy.app.timers.register(frame_review,first_interval=1.0)
