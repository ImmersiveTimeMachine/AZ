"""Blender-native materials for the approved CHALK option 01 canvas holster.

API:
    mats = create_materials(anchor=None, prefix="CHALK_Holster_v02",
                            uv_map=None, webbing_uv_map=None)

Returns canvas, webbing, binding, thread, blue_patch, metal, metal_dark,
lining, and canvas_worn. Does not create geometry, change lighting/color
management, render, save, or modify pre-existing materials.

Coordinates are METERS. Prefer an Empty shared by all parts at identity
rotation and scale; pass it as anchor. With no anchor, object scales must
be applied for physical thread dimensions. A named UV map may be supplied
instead: its U/V values must be physical meters, not normalized 0..1.
For webbing, U is across the width and V is along the strap. Assigning
webbing_uv_map is the best way to keep transverse ribs perpendicular to
both curved thigh bands and the vertical belt loop.

Optional FLOAT attribute ``holster_edge_wear`` (0..1) adds deliberate
rubbed-edge wear. Cycles pointiness also adds restrained convex-edge fade;
the microstructure and the spatial wear remain useful without either.

All authored swatches are sRGB hex converted to linear socket values.
Shader bump distances are meters; materials do not displace silhouettes.
Wave band pitch is calibrated to Blender's kernel phase 20 * coordinate *
Scale: https://github.com/blender/blender/blob/main/intern/cycles/kernel/svm/wave.h
"""

import json
import math

import bpy


VERSION = "CHALK_option01_materials_v02_2026-09-13"


