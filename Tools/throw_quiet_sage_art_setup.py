"""Author owned Quiet Sage art and optionally assign existing ability art fields.

Run main('create') in Unreal Python after GIMP production exports exist; main('verify')
reads saved assets. assign_style() changes four art references and two canvas sizes
only. Material compiles are asset-authoring validation, not PIE/tests.
"""
from pathlib import Path
import json
import re
import gc
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
ART = ROOT / 'UI Design/CHALK_Throw_Art_v01'
OUT = ROOT / 'Saved/ThrowArtProduction'
PACKAGE = '/Game/AZ/Blueprints/Throwables/Art/QuietSageV1'
OWNER = 'Codex.QuietSage.Art.v1'
LIB = unreal.MaterialEditingLibrary
SAGE = unreal.LinearColor(0.462077, 0.577580, 0.473531, 1)
WHITE = unreal.LinearColor(0.854993, 0.822786, 0.745404, 1)


def need(name):
    obj = unreal.load_asset(PACKAGE+'/'+name)
    assert obj, 'Missing asset: '+name
    return obj


def own(obj):
    unreal.EditorAssetLibrary.set_metadata_tag(obj, 'AZ.ArtOwner', OWNER)
    unreal.EditorAssetLibrary.set_metadata_tag(obj, 'AZ.ArtSource', str(ART))
    return obj


def create_asset(name, cls, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE+'/'+name):
        obj=need(name)
        assert unreal.EditorAssetLibrary.get_metadata_tag(obj,'AZ.ArtOwner')==OWNER, 'Unowned asset: '+name
        return obj
    obj = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PACKAGE, cls, factory)
    assert obj, name
    return own(obj)


def texture(name):
    if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE+'/'+name):
        obj=need(name)
        assert unreal.EditorAssetLibrary.get_metadata_tag(obj,'AZ.ArtOwner')==OWNER, 'Unowned texture: '+name
        return obj
    task = unreal.AssetImportTask()
    task.filename = str(ART/'unreal-art'/(name+'.png'))
    assert Path(task.filename).is_file()
    task.destination_path = PACKAGE
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = False
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    obj = own(need(name))
    obj.set_editor_property('srgb', False)
    # Lossless coverage masks: compression artifacts must not paint color into empty channels.
    obj.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    obj.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    obj.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    obj.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    obj.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    obj.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
    obj.set_editor_property('never_stream', True)
    return obj


