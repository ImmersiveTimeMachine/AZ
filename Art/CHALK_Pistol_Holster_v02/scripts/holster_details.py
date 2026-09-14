"""Editable Blender webbing, hardware and sewing details, in meters.

Importing this module changes nothing. Every public builder returns one Object
and links it only to the supplied Collection. Materials may be None. Coordinates
and directions are in the supplied parent's local frame, or world space when
parent is None; no active scene, selection, mode or operators are used.

Public API:
    ribbon(collection, name, material, center_points, width_directions, ...)
    ribbon_edge_paths(center_points, width_directions, ...)
    rounded_rectangle_buckle(collection, name, material, center, ...)
    stitch_path(collection, name, material, points, ...)
    box_x_stitch(collection, name, material, center, ...)
    bar_tack(collection, name, material, center, ...)
    edge_piping(collection, name, material, points, ...)

For a folded strap, supply the full center path down the front, around the
buckle rail, and back up its return tail; keep the cross-width direction across
the webbing. Stitch the exposed return tail with box_x_stitch. A .03 m ribbon
needs a buckle whose clear opening is wider than .03 m: the default .040 m
outer width with .0018 m wire leaves .0328 m between its inner side surfaces.
"""

import bisect
import math
from numbers import Real

import bpy
from mathutils import Matrix, Vector


_EPS = 1.0e-9


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _vector(value, label='vector'):
    result = Vector(value)
    _require(len(result) == 3 and all(math.isfinite(component) for component in result),
             label + ' must contain three finite coordinates')
    return result


def _points(values, closed=False):
    result = [_vector(value, 'point') for value in values]
    if closed and len(result) > 2 and (result[-1] - result[0]).length < _EPS:
        result.pop()
    _require(len(result) >= (3 if closed else 2), 'Path needs at least two points, or three for a closed path')
    _require(all((b - a).length > _EPS for a, b in zip(result, result[1:])),
             'Consecutive path points must be distinct')
    return result


def _vectors(values, count, label):
    values = list(values)
    if len(values) == 3 and all(isinstance(value, Real) for value in values):
        result = [_vector(values, label)] * count
    else:
        _require(len(values) == count, label + ' must be one vector or one vector per source point')
        result = [_vector(value, label) for value in values]
    _require(all(value.length > _EPS for value in result), label + ' cannot be zero')
    return [value.normalized() for value in result]


def _widths(width, count):
    values = [float(width)] * count if isinstance(width, Real) else [float(value) for value in width]
    _require(len(values) == count and all(math.isfinite(value) and value > 0 for value in values),
             'Width must be positive, either one value or one per source point')
    return values


def _sample_path(points, samples_per_segment=4, closed=False, smooth=True):
    """Return (position, source segment index, segment fraction) samples."""
    _require(isinstance(samples_per_segment, int) and 1 <= samples_per_segment <= 64,
             'samples_per_segment must be an integer from 1 to 64')
    count = len(points)
    result = []
    for index in range(count if closed else count - 1):
        p1, p2 = points[index], points[(index + 1) % count]
        p0 = points[(index - 1) % count] if index > 0 or closed else 2 * p1 - p2
        p3 = points[(index + 2) % count] if index + 2 < count or closed else 2 * p2 - p1
        for step in range(samples_per_segment):
            t = step / samples_per_segment
            point = (0.5 * (2 * p1 + (-p0 + p2) * t +
                           (2 * p0 - 5 * p1 + 4 * p2 - p3) * (t * t) +
                           (-p0 + 3 * p1 - 3 * p2 + p3) * (t * t * t))) if smooth else p1.lerp(p2, t)
            result.append((point, index, t))
    if not closed:
        result.append((points[-1].copy(), count - 2, 1.0))
    return result


