"""Create CHALK pistol holster option 01 in a fresh Blender 5.2 scene.

The source concept remains an image reference. This script creates a new editable
model and never opens, changes or saves the user's live Blender scene. The asset
is a wearable-prototype collection plus a separate review-only thigh dummy.
"""

from pathlib import Path
import hashlib
import json
import math

import bpy
from mathutils import Vector


ROOT = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v01')
OUTPUT = ROOT / 'CHALK_PistolHolster_01_CanvasThigh.blend'
PREVIEW = ROOT / 'CHALK_PistolHolster_01_CanvasThigh_Preview.png'
MANIFEST = ROOT / 'CHALK_PistolHolster_01_CanvasThigh_manifest.json'
REFERENCE = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Pistol_Holster_v01/concepts/01_Thigh_Canvas_Holster.png')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def rgba(hex_value):
    value = hex_value.removeprefix('#')
    return tuple(int(value[index:index + 2], 16) / 255 for index in (0, 2, 4)) + (1.0,)


def material(name, hex_value, roughness=.65, metallic=0.0):
    item = bpy.data.materials.new(name)
    item.use_nodes = True
    node = item.node_tree.nodes.get('Principled BSDF')
    node.inputs['Base Color'].default_value = rgba(hex_value)
    node.inputs['Roughness'].default_value = roughness
    node.inputs['Metallic'].default_value = metallic
    return item


MATS = {
    'Canvas Charcoal': material('PH01_M_Canvas_Charcoal', '#272B29', .77),
    'Canvas Edge': material('PH01_M_Canvas_EdgeBinding', '#151817', .74),
    'Olive Webbing': material('PH01_M_Olive_Webbing', '#4E5540', .80),
    'Blue Repair': material('PH01_M_Blue_RepairPatch', '#405775', .77),
    'Stitching': material('PH01_M_Saddle_Stitching', '#A29378', .68),
    'Hardware': material('PH01_M_Blackened_Hardware', '#1A1B1A', .32, .78),
    'Snap Metal': material('PH01_M_Dull_Snap_Metal', '#646765', .30, .87),
    'Interior': material('PH01_M_Dark_Interior', '#0A0B0B', .92),
    'Review Leg': material('PH01_M_Review_Thigh', '#4D5450', .80),
    'Floor': material('PH01_M_Review_Floor', '#151817', .88),
}


def link(collection, obj):
    collection.objects.link(obj)
    return obj


def apply_material(obj, mat):
    obj.data.materials.append(mat)


def bevel(obj, width, segments=3):
    mod = obj.modifiers.new('Soft edges / editable', 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    return mod


def box(collection, name, location, dimensions, mat, radius=.002):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    link(collection, obj)
    apply_material(obj, mat)
    if radius:
        bevel(obj, radius)
    return obj


def cylinder(collection, name, location, radius, depth, mat, rotation=(math.pi / 2, 0, 0), vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                       location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    link(collection, obj)
    apply_material(obj, mat)
    bevel(obj, .0015, 2)
    return obj


def tapered_panel(collection, name, y_center, depth, points, mat, bevel_width=.004):
    """Create a closed tapered canvas panel, points ordered in X/Z plane."""
    front_y = y_center - depth / 2
    back_y = y_center + depth / 2
    vertices = [(x, front_y, z) for x, z in points] + [(x, back_y, z) for x, z in points]
    count = len(points)
    faces = [tuple(range(count)), tuple(range(count, count * 2))]
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, following + count, index + count))
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(mat)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    link(collection, obj)
    bevel(obj, bevel_width, 4)
    return obj


def curve_path(collection, name, points, mat, bevel_depth=.0017, cyclic=False):
    curve = bpy.data.curves.new(name + '_Curve', 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 2
    curve.bevel_depth = bevel_depth
    curve.bevel_resolution = 2
    spline = curve.splines.new('POLY')
    spline.points.add(len(points) - 1)
    for point, coordinate in zip(spline.points, points):
        point.co = (*coordinate, 1)
    spline.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, curve)
    link(collection, obj)
    curve.materials.append(mat)
    return obj


def ribbon_arc(collection, name, z, radius, start, end, webbing_width, mat, segments=30):
    """Flat webbing arc around the review thigh, with editable Solidify/Bevel."""
    vertices, faces = [], []
    for index in range(segments + 1):
        theta = start + (end - start) * index / segments
        x, y = radius * math.sin(theta), -radius * math.cos(theta)
        vertices.extend([(x, y, z - webbing_width / 2), (x, y, z + webbing_width / 2)])
    for index in range(segments):
        a = index * 2
        faces.append((a, a + 1, a + 3, a + 2))
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(mat)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    link(collection, obj)
    solid = obj.modifiers.new('Webbing thickness / editable', 'SOLIDIFY')
    solid.thickness = .004
    solid.offset = 0
    bevel(obj, .0012, 2)
    return obj