def srgb_to_linear(value):
    value = float(value)
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def rgba(hex_color, alpha=1.0):
    code = hex_color.lstrip('#')
    if len(code) == 3:
        code = ''.join(c * 2 for c in code)
    if len(code) != 6:
        raise ValueError('Expected an RGB hex swatch: ' + hex_color)
    return tuple(srgb_to_linear(int(code[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (alpha,)


class _Graph:
    def __init__(self, material, anchor, uv_map):
        self.mat = material
        material.use_nodes = True
        self.nodes = material.node_tree.nodes
        self.links = material.node_tree.links
        self.nodes.clear()
        self.column = 0
        self.anchor = anchor
        self.uv_map = uv_map
        self.output = self.node('ShaderNodeOutputMaterial', 'Material Output')
        self.bsdf = self.node('ShaderNodeBsdfPrincipled', 'Surface / physical cloth and hardware')
        self.link(self.bsdf.outputs['BSDF'], self.output.inputs['Surface'])
        self.geometry = self.node('ShaderNodeNewGeometry', 'Surface geometry')
        if uv_map:
            tex = self.node('ShaderNodeUVMap', 'Meter UVs / U width, V length')
            tex.uv_map = uv_map
            self.coord = tex.outputs['UV']
        else:
            tex = self.node('ShaderNodeTexCoord', 'Object coordinates / meters')
            if anchor is not None:
                tex.object = anchor
            self.coord = tex.outputs['Object']

    def node(self, kind, label):
        n = self.nodes.new(kind)
        n.name = label
        n.label = label
        # A deterministic readable grid, with the final surface nodes at right.
        n.location = (-1450 + (self.column % 8) * 240, -(self.column // 8) * 210)
        self.column += 1
        return n

    def link(self, source, target):
        if hasattr(source, 'is_output'):
            self.links.new(source, target)
        else:
            target.default_value = source
        return target

    def scalar(self, operation, a, b=0.0, label=None, clamp=False):
        n = self.node('ShaderNodeMath', label or operation.title())
        n.operation = operation
        n.use_clamp = clamp
        self.link(a, n.inputs[0])
        self.link(b, n.inputs[1])
        return n.outputs[0]

    def vector(self, operation, a, b=None, label=None):
        n = self.node('ShaderNodeVectorMath', label or operation.title())
        n.operation = operation
        self.link(a, n.inputs[0])
        if b is not None:
            self.link(b, n.inputs[1])
        return n.outputs['Vector']

    def mix(self, factor, a, b, label, mode='MIX'):
        n = self.node('ShaderNodeMixRGB', label)
        n.blend_type = mode
        self.link(factor, n.inputs[0])
        self.link(a, n.inputs[1])
        self.link(b, n.inputs[2])
        return n.outputs[0]

    def ramp(self, value, stops, label):
        n = self.node('ShaderNodeValToRGB', label)
        cr = n.color_ramp
        cr.interpolation = 'EASE'
        for index, (position, color) in enumerate(stops):
            e = cr.elements[index] if index < 2 else cr.elements.new(position)
            e.position = position
            # Numeric stops are LINEAR masks, never sRGB-converted.
            e.color = (color, color, color, 1.0) if isinstance(color, (int, float)) else rgba(color)
        self.link(value, n.inputs['Fac'])
        return n.outputs['Color']

    def noise(self, scale, detail, roughness, label, coord=None):
        n = self.node('ShaderNodeTexNoise', label)
        n.noise_dimensions = '3D'
        n.inputs['Scale'].default_value = scale
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = roughness
        self.link(coord or self.coord, n.inputs['Vector'])
        return n.outputs['Fac']

    def wave(self, axis, pitch, label, distortion=.18, coord=None):
        n = self.node('ShaderNodeTexWave', label)
        n.wave_type = 'BANDS'
        n.bands_direction = axis
        n.wave_profile = 'SIN'
        n.inputs['Scale'].default_value = math.pi / (10.0 * pitch)
        n.inputs['Distortion'].default_value = distortion
        n.inputs['Detail'].default_value = 2.0
        n.inputs['Detail Scale'].default_value = .37
        n.inputs['Detail Roughness'].default_value = .62
        self.link(coord or self.coord, n.inputs['Vector'])
        return n.outputs['Fac']

    def bump(self, height, distance, strength, label, normal=None):
        n = self.node('ShaderNodeBump', label)
        n.inputs['Distance'].default_value = distance
        n.inputs['Strength'].default_value = strength
        self.link(height, n.inputs['Height'])
        if normal is not None:
            self.link(normal, n.inputs['Normal'])
        return n.outputs['Normal']

    def surface(self, name, value):
        socket = self.bsdf.inputs.get(name)
        if socket is not None:
            self.link(value, socket)

    def finish(self):
        self.bsdf.location = (650, 250)
        self.output.location = (960, 250)


def _weave(g, pitch, relief, ribs=False):
    """True crossing warp/weft waves with alternating over/under cells."""
    xyz = g.node('ShaderNodeSeparateXYZ', 'Physical cloth position')
    g.link(g.coord, xyz.inputs['Vector'])
    axes = ['X', 'Y'] if g.uv_map else ['X', 'Y', 'Z']
    waves = {a: g.scalar('POWER', g.wave(a, pitch, a + ' yarn / physical pitch'), .75,
                        a + ' rounded yarn profile') for a in axes}

    def projected(u, v):
        combine = g.node('ShaderNodeCombineXYZ', u + v + ' projection for interlace')
        g.link(xyz.outputs[u], combine.inputs['X'])
        g.link(xyz.outputs[v], combine.inputs['Y'])
        checker = g.node('ShaderNodeTexChecker', u + v + ' alternating over / under')
        checker.inputs['Scale'].default_value = 1.0 / pitch
        g.link(combine.outputs[0], checker.inputs['Vector'])
        interlace = g.mix(checker.outputs['Fac'], waves[u], waves[v], u + v + ' warp / weft crossing')
        crown = g.scalar('MAXIMUM', waves[u], waves[v], u + v + ' crossing yarn crown')
        return g.scalar('ADD', g.scalar('MULTIPLY', interlace, .68),
                        g.scalar('MULTIPLY', crown, .32), u + v + ' woven height')

    if g.uv_map:
        weave = projected('X', 'Y')
    else:
        # Normal-weighted box projection avoids missing weave on side walls.
        absolute = g.vector('ABSOLUTE', g.geometry.outputs['Normal'], label='Absolute surface normal')
        separate = g.node('ShaderNodeSeparateXYZ', 'Box projection weights')
        g.link(absolute, separate.inputs['Vector'])
        w = {a: g.scalar('POWER', separate.outputs[a], 4.0, a + ' projection weight') for a in axes}
        total = g.scalar('ADD', g.scalar('ADD', w['X'], w['Y']), w['Z'])
        xy = g.scalar('MULTIPLY', projected('X', 'Y'), w['Z'])
        xz = g.scalar('MULTIPLY', projected('X', 'Z'), w['Y'])
        yz = g.scalar('MULTIPLY', projected('Y', 'Z'), w['X'])
        weave = g.scalar('DIVIDE', g.scalar('ADD', g.scalar('ADD', xy, xz), yz),
                         total, 'Continuous box-projected weave')

    fiber_noise = g.noise(1.0 / (pitch * .18), 2.0, .6, 'Individual irregular fibers')
    fibers = g.scalar('ADD', .78, g.scalar('MULTIPLY', fiber_noise, .22), 'Yarn thickness variation')
    height = g.scalar('MULTIPLY', weave, fibers, 'Woven yarn relief')
    if ribs:
        rib_axis = 'Y' if g.uv_map else 'Z'
        rib = g.wave(rib_axis, .00135, 'Webbing transverse reinforcement ribs', .10)
        height = g.scalar('ADD', g.scalar('MULTIPLY', height, .68),
                          g.scalar('MULTIPLY', rib, .32), 'Woven ribbed webbing height')
    normal = g.bump(height, relief, .52, 'Thread-scale cloth bump')
    normal = g.bump(fiber_noise, relief * .13, .18, 'Fine fiber irregularity', normal)
    return height, normal


def _wear(g, amount=1.0):
    clouds = g.noise(34.0, 3.2, .64, 'Irregular 3 cm wax and fade patches')
    abrasion = g.noise(185.0, 4.3, .73, '5 mm scuffed abrasion boundaries')
    grit = g.noise(1080.0, 2.0, .66, 'Sub-millimeter embedded grit')
    broken = g.scalar('MULTIPLY',
                      g.ramp(clouds, [(.30, .0), (.68, 1.0)], 'Broad wax-loss mask'),
                      g.ramp(abrasion, [(.44, .0), (.73, 1.0)], 'Broken scuff mask'),
                      'Patchy abrasion rather than uniform noise')
    pointiness = g.ramp(g.geometry.outputs['Pointiness'], [(.515, .0), (.595, .52)],
                       'Restrained convex-edge wear / Cycles')
    authored = g.node('ShaderNodeAttribute', 'Optional painted edge wear')
    authored.attribute_name = 'holster_edge_wear'
    edge = g.scalar('MAXIMUM', pointiness, authored.outputs['Fac'], 'Geometric or authored edge wear')
    mask = g.scalar('ADD', g.scalar('MULTIPLY', broken, .72 * amount),
                    g.scalar('MULTIPLY', edge, .38 * amount), 'Rubbed canvas wear', clamp=True)
    dust = g.scalar('MULTIPLY',
                    g.ramp(abrasion, [(.56, .0), (.77, .58)], 'Sparse abraded fiber dust'),
                    g.ramp(grit, [(.43, .0), (.71, .72)], 'Dust speck breakup'),
                    'Localized pale dust flecks')
    dust = g.scalar('MULTIPLY', dust, g.scalar('ADD', .24, mask), 'Dust gathers in worn patches')
    return clouds, mask, dust, abrasion


def _cloth(material, anchor, uv_map, *, dark, base, faded, dust, pitch,
           relief, roughness, sheen, wax=0.0, ribs=False, wear_amount=1.0):
    g = _Graph(material, anchor, uv_map)
    weave, normal = _weave(g, pitch, relief, ribs)
    clouds, worn, flecks, abrasion = _wear(g, wear_amount)
    albedo = g.ramp(clouds, [(.20, dark), (.77, base)], 'Dark cloth dye / linear sRGB swatches')
    albedo = g.mix(worn, albedo, rgba(faded), 'Exposed faded yarn')
    albedo = g.mix(flecks, albedo, rgba(dust), 'Sparse accumulated pale dust')
    yarn_tint = g.ramp(weave, [(.0, '#b5b1a8'), (1.0, '#ffffff')], 'Subtle fiber albedo modulation')
    albedo = g.mix(.22, albedo, yarn_tint, 'Weave affects dye and highlights', mode='MULTIPLY')
    # Low-frequency crushed wax adds a second physical scale under the yarn.
    normal = g.bump(abrasion, .000025, .12, 'Crushed wax and minor scuff relief', normal)
    r = g.scalar('ADD', roughness - .07, g.scalar('MULTIPLY', clouds, .10), 'Uneven cloth roughness')
    r = g.scalar('ADD', r, g.scalar('MULTIPLY', worn, .08), 'Worn fibers scatter more', clamp=True)
    g.surface('Base Color', albedo)
    g.surface('Roughness', r)
    g.surface('Normal', normal)
    g.surface('Metallic', .0)
    g.surface('IOR', 1.46)
    g.surface('Specular IOR Level', .25)
    g.surface('Sheen Weight', sheen)
    g.surface('Sheen Roughness', .78)
    g.surface('Coat Weight', g.scalar('MULTIPLY', g.scalar('SUBTRACT', 1.0, worn), wax,
                                    'Remaining wax on intact canvas'))
    g.surface('Coat Roughness', .42)
    material.diffuse_color = rgba(base)
    material.roughness = roughness
    g.finish()


def _metal(material, anchor, dark=False):
    g = _Graph(material, anchor, None)
    brushed = g.wave('X', .00032, 'Fine broken handling scratches', 2.1)
    fine_lines = g.ramp(brushed, [(.87, .0), (.975, 1.0)], 'Hairline scratch grooves')
    streak_coord = g.vector('MULTIPLY', g.coord, (22.0, 360.0, 95.0), 'Elongated scratch interruption')
    interrupted = g.noise(1.0, 2.0, .63, 'Irregular short scratch segments', streak_coord)
    scratches = g.scalar('MULTIPLY', fine_lines,
                          g.ramp(interrupted, [(.38, .0), (.68, 1.0)], 'Broken scratch mask'),
                          'Scuffed steel lines')
    pitting = g.noise(1650.0, 2.0, .64, 'Fine oxidized steel pits')
    wear_clouds = g.noise(130.0, 3.0, .68, 'Patchy oxide and hand wear')
    base = g.ramp(wear_clouds,
                  [(.20, '#151918' if dark else '#777b77'),
                   (.80, '#343936' if dark else '#a6aaa4')],
                  'Blackened steel' if dark else 'Dull steel snap only')
    highlight = rgba('#636862' if dark else '#bfc3bb')
    amount = .15 if dark else .23
    albedo = g.mix(g.scalar('MULTIPLY', scratches, amount), base, highlight,
                   'Sparse worn steel exposed by scratches')
    normal = g.bump(g.scalar('SUBTRACT', 1.0, scratches), .000009, .26, 'Scratches cut into metal')
    normal = g.bump(pitting, .000006, .15, 'Microscopic oxide pitting', normal)
    g.surface('Base Color', albedo)
    g.surface('Metallic', .87 if dark else .93)
    rough = g.scalar('ADD', .46 if dark else .40,
                     g.scalar('MULTIPLY', wear_clouds, .16), 'Dull uneven metal roughness')
    g.surface('Roughness', rough)
    g.surface('Normal', normal)
    g.surface('Anisotropic IOR Level', .20)
    material.diffuse_color = rgba('#292e2b' if dark else '#92978f')
    material.metallic = .87 if dark else .93
    material.roughness = .55 if dark else .49
    g.finish()


def create_materials(anchor=None, prefix='CHALK_Holster_v02', uv_map=None, webbing_uv_map=None):
    """Create new local Blender materials and return them by semantic role.

    ``anchor`` accepts a Blender Object or its exact object name. Identity
    anchor rotation/scale is recommended for consistent triplanar weights.
    ``uv_map`` and ``webbing_uv_map`` are optional meter-valued UV names.
    Existing materials, scene objects, render settings and files are untouched.
    """
    if isinstance(anchor, str):
        resolved = bpy.data.objects.get(anchor)
        if resolved is None:
            raise ValueError('Material coordinate anchor does not exist: ' + anchor)
        anchor = resolved
    if anchor is not None and not isinstance(anchor, bpy.types.Object):
        raise TypeError('anchor must be a Blender Object, exact object name, or None')

    specs = {
        'canvas': dict(dark='#252423', base='#403b35', faded='#63594a', dust='#a58f73',
                       pitch=.00068, relief=.000095, roughness=.71, sheen=.12, wax=.085),
        'canvas_worn': dict(dark='#2b2926', base='#474037', faded='#776651', dust='#b29b7b',
                            pitch=.00068, relief=.00011, roughness=.77, sheen=.14,
                            wax=.035, wear_amount=1.35),
        'webbing': dict(dark='#303327', base='#51513a', faded='#787252', dust='#a59770',
                        pitch=.00048, relief=.00014, roughness=.81, sheen=.25,
                        ribs=True, wear_amount=.8),
        'binding': dict(dark='#40382d', base='#726047', faded='#938066', dust='#a99a7d',
                        pitch=.00038, relief=.000070, roughness=.78, sheen=.15,
                        wear_amount=.65),
        'thread': dict(dark='#726149', base='#a08b68', faded='#baaa8a', dust='#c5b598',
                       pitch=.00019, relief=.000019, roughness=.83, sheen=.18,
                       wear_amount=.25),
        'blue_patch': dict(dark='#293543', base='#455669', faded='#697887', dust='#9b9d97',
                           pitch=.00049, relief=.000085, roughness=.84, sheen=.20,
                           wear_amount=.95),
        'lining': dict(dark='#101310', base='#24281f', faded='#363d2b', dust='#635f48',
                       pitch=.00047, relief=.000065, roughness=.91, sheen=.08,
                       wear_amount=.35),
    }
    result = {}
    for key, parameters in specs.items():
        mat = bpy.data.materials.new(prefix + '_' + key)
        selected_uv = (webbing_uv_map or uv_map) if key == 'webbing' else uv_map
        _cloth(mat, anchor, selected_uv, **parameters)
        mat['holster_material_version'] = VERSION
        mat['holster_role'] = key
        mat['coordinate_units'] = 'meters'
        mat['coordinate_anchor'] = anchor.name if anchor else 'Self Object; apply scale'
        mat['meter_uv_map'] = selected_uv or ''
        mat['source_palette_srgb'] = json.dumps({k: v for k, v in parameters.items()
                                                if k in {'dark', 'base', 'faded', 'dust'}})
        mat['physical_yarn_pitch_m'] = parameters['pitch']
        mat['bump_distance_m'] = parameters['relief']
        result[key] = mat
    for key, is_dark in [('metal', False), ('metal_dark', True)]:
        mat = bpy.data.materials.new(prefix + '_' + key)
        _metal(mat, anchor, dark=is_dark)
        mat['holster_material_version'] = VERSION
        mat['holster_role'] = key
        mat['usage'] = 'Dull silver retention snap only' if not is_dark else 'All rings, sliders, buckles and other blackened hardware'
        result[key] = mat
    return result


__all__ = ['create_materials', 'rgba', 'srgb_to_linear', 'VERSION']