def _ribbon_frames(center_points, width_directions, width, samples_per_segment,
                   closed, smooth_path, end_support):
    points = _points(center_points, closed)
    directions = _vectors(width_directions, len(points), 'width_directions')
    widths = _widths(width, len(points))
    rows = []
    for point, index, t in _sample_path(points, samples_per_segment, closed, smooth_path):
        next_index = (index + 1) % len(points)
        direction = directions[index].lerp(directions[next_index], t)
        _require(direction.length > _EPS, 'Adjacent width directions cancel; provide a consistent ribbon frame')
        rows.append((point, direction, widths[index] * (1 - t) + widths[next_index] * t))
    if not closed and end_support > 0:
        # Local support rows keep cloth ends from being shortened by subdivision.
        first, second = rows[0], rows[1]
        before_last, last = rows[-2], rows[-1]
        start_t = min(0.35, end_support / (second[0] - first[0]).length)
        end_t = min(0.35, end_support / (last[0] - before_last[0]).length)
        start_row = (first[0].lerp(second[0], start_t), first[1].lerp(second[1], start_t),
                     first[2] * (1 - start_t) + second[2] * start_t)
        end_row = (last[0].lerp(before_last[0], end_t), last[1].lerp(before_last[1], end_t),
                   last[2] * (1 - end_t) + before_last[2] * end_t)
        rows = [rows[0], start_row, *rows[1:-1], end_row, rows[-1]]
    frames = []
    length = 0.0
    for index, (point, supplied_width, row_width) in enumerate(rows):
        previous = rows[(index - 1) % len(rows)][0] if index > 0 or closed else point
        following = rows[(index + 1) % len(rows)][0] if index < len(rows) - 1 or closed else point
        tangent = following - previous
        _require(tangent.length > _EPS, 'Ribbon path reverses through a zero-length tangent; round the fold with extra points')
        tangent.normalize()
        width_direction = supplied_width - tangent * supplied_width.dot(tangent)
        _require(width_direction.length > _EPS, 'Width direction cannot be parallel to the ribbon path')
        width_direction.normalize()
        normal = width_direction.cross(tangent).normalized()
        if index:
            length += (point - rows[index - 1][0]).length
        frames.append((point, width_direction, normal, row_width, length))
    total_length = length + ((rows[-1][0] - rows[0][0]).length if closed else 0)
    return frames, total_length


def _target(collection, name, material, parent):
    _require(isinstance(collection, bpy.types.Collection), 'collection must be a Blender Collection')
    _require(isinstance(name, str) and name and bpy.data.objects.get(name) is None,
             'Provide a nonempty, unused object name')
    _require(material is None or isinstance(material, bpy.types.Material), 'material must be a Blender Material or None')
    _require(parent is None or isinstance(parent, bpy.types.Object), 'parent must be a Blender Object or None')


def _link(data, collection, name, material, parent):
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    if material is not None:
        data.materials.append(material)
    if parent is not None:
        obj.parent = parent
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Identity(4)
    return obj