def frame(collection, name, location, horizontal, vertical, depth, bar, mat):
    """Small hardware rectangle in the X/Z plane, made of four round rails."""
    x, y, z = location
    rail_radius = bar / 2
    curve_path(collection, name + ' / left rail', [(x - horizontal / 2, y, z - vertical / 2),
                                                    (x - horizontal / 2, y, z + vertical / 2)], mat, rail_radius)
    curve_path(collection, name + ' / right rail', [(x + horizontal / 2, y, z - vertical / 2),
                                                     (x + horizontal / 2, y, z + vertical / 2)], mat, rail_radius)
    curve_path(collection, name + ' / upper rail', [(x - horizontal / 2, y, z + vertical / 2),
                                                     (x + horizontal / 2, y, z + vertical / 2)], mat, rail_radius)
    curve_path(collection, name + ' / lower rail', [(x - horizontal / 2, y, z - vertical / 2),
                                                     (x + horizontal / 2, y, z - vertical / 2)], mat, rail_radius)
    return depth


def stitch_run(collection, name, points, mat):
    for index, (x, y, z) in enumerate(points):
        stitch = box(collection, name + ' / %02d' % (index + 1), (x, y, z), (.014, .0015, .0023), mat, .0006)
        stitch.rotation_euler[1] = math.radians(0 if index % 2 else 3)


def look_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def add_light(collection, name, location, energy, size, tint):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    data.color = tint
    obj = bpy.data.objects.new(name, data)
    link(collection, obj)
    obj.location = location
    look_at(obj, (0, -.12, -.12))
    return obj


def object_record(obj):
    return {'name': obj.name, 'type': obj.type,
            'vertices': len(obj.data.vertices) if obj.type == 'MESH' else None,
            'material_slots': [slot.material.name if slot.material else None for slot in obj.material_slots]}


require(not OUTPUT.exists(), 'Output already exists; refusing to replace a model: ' + str(OUTPUT))
require(not PREVIEW.exists(), 'Preview already exists; refusing to replace it: ' + str(PREVIEW))
require(REFERENCE.is_file(), 'Missing approved option 01 concept: ' + str(REFERENCE))
ROOT.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for collection in list(bpy.data.collections):
    if collection.name != 'Collection':
        bpy.data.collections.remove(collection)

scene = bpy.context.scene
scene.name = 'CHALK_PistolHolster_01'
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 1500
scene.render.resolution_y = 1500
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(PREVIEW)
scene.render.film_transparent = False
scene.world.color = (0.035, 0.042, 0.040)

model = bpy.data.collections.new('PH01_Model / Canvas thigh holster')
details = bpy.data.collections.new('PH01_Details / Stitching and hardware')
review = bpy.data.collections.new('PH01_ReviewOnly / Fit dummy, lights and camera')
scene.collection.children.link(model)
scene.collection.children.link(details)
scene.collection.children.link(review)

root = bpy.data.objects.new('PH01_Root_RightThigh', None)
root.empty_display_type = 'ARROWS'
root.empty_display_size = .08
root['purpose'] = 'Root at right-thigh attachment height; model faces negative Y in Blender coordinates.'
link(model, root)

# Main waxed-canvas pocket: the silhouette is option 01, with a deliberately
# slim panel and a deep open mouth for the existing compact pistol asset.
body_outline = [(-.078, .028), (.078, .028), (.070, -.205), (.042, -.270),
                (-.042, -.270), (-.070, -.205)]
body = tapered_panel(model, 'PH01_HolsterBody / Waxed canvas shell', -.164, .020,
                     body_outline, MATS['Canvas Charcoal'])
body.parent = root

front_panel_outline = [(-.062, .010), (.062, .010), (.055, -.187), (.031, -.237),
                       (-.031, -.237), (-.055, -.187)]
front = tapered_panel(model, 'PH01_FrontPanel / Reinforced canvas', -.178, .005,
                      front_panel_outline, MATS['Canvas Charcoal'], .0025)
front.parent = root

top_binding = box(model, 'PH01_MouthBinding / Dark canvas', (0, -.179, .022), (.152, .010, .012),
                   MATS['Canvas Edge'], .002)
