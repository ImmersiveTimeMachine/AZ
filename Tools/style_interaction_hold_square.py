"""Restyle the owned copy of Interaction Essentials' progress widget. No gameplay changes."""
from pathlib import Path
from datetime import datetime
import shutil
import unreal


def run():
    ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not ed.get_game_world(), 'Stop PIE before saving the widget'
    widget_path = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_InteractionHold'
    bp = unreal.load_asset(widget_path)
    assert bp
    assert bp.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Progress widget has unsaved changes'
    backup = Path('C:/UnrealEngine/Games/AZ/Saved/InteractionAudit')/('hold-square-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    shutil.copy2('C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Menu/HUD/WBP_AZ_InteractionHold.uasset', backup/'WBP_AZ_InteractionHold-before.uasset')
    material_path = '/Game/AZ/Blueprints/Menu/HUD/M_AZ_KeyHoldFill'
    material = unreal.load_asset(material_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AZ_KeyHoldFill', '/Game/AZ/Blueprints/Menu/HUD', unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)

    def node(cls, x, y):
        return lib.create_material_expression(material, cls, x, y)

    uv = node(unreal.MaterialExpressionTextureCoordinate, -650, 0)
    axis = node(unreal.MaterialExpressionComponentMask, -450, 0)
    axis.set_editor_property('r', True)
    axis.set_editor_property('g', False)
    percent = node(unreal.MaterialExpressionScalarParameter, -450, 160)
    percent.set_editor_property('parameter_name', 'Percent')
    percent.set_editor_property('default_value', 0.0)
    fill = node(unreal.MaterialExpressionIf, -200, 0)
    on = node(unreal.MaterialExpressionConstant, -450, 310)
    on.set_editor_property('r', .35)
    off = node(unreal.MaterialExpressionConstant, -450, 390)
    off.set_editor_property('r', 0.0)
    tint = node(unreal.MaterialExpressionVectorParameter, -200, -180)
    tint.set_editor_property('parameter_name', 'FillColor')
    tint.set_editor_property('default_value', unreal.LinearColor(.855,.823,.745,1))
    for a, output, b, pin in [(uv,'',axis,''), (axis,'',fill,'A'), (percent,'',fill,'B'),
                              (on,'',fill,'A < B'), (off,'',fill,'A > B'), (off,'',fill,'A == B')]:
        assert lib.connect_material_expressions(a, output, b, pin), pin
    assert lib.connect_material_property(tint, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(fill, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)

    image_path = bp.get_path_name()+':WidgetTree.Circle'
    image = unreal.find_object(None, image_path)
    assert isinstance(image, unreal.Image), 'Pack progress image missing'
    image.modify()
    image.set_brush_from_material(material)
    defaults = unreal.get_default_object(bp.generated_class())
    defaults.set_editor_property('bUseTargetPercent', False)
    defaults.set_editor_property('bUseMarquee', False)
    defaults.set_editor_property('CurrentPercent', 0.0)
    defaults.set_editor_property('bAbsoluteFillMethod', True)
    assert unreal.EditorLoadingAndSavingUtils.save_packages([material.get_outermost(), bp.get_outermost()],False)
    print('SQUARE_HOLD_STYLE_SAVED; original pack unchanged; placement code needs full build')


run()
