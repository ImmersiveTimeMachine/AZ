"""Offline pivot derivation. Run with Blender's bundled Python (NumPy).

Reads preserved geometry only; does not open or modify Blender/Unreal assets.
"""
import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
FILES = ['source_geometry.json', 'source_topology.json', 'logical_parts.json']
HASHES = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in FILES}
geometry, topology, mapping = [json.loads((ROOT / name).read_text(encoding='utf-8-sig')) for name in FILES]
P = np.asarray(geometry['positions'], dtype=float)
POLYS = geometry['polygons']
COMPONENTS = topology['parts']


def normalized(v):
    v = np.asarray(v, dtype=float)
    return v / np.linalg.norm(v)


def component_points(ids):
    return P[sorted({v for c in ids for v in COMPONENTS[c]['vertices']})]


def part_ids(name):
    return sorted({v for f in mapping['parts'][name] for v in POLYS[f]})


def principal_axes(points):
    return np.linalg.eigh(np.cov(points.T))[1]


def bounds_midpoint(points, rotation, base=False):
    projected = points @ rotation.T
    low, high = projected.min(axis=0), projected.max(axis=0)
    center = (low + high) * 0.5
    if base:
        center[2] = low[2]
    return rotation.T @ center


def fan_center(component):
    counts = {}
    for f in COMPONENTS[component]['faces']:
        for v in POLYS[f]:
            counts[v] = counts.get(v, 0) + 1
    index = max(counts, key=counts.get)
    return index, P[index]


def axis_frame(z, forward):
    z = normalized(z)
    x = normalized(np.asarray(forward) - z * np.dot(forward, z))
    y = normalized(np.cross(z, x))
    return np.stack([x, y, z])


manifest = {
    'schema_version': 1,
    'source_object': topology['object'],
    'source_coordinate_space': 'Preserved Blender world coordinates, meters; source geometry.positions, NOT local_positions.',
    'target_coordinate_space': 'Standalone item mesh coordinates, meters. X forward, Z up, Y completes the proper right-handed rotation before Unreal conversion.',
    'transform_convention': 'Column vectors: p_item = rotation_rows @ (p_source_world - origin_world_m). Rows are item axes expressed in source world.',
    'inverse_convention': 'p_source_world = rotation_rows.T @ p_item + origin_world_m.',
    'requested_unreal_conversion': 'After the rigid transform, root imports centimeters as (p_item.x, -p_item.y, p_item.z) * 100.',
    'source_geometry_edited': False,
    'source_input_sha256': HASHES,
    'parts': {},
}
preview = {'parts': {}}


def add(name, rotation, pivot, semantic, confidence, rationale, landmarks=None):
    points = P[part_ids(name)]
    rotation, pivot = np.asarray(rotation), np.asarray(pivot)
    transformed = (points - pivot) @ rotation.T
    matrix = np.eye(4)
    matrix[:3, :3] = rotation
    matrix[:3, 3] = -rotation @ pivot
    determinant = float(np.linalg.det(rotation))
    orthogonality = float(np.max(np.abs(rotation @ rotation.T - np.eye(3))))
    if abs(determinant - 1) > 1e-10 or orthogonality > 1e-10:
        raise ValueError('Invalid proper rotation: ' + name)
    entry = {
        'origin_world_m': pivot.tolist(),
        'rotation_rows': rotation.tolist(),
        'source_world_to_item_4x4': matrix.tolist(),
        'pivot_description': semantic,
        'orientation_reasoning': rationale,
        'confidence': confidence,
        'source_component_ids': mapping['components'][name],
        'source_face_count': len(mapping['parts'][name]),
        'item_bounds_min': transformed.min(axis=0).tolist(),
        'item_bounds_max': transformed.max(axis=0).tolist(),
        'item_dimensions_m': np.ptp(transformed, axis=0).tolist(),
        'rotation_determinant': determinant,
        'orthogonality_max_error': orthogonality,
    }
    if landmarks:
        entry['landmarks'] = landmarks
    manifest['parts'][name.removeprefix('BP2_')] = entry
    # Compact geometry for independent projection rendering under system Python/Pillow.
    ids = part_ids(name)
    index = {v: i for i, v in enumerate(ids)}
    preview['parts'][name] = {
        'points': transformed.tolist(),
        'faces': [[index[v] for v in POLYS[f]] for f in mapping['parts'][name]],
        'face_components': [next(c for c in mapping['components'][name] if f in COMPONENTS[c]['faces']) for f in mapping['parts'][name]],
        'pivot_semantic': semantic,
    }


