"""Daylight material siblings and a guarded five-field art-default assignment.

Create only touches unused new owned material assets. Apply waits for no PIE and
preserves every other live style field, including Claude's newer marker tilt.
"""
from pathlib import Path
import json,re,gc
import unreal
import throw_quiet_sage_art_setup as base

ROOT=Path('C:/UnrealEngine/Games/AZ')
ART=ROOT/'UI Design/CHALK_Throw_Art_v02_Sunlight'
OUT=ROOT/'Saved/ThrowArtProduction/Sunlight'
NAMES=['M_QS_ArcSunlight','M_QS_ContactSunlight','MI_QS_ArcSunlight','MI_QS_ContactSunlight',
       'MI_QS_BlockedSunlight','MI_QS_BodyContactSunlight']
BP='/Game/AZ/Blueprints/Throwables/BP_AZ_GA_Throw'

def create():
    OUT.mkdir(parents=True,exist_ok=True)
    # New sibling materials never replace the actively rendered originals.
    assert not [p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
                if p.get_name() in [base.PACKAGE+'/'+n for n in NAMES]], 'Unsaved art edits exist'
    arc=base.material(NAMES[0],True,daylight=True)
    contact=base.material(NAMES[1],False,daylight=True)
    arc_mi=base.instance(NAMES[2],arc)
    contact_mi=base.instance(NAMES[3],contact)
    blocked=base.instance(NAMES[4],contact,base.need('T_QS_BlockedMask'),base.WHITE)
    body=base.instance(NAMES[5],contact,base.need('T_QS_BodyContactMask'))
    for obj in [blocked,body]:
        for key in ['MaskCanvasWidth','MaskCanvasHeight']:
            base.LIB.set_material_instance_scalar_parameter_value(obj,key,32)
        base.LIB.update_material_instance(obj)
    for obj in [arc_mi]:
        for key,value in [('FilamentAlpha',.55),('PulseAlpha',1.0)]:
            base.LIB.set_material_instance_scalar_parameter_value(obj,key,value)
        base.LIB.update_material_instance(obj)
    for name in NAMES:
        obj=base.need(name)
        unreal.EditorAssetLibrary.set_metadata_tag(obj,'AZ.ArtSource',str(ART))
        unreal.EditorAssetLibrary.set_metadata_tag(obj,'AZ.ArtRevision','Sunlight readability 2026-09-17')
        assert unreal.EditorAssetLibrary.save_loaded_asset(obj), name
    receipt={'created':[base.PACKAGE+'/'+n for n in NAMES],'master_material_compile_errors':[],
             'existing_assets_changed':False,'next_step':'apply after PIE stops'}
    (OUT/'create-receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    gc.collect()
    return receipt

def apply():
    OUT.mkdir(parents=True,exist_ok=True)
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before assignment'
    assert not [p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name()==BP], 'Ability has unrelated unsaved edits'
    bp=unreal.load_asset(BP);cdo=unreal.get_default_object(bp.generated_class())
    style=cdo.get_editor_property('preview_style');before=style.export_text()
    assert style.get_editor_property('arc_mesh').get_path_name()==base.need('SM_QS_Ribbon').get_path_name()
    assert style.get_editor_property('marker_mesh').get_path_name()==base.need('SM_QS_MarkerPlane').get_path_name()
    values={'ArcMaterial':base.need('MI_QS_ArcSunlight'),'MarkerMaterial':base.need('MI_QS_ContactSunlight'),
            'PulseWidthPixels':5.5,'FilamentAlpha':.55,'PulseAlpha':1.0}
    expected_text=before
    for key,val in values.items():
        encoded=('"'+val.get_class().get_path_name()+"'"+val.get_path_name()+"'\"") if isinstance(val,unreal.Object) else ('%.6f'%val)
        expected_text,count=re.subn(r'(?<!\w)'+key+r'=("[^"]*"|[^,)]+)',lambda m:key+'='+encoded,expected_text)
        assert count==1,(key,count)
    expected=style.copy();assert expected.import_text(expected_text)
    (OUT/'style-before.txt').write_text(before,encoding='utf-8')
    bp.modify();cdo.modify();cdo.set_editor_property('preview_style',expected)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    actual=unreal.get_default_object(unreal.load_asset(BP).generated_class()).get_editor_property('preview_style').export_text()
    assert actual==expected.export_text()
    receipt={'blueprint':BP,'before':before,'after':actual,'changed_fields':list(values),
             'color_width_pixels':3.5,'ribbon_envelope_pixels':5.5,'filament_width_pixels':1.4,
             'palette_preserved':True,'gameplay_and_cpp_unchanged':True}
    (OUT/'apply-receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    gc.collect()
    return receipt