def ribbon(collection, name, material, center_points, width_directions,
           width=0.030, thickness=0.0018, subdivision=2, samples_per_segment=4,
           closed=False, smooth_path=True, end_support=0.0010,
           edge_support_fraction=0.035, edge_bevel=0.00022,
           metres_per_uv=1.0, uv_name='ClothMeters', parent=None):
    """Make one quad ribbon with editable Subdivision -> Solidify -> Bevel.

    width_directions is one vector or one per center point; width may likewise
    be a scalar or a per-point list. The surface normal is width cross tangent.
    Thickness is centered on the supplied surface. UV U measures cross-width
    meters, and V measures accumulated path meters, divided by metres_per_uv.
    smooth_path=False honors a supplied dense path without Catmull-Rom fitting.
    """
    _target(collection, name, material, parent)
    _require(thickness > 0 and metres_per_uv > 0, 'Thickness and metres_per_uv must be positive')
    _require(isinstance(subdivision, int) and 0 <= subdivision <= 4, 'subdivision must be from 0 to 4')
    _require(0 < edge_support_fraction < 0.25, 'edge_support_fraction must be between 0 and .25')
    _require(end_support >= 0 and edge_bevel >= 0, 'Support and bevel widths cannot be negative')
    frames, length = _ribbon_frames(center_points, width_directions, width,
                                     samples_per_segment, closed, smooth_path, end_support)
    fractions = (-0.5, -0.5 + edge_support_fraction, 0.5 - edge_support_fraction, 0.5)
    vertices = [tuple(point + cross * (fraction * row_width))
                for point, cross, normal, row_width, distance in frames for fraction in fractions]
    faces, uv_faces = [], []
    row_count = len(frames)
    for row in range(row_count if closed else row_count - 1):
        next_row = (row + 1) % row_count
        current_v = frames[row][4] / metres_per_uv
        next_v = (length if next_row == 0 else frames[next_row][4]) / metres_per_uv
        for column in range(3):
            faces.append((row * 4 + column, row * 4 + column + 1,
                          next_row * 4 + column + 1, next_row * 4 + column))
            u0 = (fractions[column] + 0.5) * frames[row][3] / metres_per_uv
            u1 = (fractions[column + 1] + 0.5) * frames[row][3] / metres_per_uv
            next_u0 = (fractions[column] + 0.5) * frames[next_row][3] / metres_per_uv
            next_u1 = (fractions[column + 1] + 0.5) * frames[next_row][3] / metres_per_uv
            uv_faces.append(((u0, current_v), (u1, current_v), (next_u1, next_v), (next_u0, next_v)))
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name=uv_name)
    for face, coordinates in zip(mesh.polygons, uv_faces):
        face.use_smooth = True
        for loop_index, coordinate in zip(face.loop_indices, coordinates):
            uv.data[loop_index].uv = coordinate
    obj = _link(mesh, collection, name, material, parent)
    if subdivision:
        modifier = obj.modifiers.new('Editable woven strap smoothing', 'SUBSURF')
        modifier.subdivision_type = 'CATMULL_CLARK'
        modifier.levels = subdivision
        modifier.render_levels = subdivision
        modifier.boundary_smooth = 'PRESERVE_CORNERS'
        modifier.uv_smooth = 'PRESERVE_BOUNDARIES'
    modifier = obj.modifiers.new('Woven strap thickness', 'SOLIDIFY')
    modifier.thickness = thickness
    modifier.offset = 0.0
    modifier.use_even_offset = True
    modifier.use_quality_normals = True
    if edge_bevel:
        modifier = obj.modifiers.new('Soft fabric edges', 'BEVEL')
        modifier.width = min(edge_bevel, thickness * 0.3)
        modifier.segments = 2
        modifier.limit_method = 'ANGLE'
        modifier.angle_limit = math.radians(35)
    obj['detail_type'] = 'woven_ribbon'
    obj['path_length_m'] = length
    obj['thickness_m'] = thickness
    obj['metres_per_uv'] = metres_per_uv
    return obj


def ribbon_edge_paths(center_points, width_directions, width=0.030,
                      inset=0.0020, surface_lift=0.00105,
                      samples_per_segment=4, closed=False, smooth_path=True,
                      end_support=0.0010):
    """Return (left_points, right_points, surface_normals) for edge sewing.

    Pass the returned paths to stitch_path with samples_per_segment=1 and
    smooth_path=False: they already follow the same dense ribbon center path.
    surface_lift is measured from the ribbon center surface; for .0018 thickness
    a .00105 lift places thread centers just above its outer side.
    """
    frames, unused_length = _ribbon_frames(center_points, width_directions, width,
                                          samples_per_segment, closed, smooth_path, end_support)
    _require(inset >= 0 and all(inset < frame[3] * 0.5 for frame in frames), 'Inset must fit inside the ribbon width')
    left, right, normals = [], [], []
    for point, cross, normal, row_width, distance in frames:
        center = point + normal * surface_lift
        offset = cross * (row_width * 0.5 - inset)
        left.append(tuple(center - offset))
        right.append(tuple(center + offset))
        normals.append(tuple(normal))
    return left, right, normals