top_binding.parent = root
inner = box(model, 'PH01_InteriorShadow / Open pocket', (0, -.182, -.001), (.118, .009, .052),
            MATS['Interior'], .002)
inner.parent = root

# Backplate stabilizes the body against the thigh without pretending it is
# a hard military Kydex shell.
backplate = box(model, 'PH01_Backplate / Soft thigh pad', (0, -.143, -.112), (.135, .010, .230),
                MATS['Canvas Edge'], .006)
backplate.parent = root

# Two genuine webbing bands wrap the thigh. The body covers the central front
# portion, making the straps read as belt-to-body attachments rather than rings.
upper = ribbon_arc(model, 'PH01_UpperThighStrap / Olive webbing', -.075, .157,
                   math.radians(-155), math.radians(155), .032, MATS['Olive Webbing'])
upper.parent = root
lower = ribbon_arc(model, 'PH01_LowerThighStrap / Olive webbing', -.208, .151,
                   math.radians(-155), math.radians(155), .032, MATS['Olive Webbing'])
lower.parent = root

for z, label in [(-.075, 'Upper'), (-.208, 'Lower')]:
    for side in (-1, 1):
        arm = box(model, 'PH01_%sStrapConnector / %s' % (label, 'L' if side < 0 else 'R'),
                  (side * .092, -.174, z), (.052, .007, .030), MATS['Olive Webbing'], .0015)
        arm.parent = root
        frame(details, 'PH01_%sAdjustmentFrame / %s' % (label, 'L' if side < 0 else 'R'),
              (side * .110, -.180, z), .023, .020, .006, .004, MATS['Hardware'])

# Vertical belt stabilizer: the holster is constrained at the hip but still
# rides on its two leg straps, matching the chosen concept exactly.
belt = box(model, 'PH01_BeltStabilizer / Olive webbing', (.030, -.164, .115), (.031, .008, .176),
           MATS['Olive Webbing'], .0015)
belt.parent = root
frame(details, 'PH01_BeltD_Ring / Hardware', (.030, -.173, .195), .030, .027, .006, .004, MATS['Hardware'])

# Retention strap and single snap: short enough to permit a direct right-hand draw.
tab = box(model, 'PH01_RetentionTab / Olive webbing', (-.006, -.192, .071), (.032, .008, .100),
          MATS['Olive Webbing'], .002)
tab.parent = root
snap = cylinder(details, 'PH01_RetentionSnap / Dull metal', (-.006, -.198, .019), .009, .004,
                MATS['Snap Metal'])
snap.parent = root

# Blue repair patch and its bright but restrained stitched border.
patch = box(details, 'PH01_BlueRepairPatch / Canvas', (.041, -.185, -.192), (.037, .003, .054),
            MATS['Blue Repair'], .001)
patch.parent = root
for x in (.025, .057):
    for z in (-.215, -.169):
        stitch = box(details, 'PH01_BluePatchStitch / %.3f / %.3f' % (x, z), (x, -.188, z),
                     (.006, .0016, .0016), MATS['Stitching'], .0003)
        stitch.parent = root

# Clean saddle-stitch runs around the visible reinforcement panel.
left_points = [(-.060, -.184, z) for z in (.000, -.026, -.052, -.078, -.104, -.130, -.156, -.182)]
right_points = [(.060, -.184, z) for z in (.000, -.026, -.052, -.078, -.104, -.130, -.156, -.182)]
stitch_run(details, 'PH01_LeftSaddleStitch', left_points, MATS['Stitching'])
stitch_run(details, 'PH01_RightSaddleStitch', right_points, MATS['Stitching'])
stitch_run(details, 'PH01_BottomSaddleStitch', [(x, -.184, -.226) for x in (-.026, -.013, 0, .013, .026)], MATS['Stitching'])

# Soft rivets only at functional load points, not decoration overload.
for x, z in [(-.064, .004), (.064, .004), (-.062, -.177), (.062, -.177), (0, .012)]:
    item = cylinder(details, 'PH01_LoadRivet / %.3f / %.3f' % (x, z), (x, -.190, z), .0044, .004,
                    MATS['Snap Metal'], vertices=16)
    item.parent = root

# This simple retained geometry is intentionally a non-rendered fit envelope,
# not a substitute model for the existing pistol asset.
guide = box(model, 'PH01_PistolFitGuide / hidden', (0, -.198, .055), (.105, .027, .145), MATS['Interior'], .003)
guide.parent = root
guide.hide_render = True
guide.hide_viewport = True
guide['purpose'] = 'Sizing guide only; replace with the existing AZ pistol mesh during attachment work.'