# Axe: long wood-grip axis toward head -> +Z. Flat metal-face normal -> Y.
# Broad cutting end is source-world +Y, opposite the narrow rear spike.
wood = component_points([11, 167, 172])
metal = component_points([133, 150])
axe_z = principal_axes(wood)[:, -1]
if axe_z[2] < 0:
    axe_z = -axe_z
axe_y = principal_axes(metal)[:, 0]
axe_y = normalized(axe_y - axe_z * np.dot(axe_y, axe_z))
axe_x = normalized(np.cross(axe_y, axe_z))
if axe_x[1] < 0:
    axe_x, axe_y = -axe_x, -axe_y
axe_r = np.stack([axe_x, axe_y, axe_z])
axe_pivot = bounds_midpoint(wood, axe_r)
head = (metal - axe_pivot) @ axe_r.T
head = head[head[:, 2] > .18]
cut = head[head[:, 0] > .06]
spike = head[head[:, 0] < -.06]
add('BP2_Axe', axe_r, axe_pivot, 'Center of the wooden grip side plates; inside the handhold.', 'high',
    'Grip principal length axis points toward the metal head as +Z. Thin metal-face normal becomes Y. Broad cutting edge points +X; narrow rear spike points -X.',
    {'wood_components': [11, 167, 172], 'metal_components': [133, 150],
     'cutting_end_z_span_m': float(np.ptp(cut[:, 2])), 'rear_spike_z_span_m': float(np.ptp(spike[:, 2]))})


# Knife: source metal blade ends at a deliberately preserved truncated opening.
tip_ids = [1333, 1334, 1335, 1337, 1340]
guard_ids = [1325, 1326, 1351, 1352, 1358, 1378]
tip, guard = P[tip_ids].mean(axis=0), P[guard_ids].mean(axis=0)
knife_x = normalized(tip - guard)
knife_y = principal_axes(component_points([29, 70]))[:, 0]
knife_y = normalized(knife_y - knife_x * np.dot(knife_y, knife_x))
if knife_y[0] < 0:
    knife_y = -knife_y
knife_z = normalized(np.cross(knife_x, knife_y))
knife_r = np.stack([knife_x, knife_y, knife_z])
knife_pivot = bounds_midpoint(component_points([17, 121, 161]), knife_r)
add('BP2_Knife', knife_r, knife_pivot, 'Center of the wooden and blue-wrapped grip.', 'high',
    'Blade root-to-truncated-tip direction is +X. Least-variance metal normal is projected perpendicular to that axis, making the broad blade faces approximately +/-Y. Existing blade-tip opening is preserved.',
    {'grip_components': [17, 121, 161], 'blade_components': [29, 70],
     'blade_tip_source_vertices': tip_ids, 'guard_source_vertices': guard_ids,
     'blade_tip_center_source_world': tip.tolist(), 'guard_center_source_world': guard.tolist()})


# Bottle: the base and cap are actual 20-triangle fans, giving exact axis landmarks.
base_id, base_center = fan_center(175)
cap_id, cap_center = fan_center(177)
bottle_r = axis_frame(cap_center - base_center, [1, 0, 0])
bottle_points = P[part_ids('BP2_Bottle')]
base_shift = float(((bottle_points - base_center) @ bottle_r.T)[:, 2].min())
bottle_pivot = base_center + bottle_r[2] * base_shift
add('BP2_Bottle', bottle_r, bottle_pivot, 'Bottom center on the bottle axis, at the lowest surface.', 'high',
    'Actual base-fan center to cap-fan center becomes upright +Z. Forward +X follows projected source-world +X, normal to the broad bottle faces. Pivot is moved along the bottle axis only if needed to put the lowest geometry on Z=0.',
    {'base_fan_component': 175, 'cap_fan_component': 177,
     'base_center_vertex': base_id, 'cap_center_vertex': cap_id,
     'base_center_source_world': base_center.tolist(), 'cap_center_source_world': cap_center.tolist(),
     'base_floor_correction_m': base_shift})


