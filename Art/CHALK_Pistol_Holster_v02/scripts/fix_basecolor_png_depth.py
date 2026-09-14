"""Convert the holster's sRGB base-color PNG to Unreal-compatible 8-bit.

No rebake. Preserve encoded sRGB color through native Blender Standard output.
The 16-bit source PNG remains in a backup; normal/ORM/FBX are unchanged.
"""
import bpy
import hashlib
import json
import shutil
import struct
from pathlib import Path

import numpy as np

ROOT = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02')
MANIFEST = ROOT / 'exports/export_manifest.json'
manifest = json.loads(MANIFEST.read_text())
BC = Path(manifest['texture_files']['base_color'])
BACKUP = ROOT / 'textures/source_16bit/T_CHALK_PistolHolster01_BaseColor_sRGB16.png'
TEMP = ROOT / 'textures/T_CHALK_PistolHolster01_BaseColor_8bit_verified.png'
BLEND = Path(manifest['export_blend'])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


protected = {k: sha(manifest['texture_files'][k]) for k in ['normal', 'orm']}
protected['fbx'] = sha(manifest['fbx_path'])
source_hash = sha(manifest['source_blend'])
if BC.read_bytes()[24] != 16:
    raise RuntimeError('Expected the original 16-bit base-color PNG')
if BACKUP.exists() or TEMP.exists():
    raise RuntimeError('Depth-conversion outputs already exist; inspect before retrying')
BACKUP.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(BC, BACKUP)
original_hash = sha(BC)
bpy.ops.wm.open_mainfile(filepath=str(BLEND))
source_image = bpy.data.images.load(str(BC), check_existing=False)
source_image.colorspace_settings.name = 'sRGB'
source_pixels = np.empty(4096 * 4096 * 4, dtype=np.float32)
source_image.pixels.foreach_get(source_pixels)
source_pixels = source_pixels.reshape(-1, 4)[:, :3].copy()

# Standard + exposure0 + gamma1 is the sRGB display transfer only. Dithering
# is disabled, so the byte output can be checked against quantized sRGB.
conversion_scene = bpy.data.scenes.new('TEMP / sRGB 8-bit texture output')
conversion_scene.view_settings.view_transform = 'Standard'
conversion_scene.view_settings.look = 'None'
conversion_scene.view_settings.exposure = 0.0
conversion_scene.view_settings.gamma = 1.0
conversion_scene.display_settings.display_device = 'sRGB'
conversion_scene.render.image_settings.file_format = 'PNG'
conversion_scene.render.image_settings.color_mode = 'RGB'
conversion_scene.render.image_settings.color_depth = '8'
conversion_scene.render.dither_intensity = 0.0
source_image.save_render(str(TEMP), scene=conversion_scene)
if TEMP.read_bytes()[24] != 8:
    raise RuntimeError('Native Blender conversion did not produce an 8-bit PNG')
converted = bpy.data.images.load(str(TEMP), check_existing=False)
converted.colorspace_settings.name = 'sRGB'
converted_pixels = np.empty(4096 * 4096 * 4, dtype=np.float32)
converted.pixels.foreach_get(converted_pixels)
converted_pixels = converted_pixels.reshape(-1, 4)[:, :3].copy()


def srgb(linear):
    return np.where(linear <= .0031308, linear * 12.92,
                    1.055 * np.power(np.maximum(linear, 0.0), 1.0 / 2.4) - .055)


# For 8-bit Blender images pixels[] exposes encoded byte samples, while float
# 16-bit PNG pixels[] exposes scene-linear floats. Decode explicitly when needed.
old_encoded = srgb(source_pixels)
new_encoded = srgb(converted_pixels) if converted.is_float else converted_pixels
error = np.abs(new_encoded - old_encoded)
max_error = float(error.max())
if max_error > 1.0 / 255.0 + 2e-6:
    raise RuntimeError('Encoded sRGB colors changed: max error ' + str(max_error))

samples = []
for x, y in [(100, 3650), (300, 3400), (750, 3600), (240, 2250)]:
    index = (4095 - y) * 4096 + x
    encoded = new_encoded[index]
    decoded = np.where(encoded <= .04045, encoded / 12.92,
                        np.power((encoded + .055) / 1.055, 2.4))
    samples.append({'xy': [x, y], 'original_linear': source_pixels[index].tolist(),
                    'converted_encoded_srgb': encoded.tolist(), 'converted_linear': decoded.tolist()})

TEMP.replace(BC)
converted.filepath_raw = str(BC)
for material in bpy.data.materials:
    if not material.use_nodes:
        continue
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image and node.image.name.startswith('T_CHALK_PistolHolster01_BaseColor'):
            node.image = converted
converted.name = 'T_CHALK_PistolHolster01_BaseColor_8bit_sRGB'
converted.pack()
bpy.data.scenes.remove(conversion_scene)
# Remove only the now-unreferenced 16-bit BC image datablocks from this copy.
for image in list(bpy.data.images):
    if image != converted and image.name.startswith('T_CHALK_PistolHolster01_BaseColor') and image.users == 0:
        bpy.data.images.remove(image)
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))

if sha(manifest['source_blend']) != source_hash or sha(BACKUP) != original_hash:
    raise RuntimeError('Protected source or preserved 16-bit copy changed')
for key in ['normal', 'orm']:
    if sha(manifest['texture_files'][key]) != protected[key]:
        raise RuntimeError('Unexpected change to ' + key)
if sha(manifest['fbx_path']) != protected['fbx']:
    raise RuntimeError('Unexpected FBX change')
manifest['texture_sha256']['base_color'] = sha(BC)
manifest['export_blend_sha256'] = sha(BLEND)
manifest['texture_bit_depth'] = {'base_color': 8, 'normal': 16, 'orm': 16}
manifest['base_color_depth_conversion'] = {
    'reason': 'Unreal FTextureSource treats RGBA16 as linear; use 8-bit encoded sRGB base color.',
    'original_16bit_backup': str(BACKUP), 'original_16bit_sha256': original_hash,
    'method': 'Native Blender PNG8 RGB; Standard, exposure0, gamma1, sRGB display, dithering0. No rebake.',
    'max_encoded_srgb_error': max_error,
    'allowed_encoded_srgb_error': 1.0 / 255.0,
    'sample_verification': samples, 'source_normal_orm_fbx_unchanged': True,
}
MANIFEST.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print('BASECOLOR_DEPTH_FIXED ' + json.dumps(manifest['base_color_depth_conversion']), flush=True)
print('BASECOLOR_SHA256 ' + manifest['texture_sha256']['base_color'], flush=True)