# Review-only tapered thigh makes strap wrap and proportions inspectable.
bpy.ops.mesh.primitive_cone_add(vertices=64, radius1=.122, radius2=.145, depth=.60,
                                location=(0, 0, -.125))
leg = bpy.context.object
leg.name = 'REVIEW_ThighDummy / not part of asset'
for owner in list(leg.users_collection):
    owner.objects.unlink(leg)
link(review, leg)
apply_material(leg, MATS['Review Leg'])
bevel(leg, .008, 3)

# Create a floor and studio camera as review-only scene fixtures.
bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0, -.435))
floor = bpy.context.object
floor.name = 'REVIEW_Floor / not part of asset'
for owner in list(floor.users_collection):
    owner.objects.unlink(floor)
link(review, floor)
apply_material(floor, MATS['Floor'])

camera_data = bpy.data.cameras.new('REVIEW_Camera')
camera = bpy.data.objects.new('REVIEW_Camera', camera_data)
link(review, camera)
camera.location = (.46, -.72, .12)
camera_data.lens = 56
look_at(camera, (0, -.11, -.105))
scene.camera = camera
add_light(review, 'REVIEW_Key / overcast', (-.46, -.58, .54), 760, .45, (.82, .88, 1.0))
add_light(review, 'REVIEW_Fill / neutral', (.48, -.22, .12), 420, .35, (1.0, .90, .78))
add_light(review, 'REVIEW_Rim / cool', (.12, .38, .42), 600, .28, (.72, .82, 1.0))

# Pack the source concept image into this derived file for modelling reference.
image = bpy.data.images.load(str(REFERENCE), check_existing=False)
image.name = 'REFERENCE_Option01_CanvasThighConcept'
image.pack()

readme = bpy.data.texts.new('README_PistolHolster_Option01.txt')
readme.write('''CHALK / PISTOL HOLSTER / OPTION 01 — CANVAS THIGH\n\n'
                'Model: canvas thigh holster from the approved 01 concept.\n'
                'Core asset collections: PH01_Model and PH01_Details.\n'
                'Review-only collection: PH01_ReviewOnly. Do not export its thigh, floor, camera or lights.\n'
                'Root: PH01_Root_RightThigh. It represents the thigh attachment height.\n'
                'The model is deliberately unrigged. It will be fitted to the hero/thigh_r in the next attachment pass.\n'
                'PistolFitGuide is hidden and only establishes pocket volume; use the existing AZ pistol asset later.\n'
                'Concept reference is packed as REFERENCE_Option01_CanvasThighConcept.\n'
                'No backpack, pistol, skeleton or Unreal asset was altered by this Blender model build.\n''')

def collect_objects(collection):
    rows = []
    for obj in collection.objects:
        rows.append(object_record(obj))
    for child in collection.children:
        rows.extend(collect_objects(child))
    return rows

model_objects = collect_objects(model) + collect_objects(details)
manifest = {
    'asset': 'CHALK Pistol Holster Option 01 / Canvas Thigh',
    'concept_reference': str(REFERENCE),
    'concept_sha256': hashlib.sha256(REFERENCE.read_bytes()).hexdigest(),
    'output_blend': str(OUTPUT),
    'preview_render': str(PREVIEW),
    'units': 'meters',
    'root': root.name,
    'wearable_state': 'unrigged authored model; intended for later right-thigh socket fit',
    'model_collections': [model.name, details.name],
    'review_collection': review.name,
    'model_object_count': len(model_objects),
    'model_objects': model_objects,
    'materials': {name: material.name for name, material in MATS.items()},
    'components': [
        'tapered waxed-canvas holster body', 'soft thigh pad', 'two olive thigh straps',
        'belt stabilizer and D-ring', 'retention tab with snap', 'saddle stitching',
        'blue repair patch', 'load rivets and adjustment frames'
    ],
    'review_only_objects': [obj.name for obj in review.objects],
    'hidden_fit_guide': guide.name,
}
MANIFEST.write_text(json.dumps(manifest, indent=2), encoding='utf-8')

scene.render.filepath = str(PREVIEW)
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), check_existing=False)
bpy.ops.render.render(write_still=True)

require(OUTPUT.is_file() and PREVIEW.is_file() and MANIFEST.is_file(), 'Expected Blender deliverables were not created')
print(json.dumps({'output': str(OUTPUT), 'preview': str(PREVIEW), 'model_object_count': len(model_objects),
                  'reference_sha256': manifest['concept_sha256'], 'components': manifest['components']}))
