"""Author the transparent inspection composite, then assign the owned controller default.
No PIE or gameplay test is started. Existing source materials remain untouched.
"""
from pathlib import Path
from datetime import datetime
import shutil
import unreal


def run():
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not ed.get_game_world(), 'PIE must be stopped'
    folder='/Game/AZ/Blueprints/Menu/HUD'
    path=folder+'/M_AZ_InspectionComposite'
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    placeholder=unreal.load_asset(folder+'/RT_AZ_InspectionTemplate')
    if not placeholder:
        placeholder=tools.create_asset('RT_AZ_InspectionTemplate',folder,unreal.TextureRenderTarget2D,unreal.TextureRenderTargetFactoryNew())
        assert placeholder
        placeholder.set_editor_property('render_target_format',unreal.TextureRenderTargetFormat.RTF_RGBA16F)
        placeholder.set_editor_property('clear_color',unreal.LinearColor(0,0,0,1))
    placeholder.modify()
    placeholder.set_editor_property('srgb',False)  # Material compiler validates the raw SRGB flag, not only the RT format.
    mat=unreal.load_asset(path)
    if not mat:
        mat=tools.create_asset('M_AZ_InspectionComposite',folder,unreal.Material,unreal.MaterialFactoryNew())
        assert mat
        mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        lib=unreal.MaterialEditingLibrary
        tex=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,-600,0)
        tex.set_editor_property('parameter_name','InspectionTexture')
        tex.set_editor_property('texture',placeholder)
        tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        add=lib.create_material_expression(mat,unreal.MaterialExpressionAdd,-350,-70)
        add.set_editor_property('const_b',1.0)
        divide=lib.create_material_expression(mat,unreal.MaterialExpressionDivide,-120,-70)
        opacity=lib.create_material_expression(mat,unreal.MaterialExpressionOneMinus,-350,180)
        assert lib.connect_material_expressions(tex,'RGB',add,'A')
        assert lib.connect_material_expressions(tex,'RGB',divide,'A')
        assert lib.connect_material_expressions(add,'',divide,'B')
        assert lib.connect_material_expressions(tex,'A',opacity,'')
        assert lib.connect_material_property(divide,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
        lib.recompile_material(mat)
    bp=unreal.load_asset('/Game/AZ/Blueprints/Player/BP_AZ_PlayerController')
    assert bp.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Controller has unsaved edits'
    backup=Path('C:/UnrealEngine/Games/AZ/Saved/InteractionAudit')/('context-view-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    shutil.copy2('C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Player/BP_AZ_PlayerController.uasset',backup/'BP_AZ_PlayerController-before.uasset')
    cdo=unreal.get_default_object(bp.generated_class())
    component=cdo.get_editor_property('Inspection')
    bp.modify();cdo.modify();component.modify()
    component.set_editor_property('PreviewMaterial',mat)
    assert unreal.EditorLoadingAndSavingUtils.save_packages([placeholder.get_outermost(),mat.get_outermost(),bp.get_outermost()],False)
    assert component.get_editor_property('PreviewMaterial')==mat
    print('INSPECTION_CONTEXT_VIEW_SAVED; user_playtest_required=True')


run()