def node(mat, cls, x, y, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k,v in props.items(): n.set_editor_property(k,v)
    return n


def scalar(mat, name, value, y, cpd=None):
    props = dict(parameter_name=name, default_value=value, group='Quiet Sage')
    if cpd is not None:
        props.update(use_custom_primitive_data=True, primitive_data_index=cpd, group='Renderer / CPD')
    return node(mat, unreal.MaterialExpressionScalarParameter, -1300, y, **props)


def vector(mat,name,value,y):
    return node(mat,unreal.MaterialExpressionVectorParameter,-1300,y,
                parameter_name=name,default_value=value,group='Palette / linear from sRGB')


def wire(src, dest, pin, output=''):
    names=list(LIB.get_material_expression_input_names(dest))
    if pin=='Input' and pin not in names and len(names)==1:
        pin=names[0]
    assert LIB.connect_material_expressions(src, output, dest, pin), ('Connection',pin,output)


def material(name, is_arc, daylight=False):
    mat = create_asset(name, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('disable_depth_test', False)
    mat.set_editor_property('translucency_pass', unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    if is_arc: LIB.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SPLINE_MESH, True)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1600, -300, coordinate_index=0)
    inputs = {'Color':vector(mat,'Color',SAGE, -100)}
    if daylight:
        # #18251D, converted from sRGB. An actual dark edge, not more emissive light.
        inputs['OutlineColor']=vector(mat,'OutlineColor',unreal.LinearColor(.009134,.018500,.012286,1),-250)
        inputs['OutlineAlpha']=scalar(mat,'OutlineAlpha',.8,1940)
    if is_arc:
        inputs['UV'] = uv
        for i,(key,value) in enumerate([('FilamentAlpha',.31),('PulseAlpha',.94),('FilamentWidth',.4),
                                     ('PulseSpacing',40),('PulseDuty',.31),('PulsePhase',0),('Brightness',1),('Opacity',1)]):
            inputs[key]=scalar(mat,key,value,150+i*150)
        inputs['StartDistance']=scalar(mat,'StartDistance',0,1400,0)
        inputs['SegmentLength']=scalar(mat,'SegmentLength',100,1550,1)
        if daylight:
            inputs['StrokeFill']=scalar(mat,'StrokeFill',3.5/5.5,2110)
        code=((ROOT/'UI Design/CHALK_Throw_Art_v02_Sunlight' if daylight else ART)/'sources/quiet_sage_arc.hlsl').read_text(encoding='utf-8')
    else:
        inputs['CenterColor']=vector(mat,'CenterColor',WHITE,50)
        sample=node(mat,unreal.MaterialExpressionTextureObjectParameter if daylight else unreal.MaterialExpressionTextureSampleParameter2D,-1300,-400,
                    parameter_name='MaskTexture',texture=need('T_QS_ContactMasks'),
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        if daylight:
            inputs['MaskTex']=sample
            inputs['UV']=uv
            for i,(key,value) in enumerate([('MaskCanvasWidth',128),('MaskCanvasHeight',64),('OutlinePixels',1)]):
                inputs[key]=scalar(mat,key,value,2150+i*150)
        else:
            wire(uv,sample,'UVs')
            inputs['Masks']=sample
        for i,(key,value) in enumerate([('CornerAlpha',1),('RingAlpha',.85 if daylight else .7),('CenterAlpha',1),('Brightness',1),('Opacity',1)]):
            inputs[key]=scalar(mat,key,value,250+i*150)
        code=((ROOT/'UI Design/CHALK_Throw_Art_v02_Sunlight' if daylight else ART)/'sources/quiet_sage_marker.hlsl').read_text(encoding='utf-8')
    custom_inputs=[]
    for key in inputs:
        ci=unreal.CustomInput()
        ci.set_editor_property('input_name',key)
        custom_inputs.append(ci)
    custom=node(mat,unreal.MaterialExpressionCustom,-650,0,code=code,
                description='Quiet Sage / approved filament and masks',
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                inputs=custom_inputs)
    for key,src in inputs.items(): wire(src,custom,key)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,-300,-40,r=True,g=True,b=True,a=False)
    alpha=node(mat,unreal.MaterialExpressionComponentMask,-300,160,r=False,g=False,b=False,a=True)
    wire(custom,rgb,'Input');wire(custom,alpha,'Input')
    # Exposure compensation stabilizes this world-space UI without disabling depth occlusion.
    eye=node(mat,unreal.MaterialExpressionEyeAdaptationInverse,0,-40)
    pins=list(LIB.get_material_expression_input_names(eye))
    assert len(pins)==2, pins
    wire(rgb,eye,pins[0]);wire(scalar(mat,'ExposureCompensation',1,1740),eye,pins[1])
    assert LIB.connect_material_property(eye,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    errors=list(LIB.recompile_material(mat))
    assert not errors, (name,errors)
    return mat


def instance(name,parent,mask=None,color=None):
    obj=create_asset(name,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(obj,parent)
    if mask: LIB.set_material_instance_texture_parameter_value(obj,'MaskTexture',mask)
    if color: LIB.set_material_instance_vector_parameter_value(obj,'Color',color)
    LIB.update_material_instance(obj)
    return obj


def mesh(name,divisions,mat):
    if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE+'/'+name):
        obj=need(name)
        assert unreal.EditorAssetLibrary.get_metadata_tag(obj,'AZ.ArtOwner')==OWNER, 'Unowned mesh: '+name
        return obj
    dm=unreal.DynamicMesh()
    vertices=[];normals=[];uvs=[];tris=[]
    for i in range(divisions+1):
        for j in range(2):
            vertices.append(unreal.Vector(-50+100*i/divisions,-50+100*j,0))
            normals.append(unreal.Vector(0,0,1))
            uvs.append(unreal.Vector2D(i/divisions,j))
    for i in range(divisions):
        a=2*i;b=a+2
        tris += [unreal.IntVector(a,b,b+1),unreal.IntVector(a,b+1,a+1)]
    buffers=unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices,normals=normals,uv0=uvs,triangles=tris)
    unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dm,buffers)
    opts=unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_nanite=False,enable_collision=False,
               enable_recompute_normals=False,enable_recompute_tangents=True)
    obj,outcome=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,PACKAGE+'/'+name,opts)
    assert obj, str(outcome)
    own(obj);obj.set_material(0,mat)
    unreal.EditorAssetLibrary.set_metadata_tag(obj,'AZ.ArtMeshContract',
             'XY plane; +Z normal; X longitudinal; U along X 0..1; V across Y 0..1; '+str(divisions*2)+' triangles')
    return obj


NAMES=['T_QS_ContactMasks','T_QS_BlockedMask','T_QS_BodyContactMask','M_QS_Arc','M_QS_Contact',
       'MI_QS_Arc','MI_QS_Contact','MI_QS_Blocked','MI_QS_BodyContact','SM_QS_Ribbon','SM_QS_MarkerPlane']