def _new_curve(collection, name, material, radius, bevel_resolution, parent):
    _target(collection, name, material, parent)
    _require(radius > 0 and 0 <= bevel_resolution <= 8, 'Curve radius/resolution is invalid')
    data = bpy.data.curves.new(name + '_Curve', 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 5
    data.bevel_depth = radius
    data.bevel_resolution = bevel_resolution
    data.use_fill_caps = True
    return _link(data, collection, name, material, parent)


def _basis(width_axis, height_axis):
    width_axis = _vector(width_axis, 'width_axis')
    height_axis = _vector(height_axis, 'height_axis')
    _require(width_axis.length > _EPS, 'width_axis cannot be zero')
    width_axis.normalize()
    height_axis -= width_axis * height_axis.dot(width_axis)
    _require(height_axis.length > _EPS, 'Buckle/patch axes must not be parallel')
    height_axis.normalize()
    return width_axis, height_axis, width_axis.cross(height_axis).normalized()


def rounded_rectangle_buckle(collection, name, material, center,
                             width_axis=(1, 0, 0), height_axis=(0, 0, 1),
                             outer_width=0.040, outer_height=0.024,
                             corner_radius=0.005, wire_radius=0.0018,
                             center_bar=False, center_bar_offset=0.0,
                             bevel_resolution=4, parent=None):
    """One continuous rounded-rectangle Bezier ring, with optional center rail.

    Dimensions and corner_radius include the round wire thickness. Each corner
    is an exact cubic quarter-circle approximation with tangent-matched handles;
    there are no four independent bars or mitred/open outer corners. A center
    rail, if requested, is a second spline within the same curve object.
    """
    _target(collection, name, material, parent)
    center = _vector(center, 'center')
    width_axis, height_axis, normal = _basis(width_axis, height_axis)
    _require(wire_radius > 0 and min(outer_width, outer_height) > 4 * wire_radius,
             'Buckle outer dimensions must leave a clear opening')
    _require(wire_radius < corner_radius <= min(outer_width, outer_height) * 0.5,
             'Corner radius must exceed wire radius and fit the buckle')
    half_width, half_height = outer_width * 0.5 - wire_radius, outer_height * 0.5 - wire_radius
    radius = corner_radius - wire_radius
    xy = [(half_width - radius, half_height), (half_width, half_height - radius),
          (half_width, -half_height + radius), (half_width - radius, -half_height),
          (-half_width + radius, -half_height), (-half_width, -half_height + radius),
          (-half_width, half_height - radius), (-half_width + radius, half_height)]
    tangents = [width_axis, -height_axis, -height_axis, -width_axis,
                -width_axis, height_axis, height_axis, width_axis]
    positions = [center + width_axis * x + height_axis * y for x, y in xy]
    arc_handle = 4.0 / 3.0 * math.tan(math.pi / 8.0) * radius
    handles = [arc_handle if index % 2 == 0 else (positions[(index + 1) % 8] - positions[index]).length / 3.0
               for index in range(8)]
    obj = _new_curve(collection, name, material, wire_radius, bevel_resolution, parent)
    obj.data.resolution_u = 8
    spline = obj.data.splines.new('BEZIER')
    spline.bezier_points.add(7)
    spline.use_cyclic_u = True
    for index, point in enumerate(spline.bezier_points):
        point.co = positions[index]
        point.handle_left_type = point.handle_right_type = 'FREE'
        point.handle_left = positions[index] - tangents[index] * handles[(index - 1) % 8]
        point.handle_right = positions[index] + tangents[index] * handles[index]
    if center_bar:
        _require(abs(center_bar_offset) < half_height - wire_radius, 'Center rail offset lies outside buckle')
        rail = obj.data.splines.new('POLY')
        rail.points.add(1)
        rail.points[0].co = (*tuple(center - width_axis * half_width + height_axis * center_bar_offset), 1)
        rail.points[1].co = (*tuple(center + width_axis * half_width + height_axis * center_bar_offset), 1)
    obj['detail_type'] = 'continuous_rounded_buckle'
    obj['outer_width_m'], obj['outer_height_m'] = outer_width, outer_height
    return obj


def _thread_dashes(points, surface_normals, dash_length, gap, arch_height,
                    surface_offset, closed, samples_per_segment, smooth_path):
    points = _points(points, closed)
    normals = _vectors(surface_normals, len(points), 'surface_normals')
    _require(dash_length > 0 and gap >= 0 and arch_height >= 0, 'Dash length/gap/arch is invalid')
    samples = _sample_path(points, samples_per_segment, closed, smooth_path)
    dense_points, dense_normals = [], []
    for point, index, t in samples:
        normal = normals[index].lerp(normals[(index + 1) % len(points)], t)
        _require(normal.length > _EPS, 'Stitch surface normals cancel along path')
        dense_points.append(point)
        dense_normals.append(normal.normalized())
    if closed:
        dense_points.append(dense_points[0])
        dense_normals.append(dense_normals[0])
    distances = [0.0]
    for first, second in zip(dense_points, dense_points[1:]):
        distances.append(distances[-1] + (second - first).length)
    length = distances[-1]

    def evaluate(distance):
        distance = distance % length if closed else max(0.0, min(length, distance))
        index = min(len(distances) - 2, max(0, bisect.bisect_right(distances, distance) - 1))
        span = distances[index + 1] - distances[index]
        t = (distance - distances[index]) / span if span > _EPS else 0.0
        return dense_points[index].lerp(dense_points[index + 1], t), dense_normals[index].lerp(dense_normals[index + 1], t).normalized()

    if closed:
        count = max(1, int(length / (dash_length + gap)))
        pitch, margin = length / count, 0.0
        actual_dash = min(dash_length, pitch * 0.9)
    else:
        margin = min(gap * 0.5, length * 0.05)
        count = max(1, int((length - margin * 2 + gap) / (dash_length + gap)))
        pitch, actual_dash = dash_length + gap, min(dash_length, length - margin * 2)
    _require(count <= 10000, 'Stitch count exceeds 10000; check path units')
    dashes = []
    for index in range(count):
        start = margin + index * pitch
        controls = []
        for t in (0.0, 0.5, 1.0):
            point, normal = evaluate(start + actual_dash * t)
            controls.append(point + normal * (surface_offset + arch_height * math.sin(math.pi * t)))
        dashes.append(controls)
    return dashes


def _add_dashes(curve_data, dashes):
    for controls in dashes:
        spline = curve_data.splines.new('BEZIER')
        spline.bezier_points.add(2)
        for point, coordinate in zip(spline.bezier_points, controls):
            point.co = coordinate
            point.handle_left_type = point.handle_right_type = 'AUTO'


def stitch_path(collection, name, material, points,
                surface_normals=(0, 0, 1), dash_length=0.0022, gap=0.0010,
                thread_radius=0.00022, arch_height=0.00012,
                surface_offset=0.0, closed=False, samples_per_segment=4,
                smooth_path=True, bevel_resolution=2, parent=None):
    """One curve object containing tiny dashed, arched thread splines.

    Path points describe the needle/surface line. Thread ends lie on that line
    plus surface_offset; each dash rises by arch_height in its middle. A supplied
    surface normal may be one vector or one per original path point.
    """
    _target(collection, name, material, parent)
    dashes = _thread_dashes(points, surface_normals, dash_length, gap, arch_height,
                            surface_offset, closed, samples_per_segment, smooth_path)
    obj = _new_curve(collection, name, material, thread_radius, bevel_resolution, parent)
    obj.data.resolution_u = 3
    _add_dashes(obj.data, dashes)
    obj['detail_type'] = 'fine_dashed_sewing'
    obj['stitch_count'] = len(dashes)
    obj['thread_radius_m'] = thread_radius
    obj['nominal_dash_m'] = dash_length
    return obj


def box_x_stitch(collection, name, material, center,
                 width_axis=(1, 0, 0), height_axis=(0, 0, 1),
                 width=0.023, height=0.022, surface_normal=None,
                 dash_length=0.0018, gap=0.00075, thread_radius=0.00020,
                 arch_height=0.00010, surface_offset=0.0, parent=None):
    """Rectangle plus two diagonal dashed stitch runs in one curve object."""
    _target(collection, name, material, parent)
    _require(width > 0 and height > 0, 'Box-X dimensions must be positive')
    center = _vector(center, 'center')
    x, y, normal = _basis(width_axis, height_axis)
    normal = normal if surface_normal is None else _vector(surface_normal).normalized()
    corners = [center + x * width * a + y * height * b
               for a, b in ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))]
    dashes = _thread_dashes(corners, normal, dash_length, gap, arch_height,
                             surface_offset, True, 1, False)
    for first, second in ((0, 2), (1, 3)):
        dashes.extend(_thread_dashes([corners[first], corners[second]], normal,
                                      dash_length, gap, arch_height, surface_offset, False, 1, False))
    obj = _new_curve(collection, name, material, thread_radius, 2, parent)
    obj.data.resolution_u = 3
    _add_dashes(obj.data, dashes)
    obj['detail_type'] = 'box_x_webbing_reinforcement'
    obj['stitch_count'] = len(dashes)
    return obj