# Soft accessory parts use manipulation pivots without reshaping source geometry.
forward_bag_r = axis_frame([0, 0, 1], [0, 1, 0])
backpack_body = component_points([78, 125])
add('BP2_Backpack', forward_bag_r, bounds_midpoint(backpack_body, forward_bag_r),
    'Center of the main bag body and top flap, excluding shoulder harness and dangling strap extents.', 'high',
    'Keep source upright direction +Z and face the visible outward bag side (source +Y) toward item +X. Use the combined main-body/top-flap bounds center for natural manipulation; long shoulder straps and hanging strap tips do not displace the pivot. The preserved assembled set remains the wearable placement source.',
    {'main_bag_body_components': [78, 125]})

# The front pouch was worn diagonally. Stand its main shell upright for item use.
pouch_shell = component_points([56, 99])
pouch_axes = principal_axes(pouch_shell)
pouch_z, pouch_x = pouch_axes[:, -1], pouch_axes[:, 0]
if pouch_z[2] < 0:
    pouch_z = -pouch_z
if pouch_x[1] < 0:
    pouch_x = -pouch_x
pouch_r = axis_frame(pouch_z, pouch_x)
pouch_points = P[part_ids('BP2_Front_Pouch')]
add('BP2_Front_Pouch', pouch_r, bounds_midpoint(pouch_points, pouch_r, base=True),
    'Bottom-center of the upright pouch bounds.', 'medium-high',
    'Main-shell longest principal axis (toward original upper end) becomes +Z. Main-shell thin-axis normal toward the visible button/front (source +Y) becomes +X. This removes the diagonal worn orientation without deforming the pouch.',
    {'main_shell_components': [56, 99], 'button_components': [71, 178, 179]})

holder_points = P[part_ids('BP2_Bottle_Holder')]
add('BP2_Bottle_Holder', bottle_r, bounds_midpoint(holder_points, bottle_r),
    'Geometric center of the carrier cloth bounds, for attachment and manipulation.', 'high for centered manipulation; medium for upright appearance',
    'Uses the same upright axis and facing as its bottle so the carrier reads as a usable sleeve. Center the whole carrier cloth instead of grounding the hanging flap tip; geometry retains its original worn shape.')

rope_points = P[part_ids('BP2_Rope')]
rope_axes = principal_axes(rope_points)
rope_x, rope_z = rope_axes[:, -1], rope_axes[:, 0]
if rope_z[2] < 0:
    rope_z = -rope_z
if rope_x[0] < 0:
    rope_x = -rope_x
rope_y = normalized(np.cross(rope_z, rope_x))
rope_x = normalized(np.cross(rope_y, rope_z))
rope_r = np.stack([rope_x, rope_y, rope_z])
add('BP2_Rope', rope_r, bounds_midpoint(rope_points, rope_r),
    'Geometric bounds center of the rotated rope loop.', 'high for best-fit orientation; medium for resting shape',
    'Principal long axis becomes +X and best-fit loop plane becomes XY. The worn rope is nonplanar; this is a rigid orientation, not a flattening or geometry edit. Pivot remains geometric center, so some geometry lies below Z=0.')

if HASHES != {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in FILES}:
    raise RuntimeError('Source inputs changed during calculation')
manifest['source_input_hashes_unchanged'] = True
(ROOT / 'item_pivots.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
(ROOT / 'item_pivot_preview_data.json').write_text(json.dumps(preview), encoding='utf-8')
for name, data in manifest['parts'].items():
    print(name, 'pivot', [round(x, 6) for x in data['origin_world_m']], 'dimensions_m', [round(x, 4) for x in data['item_dimensions_m']])