def assign_style():
    """Assign art references/padded canvas size only; preserve all gameplay defaults."""
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before style assignment'
    path='/Game/AZ/Blueprints/Throwables/BP_AZ_GA_Throw'
    assert not [p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name()==path], 'Target has unsaved changes'
    bp=unreal.load_asset(path);cdo=unreal.get_default_object(bp.generated_class())
    style=cdo.get_editor_property('preview_style')
    before=style.export_text()
    assignments={'arc_mesh':need('SM_QS_Ribbon'),'arc_material':need('MI_QS_Arc'),
                 'marker_mesh':need('SM_QS_MarkerPlane'),'marker_material':need('MI_QS_Contact'),
                 'marker_width_pixels':128.0,'marker_height_pixels':64.0}
    # EditDefaultsOnly nested fields reject standalone struct setters. Import an
    # exact six-field T3D replacement, then assign the whole struct to the CDO.
    expected_text=before
    names={'arc_mesh':'ArcMesh','arc_material':'ArcMaterial','marker_mesh':'MarkerMesh',
           'marker_material':'MarkerMaterial','marker_width_pixels':'MarkerWidthPixels',
           'marker_height_pixels':'MarkerHeightPixels'}
    for key,value in assignments.items():
        encoded=('"'+value.get_class().get_path_name()+"'"+value.get_path_name()+"'\"") if isinstance(value,unreal.Object) else ('%.6f'%value)
        expected_text,count=re.subn(r'(?<!\w)'+names[key]+r'=("[^"]*"|[^,)]+)',
                                  lambda m:names[key]+'='+encoded,expected_text)
        assert count==1,(key,count)
    expected=style.copy()
    assert expected.import_text(expected_text)
    (OUT/'style-before.txt').write_text(before,encoding='utf-8')
    bp.modify();cdo.modify()
    cdo.set_editor_property('preview_style',expected)
    assert cdo.get_editor_property('preview_style').export_text()==expected.export_text()
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp), 'Failed to save ability art defaults'
    after=unreal.get_default_object(unreal.load_asset(path).generated_class()).get_editor_property('preview_style')
    assert after.export_text()==expected.export_text()
    result={'blueprint':path,'before':before,'after':after.export_text(),
            'changed_fields':list(assignments),'only_art_defaults_changed':True,
            'next_fresh_play_required':True,'no_cpp_or_montage_edits':True}
    (OUT/'style-assignment-receipt.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result


def main(mode='verify'):
    OUT.mkdir(parents=True,exist_ok=True)
    if mode=='create':
        assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Author after user stops PIE'
        for name in NAMES:
            if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE+'/'+name):
                assert unreal.EditorAssetLibrary.get_metadata_tag(need(name),'AZ.ArtOwner')==OWNER, 'Refuse unowned asset: '+name
        textures=[texture(n) for n in NAMES[:3]]
        arc=material('M_QS_Arc',True);contact=material('M_QS_Contact',False)
        arc_mi=instance('MI_QS_Arc',arc);contact_mi=instance('MI_QS_Contact',contact)
        instance('MI_QS_Blocked',contact,textures[1],WHITE)
        instance('MI_QS_BodyContact',contact,textures[2])
        mesh('SM_QS_Ribbon',32,arc_mi);mesh('SM_QS_MarkerPlane',1,contact_mi)
        for name in NAMES:
            assert unreal.EditorAssetLibrary.save_loaded_asset(need(name)), 'Save failed: '+name
    result={'mode':mode,'assets':[],'gameplay_wiring_changed':False,'source':str(ART)}
    for name in NAMES:
        obj=need(name)
        row={'asset':obj.get_path_name(),'class':obj.get_class().get_name(),
             'owner':unreal.EditorAssetLibrary.get_metadata_tag(obj,'AZ.ArtOwner')}
        assert row['owner']==OWNER
        if isinstance(obj,unreal.Texture2D):
            row.update(size=[obj.blueprint_get_size_x(),obj.blueprint_get_size_y()],srgb=obj.get_editor_property('srgb'),
               compression=str(obj.get_editor_property('compression_settings')),mips=str(obj.get_editor_property('mip_gen_settings')))
            assert row['srgb']==False
        if isinstance(obj,unreal.Material):
            row.update(scalars=[str(n) for n in LIB.get_scalar_parameter_names(obj)],
                       vectors=[str(n) for n in LIB.get_vector_parameter_names(obj)],
                       depth_test=not obj.get_editor_property('disable_depth_test'),
                       spline_usage=LIB.has_material_usage(obj,unreal.MaterialUsage.MATUSAGE_SPLINE_MESH))
        if isinstance(obj,unreal.StaticMesh):
            row.update(bounds=str(obj.get_bounds()),lods=obj.get_num_lods(),
                       mesh_contract=unreal.EditorAssetLibrary.get_metadata_tag(obj,'AZ.ArtMeshContract'))
        result['assets'].append(row)
    (OUT/('art-'+mode+'-receipt.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    gc.collect()
    return result


if __name__=='__main__': print(json.dumps(main(),indent=2))