def bar_tack(collection, name, material, center,
             width_axis=(1, 0, 0), height_axis=(0, 0, 1),
             width=0.022, height=0.0022, stitches=24,
             thread_radius=0.00019, surface_offset=0.00010, parent=None):
    """One dense, continuous zigzag bartack across a short webbing span."""
    _target(collection, name, material, parent)
    _require(width > 0 and height > 0 and isinstance(stitches, int) and stitches >= 3,
             'Bartack needs positive dimensions and at least three stitches')
    center = _vector(center, 'center')
    x, y, normal = _basis(width_axis, height_axis)
    coordinates = [center + x * width * (index / stitches - 0.5)
                   + y * height * (0.5 if index % 2 else -0.5) + normal * surface_offset
                   for index in range(stitches + 1)]
    obj = _new_curve(collection, name, material, thread_radius, 2, parent)
    spline = obj.data.splines.new('POLY')
    spline.points.add(len(coordinates) - 1)
    for point, coordinate in zip(spline.points, coordinates):
        point.co = (*tuple(coordinate), 1)
    obj['detail_type'] = 'dense_webbing_bartack'
    return obj


def edge_piping(collection, name, material, points, radius=0.00085,
                 closed=False, smooth=True, bevel_resolution=3,
                 resolution=8, parent=None):
    """A continuous cloth edge cord/seam, retained as an editable curve."""
    _target(collection, name, material, parent)
    points = _points(points, closed)
    obj = _new_curve(collection, name, material, radius, bevel_resolution, parent)
    obj.data.resolution_u = resolution
    spline = obj.data.splines.new('BEZIER' if smooth else 'POLY')
    spline.use_cyclic_u = closed
    if smooth:
        spline.bezier_points.add(len(points) - 1)
        for point, coordinate in zip(spline.bezier_points, points):
            point.co = coordinate
            point.handle_left_type = point.handle_right_type = 'AUTO'
    else:
        spline.points.add(len(points) - 1)
        for point, coordinate in zip(spline.points, points):
            point.co = (*tuple(coordinate), 1)
    obj['detail_type'] = 'continuous_edge_piping'
    return obj
